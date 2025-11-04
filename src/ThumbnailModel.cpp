#include "ThumbnailModel.h"
#include "ImageHash.h"
#include <QtGui/QImageReader>
#include <QtConcurrent>

namespace {
static inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static QImage gaussianBlur3x3(const QImage& src) {
    if (src.isNull()) return src;
    QImage in = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage out(in.size(), in.format());
    const int w = in.width(), h = in.height();
    static const int k[3][3] = {{1,2,1},{2,4,2},{1,2,1}};
    for (int y=0;y<h;++y){
        const QRgb* prev = reinterpret_cast<const QRgb*>(in.constScanLine(y>0?y-1:y));
        const QRgb* curr = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        const QRgb* next = reinterpret_cast<const QRgb*>(in.constScanLine(y<h-1?y+1:y));
        QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x=0;x<w;++x){
            int x0=x>0?x-1:x, x2=x<w-1?x+1:x;
            int b=0,g=0,r=0,a=0;
            auto acc=[&](const QRgb* line,int xi,int ky){
                const QRgb p0=line[x0], p1=line[xi], p2=line[x2];
                b+=k[ky][0]*qBlue(p0)+k[ky][1]*qBlue(p1)+k[ky][2]*qBlue(p2);
                g+=k[ky][0]*qGreen(p0)+k[ky][1]*qGreen(p1)+k[ky][2]*qGreen(p2);
                r+=k[ky][0]*qRed(p0)+k[ky][1]*qRed(p1)+k[ky][2]*qRed(p2);
                a+=k[ky][0]*qAlpha(p0)+k[ky][1]*qAlpha(p1)+k[ky][2]*qAlpha(p2);
            }; acc(prev,x,0); acc(curr,x,1); acc(next,x,2);
            dst[x]=qRgba(b/16,g/16,r/16,a/16);
        }
    }
    return out;
}
static QImage unsharpMask(const QImage& src, double amount=0.5, int threshold=1){
    if (src.isNull()||amount<=0.0) return src;
    QImage in=src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage blur=gaussianBlur3x3(in); QImage out(in.size(), in.format());
    const int w=in.width(), h=in.height();
    for (int y=0;y<h;++y){
        const QRgb* s=reinterpret_cast<const QRgb*>(in.constScanLine(y));
        const QRgb* b=reinterpret_cast<const QRgb*>(blur.constScanLine(y));
        QRgb* d=reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x=0;x<w;++x){
            int sr=qRed(s[x]), sg=qGreen(s[x]), sb=qBlue(s[x]), sa=qAlpha(s[x]);
            int br=qRed(b[x]), bg=qGreen(b[x]), bb=qBlue(b[x]);
            int dr=sr-br, dg=sg-bg, db=sb-bb;
            if (std::abs(dr)<threshold) dr=0; if (std::abs(dg)<threshold) dg=0; if (std::abs(db)<threshold) db=0;
            int rr=clamp255(int(sr+amount*dr)); int rg=clamp255(int(sg+amount*dg)); int rb=clamp255(int(sb+amount*db));
            d[x]=qRgba(rr,rg,rb,sa);
        }
    }
    return out;
}
static QImage downscaleHQ(const QImage& src, int maxSide){
    if (src.isNull()) return src;
    QSize target=src.size(); target.scale(maxSide,maxSide,Qt::KeepAspectRatio);
    QImage scaled=src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return unsharpMask(scaled, 0.5, 1);
}
}

ThumbnailModel::ThumbnailModel(QObject* parent) : QAbstractListModel(parent) {
    m_appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_appData);
}

int ThumbnailModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant ThumbnailModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row()<0 || index.row()>=m_items.size()) return {};
    const auto& e = m_items[index.row()];
    if (role == Qt::DisplayRole)
        return QFileInfo(e.path).fileName();
    if (role == Qt::DecorationRole)
        return iconForPath(e.path);
    if (role == PathRole)
        return e.path;
    if (role == IdRole)
        return (qlonglong)e.id;
    if (role == HashRole)
        return (qulonglong)e.phash;
    return {};
}

