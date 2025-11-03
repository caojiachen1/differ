#include "ImageIndexer.h"
#include "SqliteStore.h"
#include "ImageHash.h"
#include <QtWidgets>

ImageIndexer::ImageIndexer(QObject* parent) : QObject(parent) {}

bool ImageIndexer::isImageFile(const QString& path) {
    static const QStringList exts = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tiff"};
    const QString l = QFileInfo(path).suffix().toLower();
    return exts.contains("."+l);
}

void ImageIndexer::startIndex(const QString& folder) {
    if (m_future.isRunning()) return;
    m_future = QtConcurrent::run([this, folder]{ doIndex(folder); });
}

void ImageIndexer::doIndex(const QString& folder) {
    QDir dir(folder);
    if (!dir.exists()) { emit finished(); return; }

    // Open DB under app data dir
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    SqliteStore store;
    if (!store.open(appData + "/index.db")) {
        qWarning() << "Failed to open DB";
        emit finished(); return;
    }

    // Enumerate files
    QStringList files;
    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        if (isImageFile(p)) files.push_back(p);
    }

    const int total = files.size();
    int indexed = 0;
    emit progress(indexed, total);

    // Thumbnail dir
    const QString thumbDir = appData + "/thumbs";
    QDir().mkpath(thumbDir);

    for (const QString& path : files) {
        QFileInfo fi(path);
        ImageEntry e;
        e.path = QDir::toNativeSeparators(fi.absoluteFilePath());
        e.size = fi.size();
        e.mtime = fi.lastModified().toSecsSinceEpoch();

        QImageReader reader(path);
        reader.setAutoTransform(true);
        // Decode at a bounded size to avoid huge memory use
        const int maxDecodeDim = 4096;
        const QSize origSize = reader.size();
        if (origSize.isValid()) {
            QSize tgt = origSize;
            tgt.scale(maxDecodeDim, maxDecodeDim, Qt::KeepAspectRatio);
            reader.setScaledSize(tgt);
        }
        QImage img = reader.read();
        if (!img.isNull()) {
            e.phash = ImageHash::pHash(img);

            // Save thumbnail 256px
            const int maxSide = 256;
            QImage thumb = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const QString thPath = thumbDir + "/" + QString::number(qHash(e.path)) + ".jpg";
            thumb.save(thPath, "JPG", 85);
        }

        store.upsertImage(e);

        ++indexed;
        if (indexed % 10 == 0) emit progress(indexed, total);
    }

    emit progress(total, total);
    emit finished();
}
