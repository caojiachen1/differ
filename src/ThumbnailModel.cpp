#include "ThumbnailModel.h"
#include "ImageHash.h"
#include <QtGui/QImageReader>
#include <QtConcurrent>
#include <QMutex>
#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#endif

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

QList<ThumbnailModel::ResultItem> ThumbnailModel::searchSimilar(const QString& queryImage, int topK, int /*maxHamming*/) {
    ensureDb();
#ifndef HAVE_OPENCV
    Q_UNUSED(queryImage);
    Q_UNUSED(topK);
    // OpenCV not available, return empty to trigger UI hint
    return {};
#else
    // Load query image (respect EXIF), convert to BGR for OpenCV
    QImageReader qreader(queryImage);
    qreader.setAutoTransform(true);
    QSize qsz = qreader.size();
    if (qsz.isValid()) { qsz.scale(2048, 2048, Qt::KeepAspectRatio); qreader.setScaledSize(qsz); }
    QImage qimg = qreader.read();
    if (qimg.isNull()) return {};
    QImage qbgr = qimg.convertToFormat(QImage::Format_BGR888);
    cv::Mat qMat(qbgr.height(), qbgr.width(), CV_8UC3, const_cast<uchar*>(qbgr.constBits()), qbgr.bytesPerLine());
    qMat = qMat.clone();

    // Query descriptors and histogram - reduce ORB features to 300 for speed
    auto orb = cv::ORB::create(300);
    std::vector<cv::KeyPoint> qkps; cv::Mat qdesc;
    orb->detectAndCompute(qMat, cv::noArray(), qkps, qdesc);
    
    // Calculate query histogram with reduced bins for speed
    cv::Mat qhsv; cv::cvtColor(qMat, qhsv, cv::COLOR_BGR2HSV);
    int hbins=16, sbins=16; int histSize[] = {hbins, sbins};
    float hranges[] = {0,180}; float sranges[] = {0,256}; const float* ranges[] = {hranges, sranges};
    int channels[] = {0,1}; cv::Mat qhist;
    cv::calcHist(&qhsv, 1, channels, cv::Mat(), qhist, 2, histSize, ranges, true, false);
    cv::normalize(qhist, qhist, 1, 0, cv::NORM_L1);

    // Iterate all entries and compute similarity in parallel
    const auto entries = m_store->loadAll();
    struct Pair { ImageEntry e; double sim; };
    std::vector<Pair> pairs; pairs.resize(entries.size());

    auto loadCandidate = [this](const QString& path)->cv::Mat{
        // Prefer cached 256 thumb (faster to load than 384)
        const QString base = m_appData + "/thumbs/" + QString::number(qHash(QDir::toNativeSeparators(path)));
        const QString p256 = base + "_256.jpg";
        const QString p384 = base + "_384.jpg";
        QString use = QFile::exists(p256) ? p256 : (QFile::exists(p384) ? p384 : path);
        QImageReader r(use); r.setAutoTransform(true);
        QSize osz = r.size(); 
        // Further reduce size for faster processing (384 is enough)
        if (osz.isValid()) { osz.scale(384, 384, Qt::KeepAspectRatio); r.setScaledSize(osz); }
        QImage img = r.read();
        if (img.isNull()) return cv::Mat();
        QImage bgr = img.convertToFormat(QImage::Format_BGR888);
        cv::Mat m(bgr.height(), bgr.width(), CV_8UC3, const_cast<uchar*>(bgr.constBits()), bgr.bytesPerLine());
        return m.clone();
    };

    // Parallel computation using QtConcurrent
    QtConcurrent::blockingMap(pairs, [&](Pair& pair) {
        int idx = &pair - pairs.data();
        const auto& e = entries[idx];
        pair.e = e;
        
        cv::Mat cMat = loadCandidate(e.path);
        if (cMat.empty()) { pair.sim = 0.0; return; }
        
        // ORB with reduced features for speed
        auto orbLocal = cv::ORB::create(300);
        std::vector<cv::KeyPoint> ckps; cv::Mat cdesc;
        orbLocal->detectAndCompute(cMat, cv::noArray(), ckps, cdesc);
        double orbScore = 0.0;
        if (!qdesc.empty() && !cdesc.empty()) {
            cv::BFMatcher matcher(cv::NORM_HAMMING, false);
            std::vector<std::vector<cv::DMatch>> knn;
            matcher.knnMatch(qdesc, cdesc, knn, 2);
            int good=0; for (auto& v: knn){ if (v.size()==2 && v[0].distance < 0.75*v[1].distance) ++good; }
            orbScore = (qkps.empty()?0.0: (double)good / (double)qkps.size());
        }
        
        // HSV hist - use smaller bins for faster computation
        cv::Mat chsv; cv::cvtColor(cMat, chsv, cv::COLOR_BGR2HSV);
        int hb=16, sb=16; int hs[] = {hb, sb};
        float hr[] = {0,180}; float sr[] = {0,256}; const float* r[] = {hr, sr};
        int ch[] = {0,1}; cv::Mat chist;
        cv::calcHist(&chsv, 1, ch, cv::Mat(), chist, 2, hs, r, true, false);
        cv::normalize(chist, chist, 1, 0, cv::NORM_L1);
        double histCorr = cv::compareHist(qhist, chist, cv::HISTCMP_CORREL);
        
        pair.sim = std::max(0.0, std::min(1.0, 0.7*orbScore + 0.3*((histCorr+1.0)/2.0)));
    });

    std::stable_sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b){ return a.sim > b.sim; });

    // Build results
    QList<ResultItem> out;
    out.reserve(std::min<int>(topK, pairs.size()));
    for (int i = 0; i < pairs.size() && i < topK; ++i) {
        // Encode similarity as inverse distance (0..1000)
        int dist = (int)std::lround((1.0 - pairs[i].sim) * 1000.0);
        out.push_back({pairs[i].e, dist});
    }

    // Ensure exact same image first if present
    if (!out.isEmpty()) {
        int selfIdx = -1;
        for (int i = 0; i < out.size(); ++i) {
            if (QDir::toNativeSeparators(out[i].entry.path) == QDir::toNativeSeparators(queryImage)) { selfIdx = i; break; }
        }
        if (selfIdx > 0) std::swap(out[0], out[selfIdx]);
    }
    return out;
#endif
}

void ThumbnailModel::showResults(const QList<ResultItem>& results) {
    beginResetModel();
    m_items.clear();
    for (const auto& r : results) m_items.push_back(r.entry);
    endResetModel();
}

int ThumbnailModel::removePaths(const QStringList& paths) {
    if (paths.isEmpty()) return 0;
    ensureDb();
    int removed = 0;
    for (const QString& p : paths) {
        if (m_store->removeByPath(QDir::toNativeSeparators(p))) {
            ++removed;
            // purge memory icon cache
            m_iconCache.remove(p);
            m_iconInFlight.remove(p);
        }
    }
    // Reload to reflect removals
    loadAll();
    return removed;
}
