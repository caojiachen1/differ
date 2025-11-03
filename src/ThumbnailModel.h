#pragma once
#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include "SqliteStore.h"

class ThumbnailModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { PathRole = Qt::UserRole + 1, IdRole, HashRole };

    explicit ThumbnailModel(QObject* parent=nullptr);

    int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void loadAll();
    QString pathForIndex(const QModelIndex& idx) const;

    struct ResultItem { ImageEntry entry; int distance; };
    QList<ResultItem> searchSimilar(const QString& queryImage, int topK, int maxHamming);
    void showResults(const QList<ResultItem>& results);

private:
    void ensureDb();
    QIcon iconForPath(const QString& path) const;

    QList<ImageEntry> m_items;
    std::unique_ptr<SqliteStore> m_store;
    QString m_appData;
};