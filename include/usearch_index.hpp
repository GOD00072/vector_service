#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <atomic>
#include <usearch/index.hpp>

namespace vectordb {

enum class DistanceMetric {
    Euclidean,
    Cosine,
    DotProduct
};

struct HNSWConfig {
    size_t M = 16;
    size_t ef_construction = 200;
    size_t ef_search = 50;
    size_t max_elements = 1000000;
    DistanceMetric metric = DistanceMetric::Cosine;
};

struct VectorData {
    std::string id;
    std::vector<float> values;
    std::unordered_map<std::string, std::string> metadata;
};

struct HNSWResult {
    std::string id;
    float distance;
    const VectorData* data;

    bool operator<(const HNSWResult& other) const {
        return distance < other.distance;
    }

    bool operator>(const HNSWResult& other) const {
        return distance > other.distance;
    }
};

class HNSWIndex {
public:
    using index_t = unum::usearch::index_dense_gt<>;
    using key_t = uint64_t;

    explicit HNSWIndex(size_t dimension, const HNSWConfig& config = HNSWConfig{});
    ~HNSWIndex();

    HNSWIndex(const HNSWIndex&) = delete;
    HNSWIndex& operator=(const HNSWIndex&) = delete;
    HNSWIndex(HNSWIndex&&) = delete;
    HNSWIndex& operator=(HNSWIndex&&) = delete;

    std::string insert(const std::vector<float>& vector,
                       const std::string& id = "",
                       const std::unordered_map<std::string, std::string>& metadata = {});

    size_t batch_insert(const std::vector<VectorData>& vectors);

    bool remove(const std::string& id);

    std::vector<HNSWResult> search(const std::vector<float>& query,
                                   size_t k,
                                   size_t ef = 0) const;

    std::vector<std::vector<HNSWResult>> batch_search(
        const std::vector<std::vector<float>>& queries,
        size_t k,
        size_t ef = 0) const;

    const VectorData* get(const std::string& id) const;

    bool save(const std::string& path) const;
    bool load(const std::string& path);

    size_t size() const { return num_elements_.load(); }
    size_t dimension() const { return dimension_; }
    size_t memory_usage() const;

private:
    size_t dimension_;
    HNSWConfig config_;
    std::atomic<size_t> num_elements_{0};
    std::atomic<key_t> next_key_{1};

    std::unique_ptr<index_t> index_;
    std::unordered_map<key_t, VectorData> data_;
    std::unordered_map<std::string, key_t> id_to_key_;

    mutable std::shared_mutex mutex_;

    std::string generate_id();
};

}
