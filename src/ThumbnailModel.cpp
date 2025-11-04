#include "ThumbnailModel.h"
#include "ImageHash.h"

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
    // Try thumb file first
    const QString thPath = m_appData + "/thumbs/" + QString::number(qHash(QDir::toNativeSeparators(path))) + ".jpg";
    QPixmap pm;
    if (QFile::exists(thPath)) {
        pm.load(thPath);
    }
    if (pm.isNull()) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        reader.setScaledSize(QSize(256,256));
        QImage img = reader.read();
        if (!img.isNull()) {
            pm = QPixmap::fromImage(img);
        } else {
            pm = QPixmap(64,64);
            pm.fill(Qt::lightGray);
        }
    }
    return QIcon(pm);
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
    quint64 qh = ImageHash::pHash(img);

    // Load all and compute distances
    QList<ResultItem> all;
    const auto entries = m_store->loadAll();
    for (const auto& e : entries) {
        int d = ImageHash::hammingDistance(qh, e.phash);
        all.push_back({e, d});
    }
    std::sort(all.begin(), all.end(), [](const ResultItem& a, const ResultItem& b){ return a.distance < b.distance; });

    // Primary filter by threshold
    QList<ResultItem> within;
    for (const auto& r : all) if (r.distance <= maxHamming) within.push_back(r);

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
