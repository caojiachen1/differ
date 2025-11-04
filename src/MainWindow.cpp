// Updated file with asymmetric spacing for thumbnails and preview layout margins
#include "MainWindow.h"
#include "ThumbnailModel.h"
#include "ThumbnailDelegate.h"
#include "ImageIndexer.h"

#include <QtWidgets>

static QString humanSize(qint64 bytes) {
    static const char* suffixes[] = {"B","KB","MB","GB","TB"};
    double count = (double)bytes;
    int i=0; while (count >= 1024.0 && i < 4) { count/=1024.0; ++i; }
    return QString::number(count, 'f', count>=10?1:2) + " " + suffixes[i];
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();

    // Create workers and model before wiring signals
    m_indexer = new ImageIndexer(this);
    m_model = new ThumbnailModel(this);
    m_listView->setModel(m_model);

    setupConnections();

    loadAllFromDb();
    loadSettings();
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi() {
    setWindowTitle("Differ - 相似图片查找器");

    // Central list view
    m_listView = new QListView(this);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setUniformItemSizes(true);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setWordWrap(true);
    // 适度拉开图片之间距离
    m_listView->setSpacing(6);
    m_listView->setTextElideMode(Qt::ElideMiddle);
    // 启用 hover 探测，并安装自定义委托，让悬停框覆盖整格
    m_listView->setMouseTracking(true);
    m_listView->viewport()->setAttribute(Qt::WA_Hover, true);
    m_listView->setItemDelegate(new ThumbnailDelegate(m_listView));
    setCentralWidget(m_listView);

    // Left dock
    m_leftDock = new QDockWidget("索引", this);
    auto left = new QWidget(this);
    auto leftLay = new QFormLayout(left);
    m_folderEdit = new QLineEdit(left);
    m_browseBtn = new QPushButton("选择文件夹", left);
    m_indexBtn = new QPushButton("开始索引", left);

    auto folderRow = new QHBoxLayout();
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(m_browseBtn);

    m_thumbSizeSlider = new QSlider(Qt::Horizontal, left);
    m_thumbSizeSlider->setRange(64, 384);
    m_thumbSizeSlider->setValue(160);
    m_thumbSizeLabel = new QLabel("缩略图: 160px", left);

    leftLay->addRow("目录", new QWidget(left));
    leftLay->addRow(folderRow);
    leftLay->addRow(m_indexBtn);
    leftLay->addRow(m_thumbSizeLabel);
    leftLay->addRow(m_thumbSizeSlider);

    left->setLayout(leftLay);
    m_leftDock->setWidget(left);
    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);

    // Right dock
    m_rightDock = new QDockWidget("预览", this);
    auto right = new QWidget(this);
    auto rightLay = new QVBoxLayout(right);
    // 上下增加留白，左右更紧凑
    rightLay->setContentsMargins(4, 12, 4, 12);
    m_previewLabel = new QLabel(right);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setMinimumSize(200, 200);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setAutoFillBackground(true);
    m_metaLabel = new QLabel(right);
    m_metaLabel->setWordWrap(true);
    m_queryBtn = new QPushButton("以所选/图片为查询，查找相似", right);

    rightLay->addWidget(m_previewLabel, 1);
    rightLay->addWidget(m_metaLabel);
    rightLay->addWidget(m_queryBtn);

    right->setLayout(rightLay);
    m_rightDock->setWidget(right);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);

    // Toolbar
    auto tb = addToolBar("工具");
    m_openQueryAction = tb->addAction("打开查询图片");

    tb->addSeparator();
    tb->addWidget(new QLabel("TopK:"));
    m_topKSpin = new QSpinBox(this);
    m_topKSpin->setRange(1, 500);
    m_topKSpin->setValue(50);
    tb->addWidget(m_topKSpin);

    tb->addSeparator();
    tb->addWidget(new QLabel("最大汉明距离:"));
    m_hammingSlider = new QSlider(Qt::Horizontal, this);
    m_hammingSlider->setRange(0, 64);
    m_hammingSlider->setValue(16);
    tb->addWidget(m_hammingSlider);
    m_hammingValue = new QLabel("16", this);
    tb->addWidget(m_hammingValue);

    // Status bar
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    statusBar()->addPermanentWidget(m_progress, 1);

    // 初始化一次网格尺寸：为图片与下方文件名预留空间
    {
        const int v = m_thumbSizeSlider->value();
        const QSize iconSize(v, v);
        QFontMetrics fm(m_listView->font());
        m_listView->setIconSize(iconSize);
        m_listView->setGridSize(ThumbnailDelegate::cellSizeForIcon(iconSize, fm));
    }
}

void MainWindow::setupConnections() {
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(m_indexBtn, &QPushButton::clicked, [this]{ startIndexing(m_folderEdit->text()); });
    connect(m_thumbSizeSlider, &QSlider::valueChanged, [this](int v){
        m_thumbSizeLabel->setText(QString("缩略图: %1px").arg(v));
        const QSize iconSize(v, v);
        QFontMetrics fm(m_listView->font());
        m_listView->setIconSize(iconSize);
        m_listView->setGridSize(ThumbnailDelegate::cellSizeForIcon(iconSize, fm));
        m_listView->doItemsLayout();
        m_listView->viewport()->update();
    });
    connect(m_openQueryAction, &QAction::triggered, this, &MainWindow::openQueryImage);
    connect(m_queryBtn, &QPushButton::clicked, this, &MainWindow::findSimilar);
    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_hammingSlider, &QSlider::valueChanged, [this](int v){ m_hammingValue->setText(QString::number(v)); });

    // Indexer signals
    connect(m_indexer, &ImageIndexer::progress, this, &MainWindow::onIndexingProgress);
    connect(m_indexer, &ImageIndexer::finished, this, &MainWindow::onIndexingFinished);
}

