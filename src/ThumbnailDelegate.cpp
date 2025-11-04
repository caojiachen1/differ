#include "ThumbnailDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QVector>

namespace {
constexpr int kSideMargin = 8;
constexpr int kTopMargin = 8;
constexpr int kBottomMargin = 8;
constexpr int kImageTextSpacing = 6;
constexpr int kMaxTextLines = 2;
constexpr qreal kHighlightRadius = 8.0;
constexpr int kHighlightPenWidth = 2;

QPixmap requestPixmap(const QIcon& icon, const QSize& desiredSize, const QWidget* widget, const QStyleOptionViewItem& opt) {
    if (icon.isNull()) return {};
    QSize size = desiredSize;
    if (!size.isValid() || size.isEmpty()) {
        size = QSize(128, 128);
    }
    qreal deviceRatio = widget ? widget->devicePixelRatioF() : qApp->devicePixelRatio();
    QSize deviceSize = QSize(int(size.width() * deviceRatio), int(size.height() * deviceRatio));
    QIcon::Mode mode = opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
    QIcon::State state = opt.state.testFlag(QStyle::State_Open) ? QIcon::On : QIcon::Off;
    QPixmap pix = icon.pixmap(deviceSize, mode, state);
    if (!pix.isNull()) {
        pix.setDevicePixelRatio(deviceRatio);
    }
    return pix;
}
}

QSize ThumbnailDelegate::cellSizeForIcon(const QSize& iconSize, const QFontMetrics& fm, bool hasText) {
    QSize icon = iconSize.isValid() && !iconSize.isEmpty() ? iconSize : QSize(128, 128);
    int textBlock = 0;
    if (hasText) {
        textBlock = fm.lineSpacing() * kMaxTextLines + kImageTextSpacing;
    }
    const int width = icon.width() + kSideMargin * 2;
    const int height = kTopMargin + icon.height() + textBlock + kBottomMargin;
    return QSize(width, height);
}

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QString text = index.data(Qt::DisplayRole).toString();
    QSize iconSize = option.decorationSize;
    if (!iconSize.isValid() || iconSize.isEmpty()) {
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            iconSize = icon.actualSize(QSize(256, 256));
        }
    }
    QFontMetrics fm(option.font);
    return cellSizeForIcon(iconSize, fm, !text.isEmpty());
}

void ThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QWidget* widget = opt.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Draw the item's base (selection background, etc.)
    QStyleOptionViewItem panelOpt(opt);
    panelOpt.text.clear();
    panelOpt.icon = QIcon();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &panelOpt, painter, widget);

    const QRect contentRect = opt.rect.adjusted(kSideMargin, kTopMargin, -kSideMargin, -kBottomMargin);
    const QString text = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm(opt.font);
    const int reservedTextHeight = text.isEmpty() ? 0 : fm.lineSpacing() * kMaxTextLines;

    int availableImageHeight = contentRect.height() - reservedTextHeight;
    if (!text.isEmpty()) {
        availableImageHeight -= kImageTextSpacing;
    }
    if (availableImageHeight < 0) {
        availableImageHeight = 0;
    }

    QRect imageArea(contentRect.left(), contentRect.top(), contentRect.width(), availableImageHeight);
    QRect textArea(contentRect.left(), imageArea.bottom() + (text.isEmpty() ? 0 : kImageTextSpacing),
                   contentRect.width(), contentRect.bottom() - (imageArea.bottom() + (text.isEmpty() ? 0 : kImageTextSpacing)));

    // Fetch pixmap to render
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QPixmap pix = requestPixmap(icon, opt.decorationSize, widget, opt);

    QRect imageRect = imageArea;
    if (!pix.isNull() && imageArea.height() > 0 && imageArea.width() > 0) {
        QSize target = pix.size() / pix.devicePixelRatio();
        target.scale(imageArea.size(), Qt::KeepAspectRatio);
        imageRect = QStyle::alignedRect(opt.direction, Qt::AlignCenter, target, imageArea);
        painter->drawPixmap(imageRect, pix.scaled(imageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (imageArea.height() > 0 && imageArea.width() > 0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(opt.palette.mid());
        painter->drawRoundedRect(imageArea, kHighlightRadius, kHighlightRadius);
        imageRect = imageArea;
    }

    const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);
    const bool selected = opt.state.testFlag(QStyle::State_Selected);
    if ((hovered || selected) && imageRect.isValid() && !imageRect.isEmpty()) {
        // TODO: Hover highlight still renders smaller than the pixmap on some systems; verify device pixel ratio handling.
        QColor border = selected ? opt.palette.highlight().color() : QColor(30, 144, 255);
        QColor fill = border;
        fill.setAlpha(selected ? 70 : 45);
        painter->setPen(QPen(border, kHighlightPenWidth));
        painter->setBrush(fill);
        painter->drawRoundedRect(imageRect.adjusted(-1, -1, 1, 1), kHighlightRadius, kHighlightRadius);
    }

    if (!text.isEmpty() && textArea.height() > 0) {
        QTextOption textOpt(Qt::AlignHCenter | Qt::AlignTop);
        textOpt.setWrapMode(QTextOption::WordWrap);
        painter->setPen(opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
        painter->setFont(opt.font);

        QTextLayout layout(text, opt.font);
        layout.setTextOption(textOpt);
        layout.beginLayout();
        QVector<QTextLine> lines;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(textArea.width());
            lines.append(line);
        }
        layout.endLayout();

        if (!lines.isEmpty()) {
            const int linesToDraw = qMin(kMaxTextLines, lines.size());
            qreal y = textArea.top();
            for (int i = 0; i < linesToDraw && y <= textArea.bottom(); ++i) {
                const QTextLine& line = lines.at(i);
                qreal height = line.height();
                QRectF lineRect(textArea.left(), y, textArea.width(), height);
                QString segment = text.mid(line.textStart(), line.textLength());
                if (i == linesToDraw - 1 && lines.size() > linesToDraw) {
                    segment = fm.elidedText(text.mid(line.textStart()), Qt::ElideMiddle, textArea.width());
                } else {
                    segment = segment.trimmed();
                }
                painter->drawText(lineRect, Qt::AlignHCenter | Qt::AlignTop, segment);
                y += height;
            }
        }
    }

    if (opt.state.testFlag(QStyle::State_HasFocus)) {
        QStyleOptionFocusRect focusOpt;
        focusOpt.QStyleOption::operator=(opt);
        focusOpt.rect = opt.rect.adjusted(2, 2, -2, -2);
        focusOpt.backgroundColor = opt.palette.color(QPalette::Highlight);
        style->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, painter, widget);
    }

    painter->restore();
}
