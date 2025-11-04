#pragma once
#include <QFontMetrics>
#include <QStyledItemDelegate>
#include <QSize>

class ThumbnailDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ThumbnailDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    static QSize cellSizeForIcon(const QSize& iconSize, const QFontMetrics& fm, bool hasText = true);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
