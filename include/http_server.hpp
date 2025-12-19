#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include "vector_storage.hpp"

namespace vectordb {

class HTTPServer {
public:
    HTTPServer(int port, std::shared_ptr<VectorStorage> storage);
    ~HTTPServer();

    void start();
    void stop();

private:
    int port_;
    std::shared_ptr<VectorStorage> storage_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;

    void run_server();

    std::string handle_request(const std::string& method,
                               const std::string& path,
                               const std::string& body);

    std::string route_tenant_endpoints(const std::string& method,
                                        const std::string& path,
                                        const std::string& body);

    std::string handle_search(const std::string& body);
    std::string handle_batch_search(const std::string& body);
    std::string handle_search_with_filter(const std::string& body);
    std::string handle_insert(const std::string& body);
    std::string handle_batch_insert(const std::string& body);
    std::string handle_delete_vector(const std::string& collection, const std::string& id);
    std::string handle_get_vector(const std::string& collection, const std::string& id);
    std::string handle_update_vector(const std::string& collection, const std::string& id, const std::string& body);
    std::string handle_create_collection(const std::string& body);
    std::string handle_delete_collection(const std::string& name);
    std::string handle_list_collections();
    std::string handle_health();
    std::string handle_stats(const std::string& collection);
    std::string handle_index_stats(const std::string& collection);
    std::string handle_count(const std::string& collection);
    std::string handle_save(const std::string& collection);

    std::string handle_list_namespaces(const std::string& tenant_id);
    std::string handle_create_namespace(const std::string& tenant_id, const std::string& body);
    std::string handle_add_faq(const std::string& tenant_id, const std::string& ns, const std::string& body);
    std::string handle_bulk_faq(const std::string& tenant_id, const std::string& ns, const std::string& body);
    std::string handle_namespace_search(const std::string& tenant_id, const std::string& ns, const std::string& body);
    std::string handle_tenant_search(const std::string& tenant_id, const std::string& body);
    std::string handle_get_faq(const std::string& tenant_id, const std::string& ns, const std::string& faq_id);
    std::string handle_delete_faq(const std::string& tenant_id, const std::string& ns, const std::string& faq_id);
    std::string handle_update_faq(const std::string& tenant_id, const std::string& ns, const std::string& faq_id, const std::string& body);
    std::string handle_namespace_stats(const std::string& tenant_id, const std::string& ns);
    std::string handle_tenant_stats(const std::string& tenant_id);

    std::string json_response(int code, const std::string& body);
    std::string error_response(int code, const std::string& message);
};

}
