#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>

#if defined(USE_AVX512)
#include <immintrin.h>
#elif defined(USE_AVX2)
#include <immintrin.h>
#endif

namespace vectordb {
namespace simd {

float dot_product(const float* a, const float* b, size_t dim);

float euclidean_distance(const float* a, const float* b, size_t dim);

float cosine_similarity(const float* a, const float* b, size_t dim);

void normalize(float* vec, size_t dim);

float magnitude(const float* vec, size_t dim);

void add_vectors(const float* a, const float* b, float* result, size_t dim);

void subtract_vectors(const float* a, const float* b, float* result, size_t dim);

void scale_vector(const float* vec, float scalar, float* result, size_t dim);

#if defined(USE_AVX512)

inline float dot_product_avx512(const float* a, const float* b, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    size_t i = 0;

    for (; i + 16 <= dim; i += 16) {
        __m512 va = _mm512_loadu_ps(a + i);
        __m512 vb = _mm512_loadu_ps(b + i);
        sum = _mm512_fmadd_ps(va, vb, sum);
    }

    float result = _mm512_reduce_add_ps(sum);

    for (; i < dim; ++i) {
        result += a[i] * b[i];
    }

    return result;
}

inline float euclidean_distance_avx512(const float* a, const float* b, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    size_t i = 0;

    for (; i + 16 <= dim; i += 16) {
        __m512 va = _mm512_loadu_ps(a + i);
        __m512 vb = _mm512_loadu_ps(b + i);
        __m512 diff = _mm512_sub_ps(va, vb);
        sum = _mm512_fmadd_ps(diff, diff, sum);
    }

    float result = _mm512_reduce_add_ps(sum);

    for (; i < dim; ++i) {
        float diff = a[i] - b[i];
        result += diff * diff;
    }

    return std::sqrt(result);
}

#elif defined(USE_AVX2)

inline float dot_product_avx2(const float* a, const float* b, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;

    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_fmadd_ps(va, vb, sum);
    }

    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(hi, lo);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);

    float result = _mm_cvtss_f32(sum128);

    for (; i < dim; ++i) {
        result += a[i] * b[i];
    }

    return result;
}

inline float euclidean_distance_avx2(const float* a, const float* b, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;

    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 diff = _mm256_sub_ps(va, vb);
        sum = _mm256_fmadd_ps(diff, diff, sum);
    }

    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(hi, lo);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);

    float result = _mm_cvtss_f32(sum128);

    for (; i < dim; ++i) {
        float diff = a[i] - b[i];
        result += diff * diff;
    }

    return std::sqrt(result);
}

#endif

inline float dot_product_scalar(const float* a, const float* b, size_t dim) {
    float result = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

inline float euclidean_distance_scalar(const float* a, const float* b, size_t dim) {
    float result = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        float diff = a[i] - b[i];
        result += diff * diff;
    }
    return std::sqrt(result);
}

}
}
