#include "vector_storage.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace vectordb {

VectorStorage::VectorStorage(const std::string& data_dir)
    : data_dir_(data_dir)
{
    fs::create_directories(data_dir_);
    load_all();
}

VectorStorage::~VectorStorage() {
    save_all();
}

std::string VectorStorage::collection_path(const std::string& name) const {
    return data_dir_ + "/" + name + ".hnsw";
}

std::string VectorStorage::config_path(const std::string& name) const {
    return data_dir_ + "/" + name + ".json";
}

bool VectorStorage::create_collection(const CollectionConfig& config) {
    std::unique_lock lock(mutex_);

    if (collections_.count(config.name)) {
        return false;
    }

    auto index = std::make_unique<HNSWIndex>(config.dimension, config.hnsw_config);
    collections_[config.name] = std::move(index);
    configs_[config.name] = config;

    save_config(config.name);
    return true;
}

bool VectorStorage::delete_collection(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto it = collections_.find(name);
    if (it == collections_.end()) {
        return false;
    }

    collections_.erase(it);
    configs_.erase(name);

    fs::remove(collection_path(name));
    fs::remove(config_path(name));

    return true;
}

std::vector<std::string> VectorStorage::list_collections() const {
    std::shared_lock lock(mutex_);

    std::vector<std::string> names;
    names.reserve(collections_.size());

    for (const auto& [name, _] : collections_) {
        names.push_back(name);
    }

    return names;
}

bool VectorStorage::collection_exists(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return collections_.count(name) > 0;
}

std::optional<CollectionStats> VectorStorage::get_stats(const std::string& name) const {
    std::shared_lock lock(mutex_);

    auto it = collections_.find(name);
    if (it == collections_.end()) {
        return std::nullopt;
    }

    const auto& index = it->second;
    const auto& config = configs_.at(name);

    std::string metric_str;
    switch (config.metric) {
        case DistanceMetric::Euclidean: metric_str = "euclidean"; break;
        case DistanceMetric::Cosine: metric_str = "cosine"; break;
        case DistanceMetric::DotProduct: metric_str = "dot_product"; break;
    }

    return CollectionStats{
        index->size(),
        index->memory_usage(),
        index->dimension(),
        metric_str
    };
}

std::string VectorStorage::insert(
    const std::string& collection,
    const std::vector<float>& vector,
    const std::string& id,
    const std::unordered_map<std::string, std::string>& metadata)
{
    std::shared_lock lock(mutex_);

    auto it = collections_.find(collection);
    if (it == collections_.end()) {
        throw std::runtime_error("Collection not found: " + collection);
    }

    return it->second->insert(vector, id, metadata);
}

size_t VectorStorage::batch_insert(
    const std::string& collection,
    const std::vector<VectorData>& vectors)
{
    std::shared_lock lock(mutex_);

    auto it = collections_.find(collection);
    if (it == collections_.end()) {
        throw std::runtime_error("Collection not found: " + collection);
    }

    return it->second->batch_insert(vectors);
}

bool VectorStorage::remove(const std::string& collection, const std::string& id) {
    std::shared_lock lock(mutex_);

    auto it = collections_.find(collection);
    if (it == collections_.end()) {
        return false;
    }

    return it->second->remove(id);
}

std::vector<HNSWResult> VectorStorage::search(
    const std::string& collection,
    const std::vector<float>& query,
    size_t k,
    size_t ef) const
{
    std::shared_lock lock(mutex_);

    auto it = collections_.find(collection);
    if (it == collections_.end()) {
        throw std::runtime_error("Collection not found: " + collection);
    }

    return it->second->search(query, k, ef);
}

std::vector<std::vector<HNSWResult>> VectorStorage::batch_search(
    const std::string& collection,
    const std::vector<std::vector<float>>& queries,
    size_t k,
    size_t ef) const
{
    std::shared_lock lock(mutex_);

    auto it = collections_.find(collection);
    if (it == collections_.end()) {
        throw std::runtime_error("Collection not found: " + collection);
    }

    return it->second->batch_search(queries, k, ef);
}

const VectorData* VectorStorage::get(const std::string& collection, const std::string& id) const {
    std::shared_lock lock(mutex_);

    auto it = collections_.find(collection);
    if (it == collections_.end()) {
        return nullptr;
    }

    return it->second->get(id);
}

bool VectorStorage::save_config(const std::string& name) const {
    auto it = configs_.find(name);
    if (it == configs_.end()) return false;

    const auto& config = it->second;
    std::ofstream ofs(config_path(name));
    if (!ofs) return false;

    ofs << "{\n";
    ofs << "  \"name\": \"" << config.name << "\",\n";
    ofs << "  \"dimension\": " << config.dimension << ",\n";
    ofs << "  \"metric\": " << static_cast<int>(config.metric) << ",\n";
    ofs << "  \"M\": " << config.hnsw_config.M << ",\n";
    ofs << "  \"ef_construction\": " << config.hnsw_config.ef_construction << ",\n";
    ofs << "  \"ef_search\": " << config.hnsw_config.ef_search << "\n";
    ofs << "}\n";

    return ofs.good();
}

bool VectorStorage::load_config(const std::string& name) {
    std::ifstream ifs(config_path(name));
    if (!ifs) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    CollectionConfig config;
    config.name = name;

    auto extract_int = [&content](const std::string& key) -> int {
        auto pos = content.find("\"" + key + "\":");
        if (pos == std::string::npos) return 0;
        pos = content.find(":", pos) + 1;
        return std::stoi(content.substr(pos));
    };

    config.dimension = extract_int("dimension");
    config.metric = static_cast<DistanceMetric>(extract_int("metric"));
    config.hnsw_config.M = extract_int("M");
    config.hnsw_config.ef_construction = extract_int("ef_construction");
    config.hnsw_config.ef_search = extract_int("ef_search");
    config.hnsw_config.metric = config.metric;

    configs_[name] = config;
    return true;
}

bool VectorStorage::save_collection(const std::string& name) const {
    auto it = collections_.find(name);
    if (it == collections_.end()) return false;

    return it->second->save(collection_path(name));
}

bool VectorStorage::load_collection(const std::string& name) {
    if (!load_config(name)) return false;

    const auto& config = configs_[name];
    auto index = std::make_unique<HNSWIndex>(config.dimension, config.hnsw_config);

    if (!index->load(collection_path(name))) {
        return false;
    }

    collections_[name] = std::move(index);
    return true;
}

bool VectorStorage::save_all() const {
    std::shared_lock lock(mutex_);

    bool success = true;
    for (const auto& [name, _] : collections_) {
        success &= save_collection(name);
        success &= save_config(name);
    }
    return success;
}

bool VectorStorage::load_all() {
    for (const auto& entry : fs::directory_iterator(data_dir_)) {
        if (entry.path().extension() == ".json") {
            std::string name = entry.path().stem().string();

            if (fs::exists(collection_path(name))) {
                load_collection(name);
            }
        }
    }
    return true;
}

}
