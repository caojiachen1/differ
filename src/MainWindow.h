#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QFutureWatcher>

class QListView;
class QLabel;
class QProgressBar;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QSlider;
class QDockWidget;
class QAction;
class QFileSystemWatcher;
class ThumbnailModel;
class ImageIndexer;
class QCloseEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void chooseFolder();
    void startIndexing(const QString& folder);
    void onIndexingProgress(int indexed, int total);
    void onIndexingFinished();

    void openQueryImage();
    void findSimilar();
    void onSelectionChanged();

private:
    void setupUi();
    void setupConnections();
    void loadAllFromDb();
    void loadSettings();
    void saveSettings();
    void setPreviewFromImage(const QImage& img);

    // UI
    QListView* m_listView{};
    ThumbnailModel* m_model{};

    // Left dock controls
    QDockWidget* m_leftDock{};
    QLineEdit* m_folderEdit{};
    QPushButton* m_browseBtn{};
    QPushButton* m_indexBtn{};
    QSlider* m_thumbSizeSlider{};
    QLabel* m_thumbSizeLabel{};

    // Right dock controls
    QDockWidget* m_rightDock{};
    QLabel* m_previewLabel{};
    QLabel* m_metaLabel{};
    QPushButton* m_queryBtn{};

    // Toolbar/search
    QAction* m_openQueryAction{};
    QAction* m_showAllAction{};
    QSpinBox* m_topKSpin{};
    QSlider* m_hammingSlider{};
    QLabel* m_hammingValue{};

    // Status
    QProgressBar* m_progress{};

    // Workers
    ImageIndexer* m_indexer{};
};
