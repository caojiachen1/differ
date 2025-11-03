#pragma once
#include <QtCore>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace ImageHash {
    // 64-bit perceptual hash
    quint64 pHash(const QImage& src);

    inline int hammingDistance(quint64 a, quint64 b) {
        quint64 x = a ^ b;
#if defined(_MSC_VER)
        return (int)__popcnt64(x);
#else
        return (int)__builtin_popcountll(x);
#endif
    }
}
