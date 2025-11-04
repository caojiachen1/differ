#pragma once
#include <QtCore>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace ImageHash {
    // 64-bit perceptual hash
    quint64 pHash(const QImage& src);

        // 64-bit average hash (aHash)
        quint64 aHash(const QImage& src);

        // 64-bit difference hash (dHash)
        quint64 dHash(const QImage& src);

    inline int hammingDistance(quint64 a, quint64 b) {
        quint64 x = a ^ b;
#if defined(_MSC_VER)
        return (int)__popcnt64(x);
#else
        return (int)__builtin_popcountll(x);
#endif
    }
}