void MainWindow::chooseFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "选择图片目录");
    if (!dir.isEmpty()) {
        m_folderEdit->setText(dir);
    }
}

void MainWindow::startIndexing(const QString& folder) {
    if (folder.isEmpty() || !QDir(folder).exists()) {
        QMessageBox::warning(this, "提示", "请选择有效的目录");
        return;
    }
    m_progress->setValue(0);
    m_indexBtn->setEnabled(false);
    m_indexer->startIndex(folder);
}

void MainWindow::onIndexingProgress(int indexed, int total) {
    if (total > 0) {
        int pct = int((indexed * 100.0) / total);
        m_progress->setValue(pct);
        statusBar()->showMessage(QString("已索引 %1/%2").arg(indexed).arg(total));
    }
}

void MainWindow::onIndexingFinished() {
    m_indexBtn->setEnabled(true);
    m_progress->setValue(100);
    statusBar()->showMessage("索引完成", 5000);
    loadAllFromDb();
}

void MainWindow::openQueryImage() {
    QString fn = QFileDialog::getOpenFileName(this, "选择查询图片", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff)");
    if (fn.isEmpty()) return;

    QImageReader reader(fn);
    reader.setAutoTransform(true);
    QSize pv = m_previewLabel->size();
    if (pv.width() > 0 && pv.height() > 0) {
        QSize tgt = reader.size();
        if (tgt.isValid()) {
            tgt.scale(qMax(512, pv.width()), qMax(512, pv.height()), Qt::KeepAspectRatio);
            reader.setScaledSize(tgt);
        }
    }
    QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, "错误", "无法打开图片");
        return;
    }
    setPreviewFromImage(img);

    // Perform search
    auto results = m_model->searchSimilar(fn, m_topKSpin->value(), m_hammingSlider->value());
    m_model->showResults(results);
    if (results.isEmpty()) {
        // Give a helpful hint when nothing is found
        QMessageBox::information(this, "未找到相似图片",
            "没有在当前阈值内找到相似图片。\n"
            "建议：\n"
            "1) 先在左侧选择要索引的目录并点击‘开始索引’；\n"
            "2) 适当调大‘最大汉明距离’（例如 16~24）；\n"
            "3) 也可以换一张更接近的图片再试试。");
    }
}

void MainWindow::findSimilar() {
    // Use selected item as query
    auto sel = m_listView->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择一张图片，或使用工具栏打开查询图片");
        return;
    }
    QString path = m_model->pathForIndex(sel.first());
    auto results = m_model->searchSimilar(path, m_topKSpin->value(), m_hammingSlider->value());
    m_model->showResults(results);
    if (results.isEmpty()) {
        QMessageBox::information(this, "未找到相似图片",
            "没有在当前阈值内找到相似图片。\n"
            "建议：调大‘最大汉明距离’，或先索引包含相似图片的目录。");
    }
}

void MainWindow::onSelectionChanged() {
    auto sel = m_listView->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) return;
    QString path = m_model->pathForIndex(sel.first());
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize pv = m_previewLabel->size();
    if (pv.width() > 0 && pv.height() > 0) {
        QSize tgt = reader.size();
        if (tgt.isValid()) {
            tgt.scale(qMax(512, pv.width()), qMax(512, pv.height()), Qt::KeepAspectRatio);
            reader.setScaledSize(tgt);
        }
    }
    QImage img = reader.read();
    if (!img.isNull()) {
        setPreviewFromImage(img);
        QFileInfo fi(path);
        m_metaLabel->setText(QString("%1\n%2\n%3x%4")
            .arg(fi.fileName())
            .arg(humanSize(fi.size()))
            .arg(img.width()).arg(img.height()));
    }
}

void MainWindow::setPreviewFromImage(const QImage& img) {
    if (img.isNull()) { m_previewLabel->clear(); return; }
    m_previewLabel->clear();
    QSize target = m_previewLabel->size();
    if (target.width() <= 0 || target.height() <= 0) {
        m_previewLabel->setPixmap(QPixmap::fromImage(img));
        return;
    }
    QPixmap pm = QPixmap::fromImage(img.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_previewLabel->setPixmap(pm);
}

void MainWindow::loadAllFromDb() {
    m_model->loadAll();
}

void MainWindow::loadSettings() {
    QSettings s;
    m_folderEdit->setText(s.value("lastFolder").toString());
    int thumb = s.value("thumbSize", 160).toInt();
    m_thumbSizeSlider->setValue(thumb);
    int topk = s.value("topK", 50).toInt();
    m_topKSpin->setValue(topk);
    int ham = s.value("maxHamming", 16).toInt();
    m_hammingSlider->setValue(ham);
}

void MainWindow::saveSettings() {
    QSettings s;
    s.setValue("lastFolder", m_folderEdit->text());
    s.setValue("thumbSize", m_thumbSizeSlider->value());
    s.setValue("topK", m_topKSpin->value());
    s.setValue("maxHamming", m_hammingSlider->value());
}
