#include "hnsw_index.hpp"
#include <chrono>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <future>
#include <algorithm>

namespace vectordb {

namespace us = unum::usearch;

HNSWIndex::HNSWIndex(size_t dimension, const HNSWConfig& config)
    : dimension_(dimension)
    , config_(config)
{
    us::metric_kind_t metric_kind;
    switch (config.metric) {
        case DistanceMetric::Euclidean:
            metric_kind = us::metric_kind_t::l2sq_k;
            break;
        case DistanceMetric::DotProduct:
            metric_kind = us::metric_kind_t::ip_k;
            break;
        case DistanceMetric::Cosine:
        default:
            metric_kind = us::metric_kind_t::cos_k;
            break;
    }

    us::metric_punned_t metric(dimension, metric_kind, us::scalar_kind_t::f32_k);

    us::index_dense_config_t index_config;
    index_config.connectivity = config.M;
    index_config.expansion_add = config.ef_construction;
    index_config.expansion_search = config.ef_search;

    auto result = index_t::make(metric, index_config);
    if (!result) {
        throw std::runtime_error("Failed to create USearch index");
    }
    index_ = std::make_unique<index_t>(std::move(result.index));

    index_->reserve(config.max_elements);
}

HNSWIndex::~HNSWIndex() = default;

std::string HNSWIndex::generate_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << std::hex << ms << "-" << next_key_.load();
    return oss.str();
}

std::string HNSWIndex::insert(
    const std::vector<float>& vector,
    const std::string& id,
    const std::unordered_map<std::string, std::string>& metadata)
{
    if (vector.size() != dimension_) {
        throw std::runtime_error("Vector dimension mismatch");
    }

    std::unique_lock lock(mutex_);

    std::string actual_id = id.empty() ? generate_id() : id;

    if (id_to_key_.count(actual_id)) {
        throw std::runtime_error("ID already exists: " + actual_id);
    }

    key_t key = next_key_++;

    index_->add(key, vector.data());

    VectorData data;
    data.id = actual_id;
    data.values = vector;
    data.metadata = metadata;

    data_[key] = std::move(data);
    id_to_key_[actual_id] = key;

    num_elements_++;
    return actual_id;
}

size_t HNSWIndex::batch_insert(const std::vector<VectorData>& vectors) {
    size_t count = 0;
    for (const auto& v : vectors) {
        try {
            insert(v.values, v.id, v.metadata);
            count++;
        } catch (...) {}
    }
    return count;
}

bool HNSWIndex::remove(const std::string& id) {
    std::unique_lock lock(mutex_);

    auto it = id_to_key_.find(id);
    if (it == id_to_key_.end()) {
        return false;
    }

    key_t key = it->second;

    index_->remove(key);

    data_.erase(key);
    id_to_key_.erase(it);

    num_elements_--;
    return true;
}

std::vector<HNSWResult> HNSWIndex::search(
    const std::vector<float>& query,
    size_t k,
    size_t ef) const
{
    if (query.size() != dimension_) {
        throw std::runtime_error("Query dimension mismatch");
    }

    std::shared_lock lock(mutex_);

    if (num_elements_ == 0) {
        return {};
    }

    size_t actual_k = std::min(k, num_elements_.load());

    auto results = index_->search(query.data(), actual_k);

    std::vector<HNSWResult> output;
    output.reserve(results.size());

    for (size_t i = 0; i < results.size(); ++i) {
        key_t key = results[i].member.key;
        float dist = results[i].distance;

        auto data_it = data_.find(key);
        if (data_it != data_.end()) {
            HNSWResult r;
            r.id = data_it->second.id;
            r.distance = dist;
            r.data = &data_it->second;
            output.push_back(r);
        }
    }

    return output;
}

