#include "ImageHash.h"
#include <QtGui/QImage>
#include <QtGui/QColor>
#include <vector>
#include <cmath>

// Implementation of pHash:
// 1) Convert to grayscale 32x32
// 2) Compute DCT (32x32)
// 3) Take top-left 8x8 (excluding DC), compute median
// 4) Set bits based on > median

namespace {
    static void dct1D(const double* in, double* out, int N) {
        const double PI = 3.14159265358979323846;
        const double factor = 1.0;
        for (int k = 0; k < N; ++k) {
            double sum = 0.0;
            for (int n = 0; n < N; ++n) {
                sum += in[n] * cos((PI / N) * (n + 0.5) * k);
            }
            double ck = (k == 0) ? sqrt(1.0 / N) : sqrt(2.0 / N);
            out[k] = ck * sum * factor;
        }
    }
}

quint64 ImageHash::pHash(const QImage& src) {
    if (src.isNull()) return 0;
    QImage img = src.convertToFormat(QImage::Format_Grayscale8).scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Prepare matrix
    double a[32][32];
    for (int y = 0; y < 32; ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < 32; ++x) {
            a[y][x] = (double)line[x];
        }
    }

    // DCT rows then cols
    double tmp[32][32];
    double rowIn[32], rowOut[32];
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) rowIn[x] = a[y][x];
        dct1D(rowIn, rowOut, 32);
        for (int x = 0; x < 32; ++x) tmp[y][x] = rowOut[x];
    }

    double colIn[32], colOut[32];
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 32; ++y) colIn[y] = tmp[y][x];
        dct1D(colIn, colOut, 32);
        for (int y = 0; y < 32; ++y) tmp[y][x] = colOut[y];
    }

    // Take top-left 8x8 (skip [0,0])
    std::vector<double> vals;
    vals.reserve(64);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (y == 0 && x == 0) continue;
            vals.push_back(tmp[y][x]);
        }
    }
    std::vector<double> sorted = vals;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size()/2, sorted.end());
    double median = sorted[sorted.size()/2];

    quint64 hash = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (y == 0 && x == 0) continue;
            double v = tmp[y][x];
            if (v > median) hash |= (1ull << bit);
            ++bit;
        }
    }
    return hash;
}