Qt::ItemFlags ThumbnailModel::flags(const QModelIndex& index) const {
    return QAbstractListModel::flags(index) | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void ThumbnailModel::ensureDb() {
    if (m_store) return;
    m_store = std::make_unique<SqliteStore>();
    m_store->open(m_appData + "/index.db");
}

void ThumbnailModel::loadAll() {
    ensureDb();
    beginResetModel();
    m_items = m_store->loadAll();
    endResetModel();
}

QString ThumbnailModel::pathForIndex(const QModelIndex& idx) const {
    if (!idx.isValid()) return {};
    return m_items[idx.row()].path;
}

QIcon ThumbnailModel::iconForPath(const QString& path) const {
    // Return from memory cache if available
    auto it = m_iconCache.constFind(path);
    if (it != m_iconCache.constEnd()) return it.value();

    const QString base = m_appData + "/thumbs/" + QString::number(qHash(QDir::toNativeSeparators(path)));
    const QString p256 = base + "_256.jpg";
    const QString p384 = base + "_384.jpg";

    // Fast-path: load cached files once and store in memory
    QPixmap pm384, pm256;
    if (QFile::exists(p384)) pm384.load(p384);
    if (QFile::exists(p256)) pm256.load(p256);
    if (!pm384.isNull() || !pm256.isNull()) {
        QIcon icon;
        if (!pm384.isNull()) icon.addPixmap(pm384);
        if (!pm256.isNull()) icon.addPixmap(pm256);
        m_iconCache.insert(path, icon);
        return icon;
    }

    // If no cache on disk and not already generating, kick off a background task
    if (!m_iconInFlight.contains(path)) {
        m_iconInFlight.insert(path);
        // Capture copies for worker
        const QString cPath = path;
        const QString cP256 = p256;
        const QString cP384 = p384;
        ThumbnailModel* self = const_cast<ThumbnailModel*>(this);
        (void)QtConcurrent::run([self, cPath, cP256, cP384]() {
            // Load and generate HQ thumbs
            QImageReader reader(cPath);
            reader.setAutoTransform(true);
            QSize osz = reader.size();
            if (osz.isValid()) { osz.scale(4096,4096,Qt::KeepAspectRatio); reader.setScaledSize(osz); }
            QImage img = reader.read();
            QIcon icon;
            if (!img.isNull()) {
                QImage th384 = downscaleHQ(img, 384);
                QImage th256 = downscaleHQ(img, 256);
                th384.save(cP384, "JPG", 92);
                th256.save(cP256, "JPG", 92);
                icon.addPixmap(QPixmap::fromImage(th384));
                icon.addPixmap(QPixmap::fromImage(th256));
            }
            // Post result back to UI thread
            QMetaObject::invokeMethod(self, [self, cPath, icon]() {
                self->m_iconInFlight.remove(cPath);
                if (!icon.isNull()) {
                    self->m_iconCache.insert(cPath, icon);
                    // Notify views that decoration changed for all rows with this path
                    for (int row = 0; row < self->m_items.size(); ++row) {
                        if (self->m_items[row].path == cPath) {
                            const QModelIndex idx = self->index(row, 0);
                            self->dataChanged(idx, idx, {Qt::DecorationRole});
                        }
                    }
                }
            }, Qt::QueuedConnection);
        });
    }

    // Return a lightweight placeholder immediately
    QPixmap placeholder(64,64); placeholder.fill(Qt::lightGray);
    QIcon ph; ph.addPixmap(placeholder);
    return ph;
}

QList<ThumbnailModel::ResultItem> ThumbnailModel::searchSimilar(const QString& queryImage, int topK, int maxHamming) {
    ensureDb();
    // Load query image consistently with the indexer: honor EXIF orientation and avoid huge decodes
    QImageReader reader(queryImage);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    if (orig.isValid()) {
        QSize tgt = orig;
        tgt.scale(4096, 4096, Qt::KeepAspectRatio);
        reader.setScaledSize(tgt);
    }
    QImage img = reader.read();
    if (img.isNull()) return {};
    quint64 qp = ImageHash::pHash(img);
    quint64 qa = ImageHash::aHash(img);
    quint64 qd = ImageHash::dHash(img);
    const int qw = img.width();
    const int qh = img.height();

    // Load all and compute distances
    QList<ResultItem> all;
    const auto entries = m_store->loadAll();
    all.reserve(entries.size());
    for (const auto& e : entries) {
        int dp = ImageHash::hammingDistance(qp, e.phash);
        int score = dp * 2; // pHash weight 2
        if (e.ahash) score += ImageHash::hammingDistance(qa, e.ahash);
        if (e.dhash) score += ImageHash::hammingDistance(qd, e.dhash);
        // mild aspect ratio penalty
        if (qw > 0 && qh > 0 && e.width > 0 && e.height > 0) {
            double arQ = (double)qw / (double)qh;
            double arE = (double)e.width / (double)e.height;
            double diff = std::abs(arQ - arE);
            score += (int)std::min(8.0, diff * 6.0); // up to +8 penalty
        }
        all.push_back({e, score});
    }
    std::sort(all.begin(), all.end(), [](const ResultItem& a, const ResultItem& b){ return a.distance < b.distance; });

    // Primary filter by pHash threshold for recall control
    QList<ResultItem> within;
    for (const auto& e : entries) {
        int dp = ImageHash::hammingDistance(qp, e.phash);
        if (dp <= maxHamming) {
            // find its score entry
            // linear search is fine for small lists; could optimize if needed
            for (const auto& r : all) {
                if (r.entry.id == e.id) { within.push_back(r); break; }
            }
        }
    }

    // Fallback: if nothing within threshold, return topK closest overall
    QList<ResultItem> out = within.isEmpty() ? all : within;
    if (out.size() > topK) out = out.mid(0, topK);
    return out;
}

void ThumbnailModel::showResults(const QList<ResultItem>& results) {
    beginResetModel();
    m_items.clear();
    for (const auto& r : results) m_items.push_back(r.entry);
    endResetModel();
}
