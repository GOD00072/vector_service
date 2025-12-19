#include "simd_ops.hpp"
#include <cstring>

namespace vectordb {
namespace simd {

float dot_product(const float* a, const float* b, size_t dim) {
#if defined(USE_AVX512)
    return dot_product_avx512(a, b, dim);
#elif defined(USE_AVX2)
    return dot_product_avx2(a, b, dim);
#else
    return dot_product_scalar(a, b, dim);
#endif
}

float euclidean_distance(const float* a, const float* b, size_t dim) {
#if defined(USE_AVX512)
    return euclidean_distance_avx512(a, b, dim);
#elif defined(USE_AVX2)
    return euclidean_distance_avx2(a, b, dim);
#else
    return euclidean_distance_scalar(a, b, dim);
#endif
}

float cosine_similarity(const float* a, const float* b, size_t dim) {
    float dot = dot_product(a, b, dim);
    float mag_a = magnitude(a, dim);
    float mag_b = magnitude(b, dim);

    if (mag_a < 1e-9f || mag_b < 1e-9f) {
        return 0.0f;
    }

    return dot / (mag_a * mag_b);
}

float magnitude(const float* vec, size_t dim) {
    return std::sqrt(dot_product(vec, vec, dim));
}

void normalize(float* vec, size_t dim) {
    float mag = magnitude(vec, dim);
    if (mag < 1e-9f) return;

    float inv_mag = 1.0f / mag;
    scale_vector(vec, inv_mag, vec, dim);
}

void add_vectors(const float* a, const float* b, float* result, size_t dim) {
#if defined(USE_AVX2)
    size_t i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vr = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(result + i, vr);
    }
    for (; i < dim; ++i) {
        result[i] = a[i] + b[i];
    }
#else
    for (size_t i = 0; i < dim; ++i) {
        result[i] = a[i] + b[i];
    }
#endif
}

void subtract_vectors(const float* a, const float* b, float* result, size_t dim) {
#if defined(USE_AVX2)
    size_t i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vr = _mm256_sub_ps(va, vb);
        _mm256_storeu_ps(result + i, vr);
    }
    for (; i < dim; ++i) {
        result[i] = a[i] - b[i];
    }
#else
    for (size_t i = 0; i < dim; ++i) {
        result[i] = a[i] - b[i];
    }
#endif
}

void scale_vector(const float* vec, float scalar, float* result, size_t dim) {
#if defined(USE_AVX2)
    __m256 vs = _mm256_set1_ps(scalar);
    size_t i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 vv = _mm256_loadu_ps(vec + i);
        __m256 vr = _mm256_mul_ps(vv, vs);
        _mm256_storeu_ps(result + i, vr);
    }
    for (; i < dim; ++i) {
        result[i] = vec[i] * scalar;
    }
#else
    for (size_t i = 0; i < dim; ++i) {
        result[i] = vec[i] * scalar;
    }
#endif
}

}
}
