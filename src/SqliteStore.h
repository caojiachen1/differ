#pragma once
#include <QtCore>
#include <QtSql>

struct ImageEntry {
    qint64 id{0};
    QString path;
    qint64 mtime{0};
    qint64 size{0};
    quint64 phash{0};
};

class SqliteStore : public QObject {
    Q_OBJECT
public:
    explicit SqliteStore(QObject* parent=nullptr);
    ~SqliteStore() override;

    bool open(const QString& dbPath);
    bool ensureSchema();

    bool upsertImage(const ImageEntry& e);
    bool removeMissingPaths(const QStringList& existingPaths);
    QList<ImageEntry> loadAll();
    QList<ImageEntry> loadByIds(const QList<qint64>& ids);

    QList<ImageEntry> queryAllBasic();

private:
    QSqlDatabase m_db;
    QString m_connName;
};
