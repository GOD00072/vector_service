#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <random>
#include "simd_ops.hpp"

using namespace vectordb::simd;

bool approx_equal(float a, float b, float epsilon = 1e-5f) {
    return std::abs(a - b) < epsilon;
}

void test_dot_product() {
    std::cout << "Testing dot product..." << std::endl;

    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> b = {8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};

    float expected = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        expected += a[i] * b[i];
    }

    float result = dot_product(a.data(), b.data(), a.size());

    if (approx_equal(result, expected)) {
        std::cout << "  PASS: dot_product = " << result << std::endl;
    } else {
        std::cout << "  FAIL: expected " << expected << ", got " << result << std::endl;
    }
}

void test_euclidean_distance() {
    std::cout << "Testing euclidean distance..." << std::endl;

    std::vector<float> a = {0.0f, 0.0f, 0.0f};
    std::vector<float> b = {1.0f, 2.0f, 2.0f};

    float expected = 3.0f;
    float result = euclidean_distance(a.data(), b.data(), a.size());

    if (approx_equal(result, expected)) {
        std::cout << "  PASS: euclidean_distance = " << result << std::endl;
    } else {
        std::cout << "  FAIL: expected " << expected << ", got " << result << std::endl;
    }
}

void test_cosine_similarity() {
    std::cout << "Testing cosine similarity..." << std::endl;

    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {1.0f, 0.0f, 0.0f};

    float result = cosine_similarity(a.data(), b.data(), a.size());

    if (approx_equal(result, 1.0f)) {
        std::cout << "  PASS: cosine_similarity = " << result << std::endl;
    } else {
        std::cout << "  FAIL: expected 1.0, got " << result << std::endl;
    }

    std::vector<float> c = {1.0f, 0.0f, 0.0f};
    std::vector<float> d = {0.0f, 1.0f, 0.0f};

    result = cosine_similarity(c.data(), d.data(), c.size());

    if (approx_equal(result, 0.0f)) {
        std::cout << "  PASS: orthogonal vectors = " << result << std::endl;
    } else {
        std::cout << "  FAIL: expected 0.0, got " << result << std::endl;
    }
}

void benchmark_dot_product() {
    std::cout << "\nBenchmarking dot product (dim=1536, 100k iterations)..." << std::endl;

    const size_t dim = 1536;
    const size_t iterations = 100000;

    std::vector<float> a(dim), b(dim);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (size_t i = 0; i < dim; ++i) {
        a[i] = dist(rng);
        b[i] = dist(rng);
    }

    auto start = std::chrono::high_resolution_clock::now();

    float result = 0.0f;
    for (size_t i = 0; i < iterations; ++i) {
        result += dot_product(a.data(), b.data(), dim);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "  Total time: " << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "  Per iteration: " << duration.count() / static_cast<double>(iterations) << " us" << std::endl;
    std::cout << "  Throughput: " << (iterations * 1000000.0) / duration.count() << " ops/sec" << std::endl;
    std::cout << "  (result checksum: " << result << ")" << std::endl;
}

int main() {
    std::cout << "=== SIMD Operations Tests ===" << std::endl;

#if defined(USE_AVX512)
    std::cout << "Using AVX-512 instructions" << std::endl;
#elif defined(USE_AVX2)
    std::cout << "Using AVX2 instructions" << std::endl;
#else
    std::cout << "Using scalar operations" << std::endl;
#endif

    std::cout << std::endl;

    test_dot_product();
    test_euclidean_distance();
    test_cosine_similarity();

    benchmark_dot_product();

    std::cout << "\n=== Tests Complete ===" << std::endl;
    return 0;
}
