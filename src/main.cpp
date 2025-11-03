#include <QApplication>
#include <QImageReader>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Differ");
    QApplication::setOrganizationName("Local");
    QApplication::setApplicationVersion("0.1.0");

    // Raise image allocation limit to handle large images, but avoid unbounded
    QImageReader::setAllocationLimit(1024); // in megabytes

    MainWindow w;
    w.resize(1280, 800);
    w.show();

    return app.exec();
}
