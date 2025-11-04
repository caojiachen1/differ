#include "ImageIndexer.h"
#include "SqliteStore.h"
#include "ImageHash.h"
#include <QtWidgets>

namespace {
static inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// Simple and fast 3x3 Gaussian blur
static QImage gaussianBlur3x3(const QImage& src) {
    if (src.isNull()) return src;
    QImage in = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage out(in.size(), in.format());
    const int w = in.width();
    const int h = in.height();
    static const int k[3][3] = {{1,2,1},{2,4,2},{1,2,1}}; // sum=16
    for (int y = 0; y < h; ++y) {
        const QRgb* prev = reinterpret_cast<const QRgb*>(in.constScanLine(y > 0 ? y-1 : y));
        const QRgb* curr = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        const QRgb* next = reinterpret_cast<const QRgb*>(in.constScanLine(y < h-1 ? y+1 : y));
        QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int x0 = x > 0 ? x-1 : x;
            int x2 = x < w-1 ? x+1 : x;
            int b = 0, g = 0, r = 0, a = 0;
            auto acc = [&](const QRgb* line, int xi, int ky){
                const QRgb p0 = line[x0];
                const QRgb p1 = line[xi];
                const QRgb p2 = line[x2];
                b += k[ky][0]*qBlue(p0) + k[ky][1]*qBlue(p1) + k[ky][2]*qBlue(p2);
                g += k[ky][0]*qGreen(p0) + k[ky][1]*qGreen(p1) + k[ky][2]*qGreen(p2);
                r += k[ky][0]*qRed(p0) + k[ky][1]*qRed(p1) + k[ky][2]*qRed(p2);
                a += k[ky][0]*qAlpha(p0) + k[ky][1]*qAlpha(p1) + k[ky][2]*qAlpha(p2);
            };
            acc(prev, x, 0);
            acc(curr, x, 1);
            acc(next, x, 2);
            dst[x] = qRgba(b/16, g/16, r/16, a/16);
        }
    }
    return out;
}

// Unsharp mask to boost perceived sharpness after downscaling
static QImage unsharpMask(const QImage& src, double amount = 0.6, int threshold = 2) {
    if (src.isNull() || amount <= 0.0) return src;
    QImage in = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage blur = gaussianBlur3x3(in);
    QImage out(in.size(), in.format());
    const int w = in.width();
    const int h = in.height();
    for (int y = 0; y < h; ++y) {
        const QRgb* s = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        const QRgb* b = reinterpret_cast<const QRgb*>(blur.constScanLine(y));
        QRgb* d = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int sr = qRed(s[x]), sg = qGreen(s[x]), sb = qBlue(s[x]), sa = qAlpha(s[x]);
            int br = qRed(b[x]), bg = qGreen(b[x]), bb = qBlue(b[x]);
            int dr = sr - br, dg = sg - bg, db = sb - bb;
            if (std::abs(dr) < threshold) dr = 0;
            if (std::abs(dg) < threshold) dg = 0;
            if (std::abs(db) < threshold) db = 0;
            int rr = clamp255(int(sr + amount * dr));
            int rg = clamp255(int(sg + amount * dg));
            int rb = clamp255(int(sb + amount * db));
            d[x] = qRgba(rr, rg, rb, sa);
        }
    }
    return out;
}

static QImage downscaleHQ(const QImage& src, int maxSide) {
    if (src.isNull()) return src;
    QSize target = src.size();
    target.scale(maxSide, maxSide, Qt::KeepAspectRatio);
    QImage scaled = src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return unsharpMask(scaled, 0.5, 1);
}
}

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
            e.width = img.width();
            e.height = img.height();
            e.phash = ImageHash::pHash(img);
            e.ahash = ImageHash::aHash(img);
            e.dhash = ImageHash::dHash(img);

            // Save thumbnails at 256 and 384 for better clarity
            const QString base = thumbDir + "/" + QString::number(qHash(e.path));
            QImage th256 = downscaleHQ(img, 256);
            QImage th384 = downscaleHQ(img, 384);
            th256.save(base + "_256.jpg", "JPG", 92);
            th384.save(base + "_384.jpg", "JPG", 92);
        }

        store.upsertImage(e);

        ++indexed;
        if (indexed % 10 == 0) emit progress(indexed, total);
    }

    emit progress(total, total);
    emit finished();
}
