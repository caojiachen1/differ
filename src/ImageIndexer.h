#pragma once
#include <QtCore>
#include <QtConcurrent>

class SqliteStore;

class ImageIndexer : public QObject {
    Q_OBJECT
public:
    explicit ImageIndexer(QObject* parent=nullptr);

    void startIndex(const QString& folder);

signals:
    void progress(int indexed, int total);
    void finished();

private:
    void doIndex(const QString& folder);
    static bool isImageFile(const QString& path);

    QFuture<void> m_future;
};
