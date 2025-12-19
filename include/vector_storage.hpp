#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <optional>
#include "hnsw_index.hpp"

namespace vectordb {

struct CollectionConfig {
    std::string name;
    size_t dimension;
    DistanceMetric metric = DistanceMetric::Cosine;
    HNSWConfig hnsw_config;
};

struct CollectionStats {
    size_t vector_count;
    size_t memory_usage;
    size_t dimension;
    std::string metric;
};

class VectorStorage {
public:
    explicit VectorStorage(const std::string& data_dir = "./data");
    ~VectorStorage();

    VectorStorage(const VectorStorage&) = delete;
    VectorStorage& operator=(const VectorStorage&) = delete;

    bool create_collection(const CollectionConfig& config);

    bool delete_collection(const std::string& name);

    std::vector<std::string> list_collections() const;

    bool collection_exists(const std::string& name) const;

    std::optional<CollectionStats> get_stats(const std::string& name) const;

    std::string insert(const std::string& collection,
                       const std::vector<float>& vector,
                       const std::string& id = "",
                       const std::unordered_map<std::string, std::string>& metadata = {});

    size_t batch_insert(const std::string& collection,
                        const std::vector<VectorData>& vectors);

    bool remove(const std::string& collection, const std::string& id);

    std::vector<HNSWResult> search(const std::string& collection,
                                     const std::vector<float>& query,
                                     size_t k,
                                     size_t ef = 0) const;

    std::vector<std::vector<HNSWResult>> batch_search(
        const std::string& collection,
        const std::vector<std::vector<float>>& queries,
        size_t k,
        size_t ef = 0) const;

    const VectorData* get(const std::string& collection, const std::string& id) const;

    bool save_all() const;
    bool load_all();

private:
    std::string data_dir_;
    std::unordered_map<std::string, std::unique_ptr<HNSWIndex>> collections_;
    std::unordered_map<std::string, CollectionConfig> configs_;
    mutable std::shared_mutex mutex_;

    std::string collection_path(const std::string& name) const;
    std::string config_path(const std::string& name) const;

    bool save_collection(const std::string& name) const;
    bool load_collection(const std::string& name);
    bool save_config(const std::string& name) const;
    bool load_config(const std::string& name);
};

}
