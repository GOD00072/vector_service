#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <atomic>
#include <chrono>
#include "vector_service.grpc.pb.h"
#include "vector_storage.hpp"

namespace vectordb {

class VectorServiceImpl final : public ::vectordb::VectorService::Service {
public:
    explicit VectorServiceImpl(std::shared_ptr<VectorStorage> storage);
    ~VectorServiceImpl() override = default;

    grpc::Status CreateCollection(
        grpc::ServerContext* context,
        const ::vectordb::CreateCollectionRequest* request,
        ::vectordb::CreateCollectionResponse* response) override;

    grpc::Status DeleteCollection(
        grpc::ServerContext* context,
        const ::vectordb::DeleteCollectionRequest* request,
        ::vectordb::DeleteCollectionResponse* response) override;

    grpc::Status ListCollections(
        grpc::ServerContext* context,
        const ::vectordb::ListCollectionsRequest* request,
        ::vectordb::ListCollectionsResponse* response) override;

    grpc::Status Insert(
        grpc::ServerContext* context,
        const ::vectordb::InsertRequest* request,
        ::vectordb::InsertResponse* response) override;

    grpc::Status BatchInsert(
        grpc::ServerContext* context,
        const ::vectordb::BatchInsertRequest* request,
        ::vectordb::BatchInsertResponse* response) override;

    grpc::Status Delete(
        grpc::ServerContext* context,
        const ::vectordb::DeleteRequest* request,
        ::vectordb::DeleteResponse* response) override;

    grpc::Status Search(
        grpc::ServerContext* context,
        const ::vectordb::SearchRequest* request,
        ::vectordb::SearchResponse* response) override;

    grpc::Status BatchSearch(
        grpc::ServerContext* context,
        const ::vectordb::BatchSearchRequest* request,
        ::vectordb::BatchSearchResponse* response) override;

    grpc::Status GetVector(
        grpc::ServerContext* context,
        const ::vectordb::GetVectorRequest* request,
        ::vectordb::GetVectorResponse* response) override;

    grpc::Status Health(
        grpc::ServerContext* context,
        const ::vectordb::HealthRequest* request,
        ::vectordb::HealthResponse* response) override;

    grpc::Status Stats(
        grpc::ServerContext* context,
        const ::vectordb::StatsRequest* request,
        ::vectordb::StatsResponse* response) override;

private:
    std::shared_ptr<VectorStorage> storage_;
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<uint64_t> total_searches_{0};
    std::atomic<double> total_search_time_{0.0};
};

class GRPCServer {
public:
    GRPCServer(const std::string& address, std::shared_ptr<VectorStorage> storage);
    ~GRPCServer();

    void run();
    void shutdown();

private:
    std::string address_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<VectorServiceImpl> service_;
};

}
