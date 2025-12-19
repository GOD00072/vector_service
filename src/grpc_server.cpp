#include "grpc_server.hpp"
#include <iostream>

namespace vectordb {

VectorServiceImpl::VectorServiceImpl(std::shared_ptr<VectorStorage> storage)
    : storage_(std::move(storage))
    , start_time_(std::chrono::steady_clock::now())
{
}

grpc::Status VectorServiceImpl::CreateCollection(
    grpc::ServerContext*,
    const ::vectordb::CreateCollectionRequest* request,
    ::vectordb::CreateCollectionResponse* response)
{
    CollectionConfig config;
    config.name = request->name();
    config.dimension = request->dimension();

    if (request->metric() == "euclidean") {
        config.metric = DistanceMetric::Euclidean;
    } else if (request->metric() == "dot_product") {
        config.metric = DistanceMetric::DotProduct;
    } else {
        config.metric = DistanceMetric::Cosine;
    }

    if (request->has_index_config()) {
        config.hnsw_config.M = request->index_config().m();
        config.hnsw_config.ef_construction = request->index_config().ef_construction();
        config.hnsw_config.ef_search = request->index_config().ef_search();
    }
    config.hnsw_config.metric = config.metric;

    bool success = storage_->create_collection(config);
    response->set_success(success);
    response->set_message(success ? "Collection created" : "Collection already exists");

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::DeleteCollection(
    grpc::ServerContext*,
    const ::vectordb::DeleteCollectionRequest* request,
    ::vectordb::DeleteCollectionResponse* response)
{
    bool success = storage_->delete_collection(request->name());
    response->set_success(success);
    response->set_message(success ? "Collection deleted" : "Collection not found");

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::ListCollections(
    grpc::ServerContext*,
    const ::vectordb::ListCollectionsRequest*,
    ::vectordb::ListCollectionsResponse* response)
{
    auto names = storage_->list_collections();

    for (const auto& name : names) {
        auto stats = storage_->get_stats(name);
        if (stats) {
            auto* info = response->add_collections();
            info->set_name(name);
            info->set_dimension(stats->dimension);
            info->set_count(stats->vector_count);
            info->set_metric(stats->metric);
        }
    }

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::Insert(
    grpc::ServerContext*,
    const ::vectordb::InsertRequest* request,
    ::vectordb::InsertResponse* response)
{
    try {
        const auto& vec = request->vector();
        std::vector<float> values(vec.values().begin(), vec.values().end());

        std::unordered_map<std::string, std::string> metadata;
        for (const auto& [k, v] : vec.metadata()) {
            metadata[k] = v;
        }

        std::string id = storage_->insert(
            request->collection(),
            values,
            vec.id(),
            metadata
        );

        response->set_success(true);
        response->set_id(id);
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_id("");
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::BatchInsert(
    grpc::ServerContext*,
    const ::vectordb::BatchInsertRequest* request,
    ::vectordb::BatchInsertResponse* response)
{
    try {
        std::vector<VectorData> vectors;
        vectors.reserve(request->vectors_size());

        for (const auto& v : request->vectors()) {
            VectorData data;
            data.id = v.id();
            data.values = std::vector<float>(v.values().begin(), v.values().end());
            for (const auto& [k, val] : v.metadata()) {
                data.metadata[k] = val;
            }
            vectors.push_back(std::move(data));
        }

        size_t count = storage_->batch_insert(request->collection(), vectors);

        response->set_success(true);
        response->set_inserted_count(count);
    } catch (const std::exception& e) {
        response->set_success(false);
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::Delete(
    grpc::ServerContext*,
    const ::vectordb::DeleteRequest* request,
    ::vectordb::DeleteResponse* response)
{
    bool success = storage_->remove(request->collection(), request->id());
    response->set_success(success);
    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::Search(
    grpc::ServerContext*,
    const ::vectordb::SearchRequest* request,
    ::vectordb::SearchResponse* response)
{
    try {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<float> query(request->query().begin(), request->query().end());
        auto results = storage_->search(
            request->collection(),
            query,
            request->top_k()
        );

        auto end = std::chrono::high_resolution_clock::now();
        float time_ms = std::chrono::duration<float, std::milli>(end - start).count();

        for (const auto& r : results) {
            auto* result = response->add_results();
            result->set_id(r.id);
            result->set_score(r.distance);

            if (r.data) {
                for (float v : r.data->values) {
                    result->add_values(v);
                }
                for (const auto& [k, v] : r.data->metadata) {
                    (*result->mutable_metadata())[k] = v;
                }
            }
        }

        response->set_search_time_ms(time_ms);
        total_searches_.fetch_add(1);
        total_search_time_.store(total_search_time_.load() + time_ms);

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::BatchSearch(
    grpc::ServerContext*,
    const ::vectordb::BatchSearchRequest* request,
    ::vectordb::BatchSearchResponse* response)
{
    try {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::vector<float>> queries;
        queries.reserve(request->queries_size());

        for (const auto& q : request->queries()) {
            queries.emplace_back(q.values().begin(), q.values().end());
        }

        auto all_results = storage_->batch_search(
            request->collection(),
            queries,
            request->top_k()
        );

        auto end = std::chrono::high_resolution_clock::now();
        float time_ms = std::chrono::duration<float, std::milli>(end - start).count();

        for (const auto& results : all_results) {
            auto* result_list = response->add_results();

            for (const auto& r : results) {
                auto* result = result_list->add_results();
                result->set_id(r.id);
                result->set_score(r.distance);

                if (r.data) {
                    for (float v : r.data->values) {
                        result->add_values(v);
                    }
                    for (const auto& [k, v] : r.data->metadata) {
                        (*result->mutable_metadata())[k] = v;
                    }
                }
            }
        }

        response->set_total_time_ms(time_ms);

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::GetVector(
    grpc::ServerContext*,
    const ::vectordb::GetVectorRequest* request,
    ::vectordb::GetVectorResponse* response)
{
    const VectorData* data = storage_->get(request->collection(), request->id());

    if (data) {
        response->set_found(true);
        auto* vec = response->mutable_vector();
        vec->set_id(data->id);
        for (float v : data->values) {
            vec->add_values(v);
        }
        for (const auto& [k, v] : data->metadata) {
            (*vec->mutable_metadata())[k] = v;
        }
    } else {
        response->set_found(false);
    }

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::Health(
    grpc::ServerContext*,
    const ::vectordb::HealthRequest*,
    ::vectordb::HealthResponse* response)
{
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

    response->set_healthy(true);
    response->set_version("1.0.0");
    response->set_uptime_seconds(uptime);

    return grpc::Status::OK;
}

grpc::Status VectorServiceImpl::Stats(
    grpc::ServerContext*,
    const ::vectordb::StatsRequest* request,
    ::vectordb::StatsResponse* response)
{
    auto stats = storage_->get_stats(request->collection());

    if (stats) {
        response->set_total_vectors(stats->vector_count);
        response->set_memory_usage_bytes(stats->memory_usage);
        response->set_index_size_bytes(stats->memory_usage);

        uint64_t searches = total_searches_.load();
        if (searches > 0) {
            response->set_avg_search_time_ms(total_search_time_.load() / searches);
        }
    }

    return grpc::Status::OK;
}

GRPCServer::GRPCServer(const std::string& address, std::shared_ptr<VectorStorage> storage)
    : address_(address)
    , service_(std::make_unique<VectorServiceImpl>(std::move(storage)))
{
}

GRPCServer::~GRPCServer() {
    shutdown();
}

void GRPCServer::run() {
    grpc::ServerBuilder builder;

    builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
    builder.RegisterService(service_.get());

    builder.SetMaxReceiveMessageSize(100 * 1024 * 1024);
    builder.SetMaxSendMessageSize(100 * 1024 * 1024);

    server_ = builder.BuildAndStart();

    std::cout << "Vector Service listening on " << address_ << std::endl;
    server_->Wait();
}

void GRPCServer::shutdown() {
    if (server_) {
        server_->Shutdown();
    }
}

}
