#include "SqliteStore.h"
#include <QUuid>

SqliteStore::SqliteStore(QObject* parent) : QObject(parent) {}
SqliteStore::~SqliteStore() {
    if (m_db.isOpen()) m_db.close();
    // Properly remove connection tied to this thread/instance
    if (!m_connName.isEmpty()) {
        const QString name = m_connName;
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

bool SqliteStore::open(const QString& dbPath) {
    if (m_db.isOpen()) m_db.close();
    if (!m_connName.isEmpty()) {
        const QString prev = m_connName;
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(prev);
    }
    m_connName = QStringLiteral("differ_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) return false;
    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    return ensureSchema();
}

bool SqliteStore::ensureSchema() {
    QSqlQuery q(m_db);
    return q.exec("CREATE TABLE IF NOT EXISTS images (\n"
                  " id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                  " path TEXT UNIQUE,\n"
                  " mtime INTEGER,\n"
                  " size INTEGER,\n"
                  " phash INTEGER\n"
                  ")");
}

bool SqliteStore::upsertImage(const ImageEntry& e) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO images(path, mtime, size, phash) VALUES(?,?,?,?)\n"
              "ON CONFLICT(path) DO UPDATE SET mtime=excluded.mtime, size=excluded.size, phash=excluded.phash");
    q.addBindValue(e.path);
    q.addBindValue(e.mtime);
    q.addBindValue(e.size);
    q.addBindValue((qlonglong)e.phash);
    return q.exec();
}

bool SqliteStore::removeMissingPaths(const QStringList& existingPaths) {
    // Remove DB rows whose paths are not in existingPaths
    // For simplicity, not implemented now. Placeholder that returns true.
    Q_UNUSED(existingPaths);
    return true;
}

QList<ImageEntry> SqliteStore::loadAll() {
    QList<ImageEntry> res;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, path, mtime, size, phash FROM images ORDER BY id DESC")) return res;
    while (q.next()) {
        ImageEntry e;
        e.id = q.value(0).toLongLong();
        e.path = q.value(1).toString();
        e.mtime = q.value(2).toLongLong();
        e.size = q.value(3).toLongLong();
        e.phash = q.value(4).toULongLong();
        res.push_back(e);
    }
    return res;
}

QList<ImageEntry> SqliteStore::loadByIds(const QList<qint64>& ids) {
    QList<ImageEntry> res;
    if (ids.isEmpty()) return res;
    QString inClause;
    for (int i=0;i<ids.size();++i) {
        if (i) inClause += ",";
        inClause += QString::number(ids[i]);
    }
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, path, mtime, size, phash FROM images WHERE id IN (" + inClause + ")")) return res;
    while (q.next()) {
        ImageEntry e;
        e.id = q.value(0).toLongLong();
        e.path = q.value(1).toString();
        e.mtime = q.value(2).toLongLong();
        e.size = q.value(3).toLongLong();
        e.phash = q.value(4).toULongLong();
        res.push_back(e);
    }
    return res;
}

QList<ImageEntry> SqliteStore::queryAllBasic() {
    return loadAll();
}
