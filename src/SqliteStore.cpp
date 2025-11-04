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
    bool ok = q.exec("CREATE TABLE IF NOT EXISTS images (\n"
                  " id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                  " path TEXT UNIQUE,\n"
                  " mtime INTEGER,\n"
                  " size INTEGER,\n"
                  " phash INTEGER,\n"
                  " ahash INTEGER DEFAULT 0,\n"
                  " dhash INTEGER DEFAULT 0,\n"
                  " width INTEGER DEFAULT 0,\n"
                  " height INTEGER DEFAULT 0\n"
                  ")");
    if (!ok) return false;
    // Ensure new columns exist for older DBs
    auto hasCol = [this](const QString& name){
        QSqlQuery qi(m_db);
        qi.exec("PRAGMA table_info(images)");
        while (qi.next()) {
            if (qi.value(1).toString().compare(name, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    };
    struct { const char* name; const char* type; const char* defv; } cols[] = {
        {"ahash", "INTEGER", "0"},
        {"dhash", "INTEGER", "0"},
        {"width", "INTEGER", "0"},
        {"height", "INTEGER", "0"}
    };
    for (auto& c : cols) {
        if (!hasCol(c.name)) {
            QSqlQuery qa(m_db);
            qa.exec(QString("ALTER TABLE images ADD COLUMN %1 %2 DEFAULT %3")
                .arg(c.name).arg(c.type).arg(c.defv));
        }
    }
    return true;
}

bool SqliteStore::upsertImage(const ImageEntry& e) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO images(path, mtime, size, phash, ahash, dhash, width, height) VALUES(?,?,?,?,?,?,?,?)\n"
              "ON CONFLICT(path) DO UPDATE SET mtime=excluded.mtime, size=excluded.size, phash=excluded.phash, ahash=excluded.ahash, dhash=excluded.dhash, width=excluded.width, height=excluded.height");
    q.addBindValue(e.path);
    q.addBindValue(e.mtime);
    q.addBindValue(e.size);
    q.addBindValue((qlonglong)e.phash);
    q.addBindValue((qlonglong)e.ahash);
    q.addBindValue((qlonglong)e.dhash);
    q.addBindValue(e.width);
    q.addBindValue(e.height);
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
    if (!q.exec("SELECT id, path, mtime, size, phash, ahash, dhash, width, height FROM images ORDER BY id DESC")) return res;
    while (q.next()) {
        ImageEntry e;
        e.id = q.value(0).toLongLong();
        e.path = q.value(1).toString();
        e.mtime = q.value(2).toLongLong();
        e.size = q.value(3).toLongLong();
        e.phash = q.value(4).toULongLong();
        e.ahash = q.value(5).toULongLong();
        e.dhash = q.value(6).toULongLong();
        e.width = q.value(7).toInt();
        e.height = q.value(8).toInt();
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
    if (!q.exec("SELECT id, path, mtime, size, phash, ahash, dhash, width, height FROM images WHERE id IN (" + inClause + ")")) return res;
    while (q.next()) {
        ImageEntry e;
        e.id = q.value(0).toLongLong();
        e.path = q.value(1).toString();
        e.mtime = q.value(2).toLongLong();
        e.size = q.value(3).toLongLong();
        e.phash = q.value(4).toULongLong();
        e.ahash = q.value(5).toULongLong();
        e.dhash = q.value(6).toULongLong();
        e.width = q.value(7).toInt();
        e.height = q.value(8).toInt();
        res.push_back(e);
    }
    return res;
}

QList<ImageEntry> SqliteStore::queryAllBasic() {
    return loadAll();
}