std::vector<std::vector<HNSWResult>> HNSWIndex::batch_search(
    const std::vector<std::vector<float>>& queries,
    size_t k,
    size_t ef) const
{
    size_t num_queries = queries.size();

    // For small batches, use sequential search
    if (num_queries <= 100) {
        std::vector<std::vector<HNSWResult>> results;
        results.reserve(num_queries);
        for (const auto& q : queries) {
            results.push_back(search(q, k, ef));
        }
        return results;
    }

    // For large batches (100K+), use parallel search
    std::vector<std::vector<HNSWResult>> results(num_queries);

    // Determine optimal thread count
    size_t num_threads = std::min(
        static_cast<size_t>(std::thread::hardware_concurrency()),
        std::max(size_t(1), num_queries / 100)
    );
    num_threads = std::min(num_threads, size_t(32));  // Cap at 32 threads

    // Calculate chunk size per thread
    size_t chunk_size = (num_queries + num_threads - 1) / num_threads;

    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        size_t start_idx = t * chunk_size;
        size_t end_idx = std::min(start_idx + chunk_size, num_queries);

        if (start_idx >= num_queries) break;

        futures.push_back(std::async(std::launch::async, [this, &queries, &results, k, ef, start_idx, end_idx]() {
            for (size_t i = start_idx; i < end_idx; ++i) {
                results[i] = search(queries[i], k, ef);
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& f : futures) {
        f.get();
    }

    return results;
}

const VectorData* HNSWIndex::get(const std::string& id) const {
    std::shared_lock lock(mutex_);

    auto key_it = id_to_key_.find(id);
    if (key_it == id_to_key_.end()) {
        return nullptr;
    }

    auto data_it = data_.find(key_it->second);
    if (data_it == data_.end()) {
        return nullptr;
    }

    return &data_it->second;
}

bool HNSWIndex::save(const std::string& path) const {
    std::shared_lock lock(mutex_);

    try {
        index_->save(path.c_str());

        std::string meta_path = path + ".meta";
        std::ofstream ofs(meta_path, std::ios::binary);
        if (!ofs) return false;

        size_t num = data_.size();
        ofs.write(reinterpret_cast<const char*>(&num), sizeof(num));
        ofs.write(reinterpret_cast<const char*>(&next_key_), sizeof(key_t));

        for (const auto& [key, data] : data_) {
            ofs.write(reinterpret_cast<const char*>(&key), sizeof(key));

            size_t id_len = data.id.size();
            ofs.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
            ofs.write(data.id.data(), id_len);

            size_t vec_size = data.values.size();
            ofs.write(reinterpret_cast<const char*>(&vec_size), sizeof(vec_size));
            ofs.write(reinterpret_cast<const char*>(data.values.data()), vec_size * sizeof(float));

            size_t meta_size = data.metadata.size();
            ofs.write(reinterpret_cast<const char*>(&meta_size), sizeof(meta_size));
            for (const auto& [k, v] : data.metadata) {
                size_t k_len = k.size();
                size_t v_len = v.size();
                ofs.write(reinterpret_cast<const char*>(&k_len), sizeof(k_len));
                ofs.write(k.data(), k_len);
                ofs.write(reinterpret_cast<const char*>(&v_len), sizeof(v_len));
                ofs.write(v.data(), v_len);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool HNSWIndex::load(const std::string& path) {
    std::unique_lock lock(mutex_);

    try {
        index_->load(path.c_str());

        std::string meta_path = path + ".meta";
        std::ifstream ifs(meta_path, std::ios::binary);
        if (!ifs) return false;

        size_t num;
        ifs.read(reinterpret_cast<char*>(&num), sizeof(num));

        key_t next_key;
        ifs.read(reinterpret_cast<char*>(&next_key), sizeof(key_t));
        next_key_.store(next_key);

        data_.clear();
        id_to_key_.clear();

        for (size_t i = 0; i < num; ++i) {
            key_t key;
            ifs.read(reinterpret_cast<char*>(&key), sizeof(key));

            VectorData data;

            size_t id_len;
            ifs.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
            data.id.resize(id_len);
            ifs.read(data.id.data(), id_len);

            size_t vec_size;
            ifs.read(reinterpret_cast<char*>(&vec_size), sizeof(vec_size));
            data.values.resize(vec_size);
            ifs.read(reinterpret_cast<char*>(data.values.data()), vec_size * sizeof(float));

            size_t meta_size;
            ifs.read(reinterpret_cast<char*>(&meta_size), sizeof(meta_size));
            for (size_t j = 0; j < meta_size; ++j) {
                size_t k_len, v_len;
                ifs.read(reinterpret_cast<char*>(&k_len), sizeof(k_len));
                std::string k(k_len, '\0');
                ifs.read(k.data(), k_len);
                ifs.read(reinterpret_cast<char*>(&v_len), sizeof(v_len));
                std::string v(v_len, '\0');
                ifs.read(v.data(), v_len);
                data.metadata[k] = v;
            }

            id_to_key_[data.id] = key;
            data_[key] = std::move(data);
        }

        num_elements_.store(data_.size());
        return true;
    } catch (...) {
        return false;
    }
}

size_t HNSWIndex::memory_usage() const {
    std::shared_lock lock(mutex_);

    size_t usage = index_->memory_usage();

    for (const auto& [key, data] : data_) {
        usage += sizeof(key);
        usage += data.id.capacity();
        usage += data.values.capacity() * sizeof(float);
        for (const auto& [k, v] : data.metadata) {
            usage += k.capacity() + v.capacity();
        }
    }

    return usage;
}

}
