#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include "hnsw_index.hpp"

using namespace vectordb;

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;

    HNSWConfig config;
    config.M = 16;
    config.ef_construction = 100;
    config.ef_search = 50;
    config.metric = DistanceMetric::Cosine;

    HNSWIndex index(128, config);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<float> v1(128), v2(128), v3(128);
    for (size_t i = 0; i < 128; ++i) {
        v1[i] = dist(rng);
        v2[i] = dist(rng);
        v3[i] = dist(rng);
    }

    std::string id1 = index.insert(v1, "vec1", {{"type", "test"}});
    std::string id2 = index.insert(v2, "vec2", {{"type", "test"}});
    std::string id3 = index.insert(v3, "vec3", {{"type", "test"}});

    std::cout << "  Inserted 3 vectors" << std::endl;
    std::cout << "  Index size: " << index.size() << std::endl;

    auto results = index.search(v1, 3);
    std::cout << "  Search results for v1:" << std::endl;
    for (const auto& r : results) {
        std::cout << "    ID: " << r.id << ", Distance: " << r.distance << std::endl;
    }

    if (results[0].id == "vec1" && results[0].distance < 0.001f) {
        std::cout << "  PASS: Found exact match" << std::endl;
    } else {
        std::cout << "  FAIL: Expected vec1 as top result" << std::endl;
    }

    auto* data = index.get("vec2");
    if (data && data->metadata.at("type") == "test") {
        std::cout << "  PASS: Retrieved vector with metadata" << std::endl;
    } else {
        std::cout << "  FAIL: Could not retrieve vector" << std::endl;
    }

    index.remove("vec2");
    if (index.size() == 2) {
        std::cout << "  PASS: Removed vector" << std::endl;
    } else {
        std::cout << "  FAIL: Remove failed" << std::endl;
    }
}

void test_save_load() {
    std::cout << "\nTesting save/load..." << std::endl;

    HNSWIndex index(64);

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (int i = 0; i < 100; ++i) {
        std::vector<float> v(64);
        for (auto& x : v) x = dist(rng);
        index.insert(v, "id_" + std::to_string(i));
    }

    index.save("/tmp/test_hnsw.bin");
    std::cout << "  Saved index with " << index.size() << " vectors" << std::endl;

    HNSWIndex index2(64);
    index2.load("/tmp/test_hnsw.bin");
    std::cout << "  Loaded index with " << index2.size() << " vectors" << std::endl;

    if (index2.size() == 100) {
        std::cout << "  PASS: Save/load successful" << std::endl;
    } else {
        std::cout << "  FAIL: Vector count mismatch" << std::endl;
    }
}

void benchmark_search() {
    std::cout << "\nBenchmarking search (10k vectors, dim=1536)..." << std::endl;

    HNSWConfig config;
    config.M = 32;
    config.ef_construction = 200;
    config.ef_search = 100;
    config.metric = DistanceMetric::Cosine;

    HNSWIndex index(1536, config);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::cout << "  Inserting vectors..." << std::endl;
    auto insert_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        std::vector<float> v(1536);
        for (auto& x : v) x = dist(rng);
        index.insert(v);
    }

    auto insert_end = std::chrono::high_resolution_clock::now();
    auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - insert_start);
    std::cout << "  Insert time: " << insert_time.count() << " ms" << std::endl;
    std::cout << "  Memory usage: " << index.memory_usage() / (1024 * 1024) << " MB" << std::endl;

    std::vector<float> query(1536);
    for (auto& x : query) x = dist(rng);

    std::cout << "  Running 1000 searches..." << std::endl;
    auto search_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        auto results = index.search(query, 10);
    }

    auto search_end = std::chrono::high_resolution_clock::now();
    auto search_time = std::chrono::duration_cast<std::chrono::microseconds>(search_end - search_start);

    std::cout << "  Total search time: " << search_time.count() / 1000.0 << " ms" << std::endl;
    std::cout << "  Per search: " << search_time.count() / 1000.0 << " us" << std::endl;
    std::cout << "  Throughput: " << (1000 * 1000000.0) / search_time.count() << " queries/sec" << std::endl;
}

int main() {
    std::cout << "=== HNSW Index Tests ===" << std::endl << std::endl;

    test_basic_operations();
    test_save_load();
    benchmark_search();

    std::cout << "\n=== Tests Complete ===" << std::endl;
    return 0;
}
