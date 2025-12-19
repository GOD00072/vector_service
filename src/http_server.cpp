#include "http_server.hpp"
#include "http_router.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>
#include <algorithm>

namespace vectordb {

namespace {

std::string parse_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) return "";

    size_t start = json.find('"', colon);
    if (start == std::string::npos) return "";

    size_t end = json.find('"', start + 1);
    if (end == std::string::npos) return "";

    return json.substr(start + 1, end - start - 1);
}

int parse_json_int(const std::string& json, const std::string& key, int default_val = 0) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_val;

    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) return default_val;

    size_t num_start = colon + 1;
    while (num_start < json.length() && (json[num_start] == ' ' || json[num_start] == '\t')) {
        num_start++;
    }

    size_t num_end = num_start;
    while (num_end < json.length() && (json[num_end] >= '0' && json[num_end] <= '9')) {
        num_end++;
    }

    if (num_end > num_start) {
        return std::stoi(json.substr(num_start, num_end - num_start));
    }

    return default_val;
}

std::vector<float> parse_json_float_array(const std::string& json, const std::string& key) {
    std::vector<float> result;

    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) return result;

    size_t bracket_pos = json.find('[', key_pos + search_key.length());
    if (bracket_pos == std::string::npos) return result;

    size_t end_pos = json.find(']', bracket_pos);
    if (end_pos == std::string::npos) return result;

    result.reserve(4096);

    size_t pos = bracket_pos + 1;
    while (pos < end_pos) {
        while (pos < end_pos && (json[pos] == ' ' || json[pos] == ',' ||
               json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
            pos++;
        }
        if (pos >= end_pos) break;

        size_t num_start = pos;
        while (pos < end_pos && (json[pos] == '-' || json[pos] == '+' ||
               json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' ||
               (json[pos] >= '0' && json[pos] <= '9'))) {
            pos++;
        }

        if (pos > num_start) {
            try {
                result.push_back(std::stof(json.substr(num_start, pos - num_start)));
            } catch (...) {}
        }
    }

    return result;
}

std::string float_array_to_json(const std::vector<float>& arr) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0) oss << ",";
        oss << arr[i];
    }
    oss << "]";
    return oss.str();
}

std::string metadata_to_json(const std::unordered_map<std::string, std::string>& meta) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : meta) {
        if (!first) oss << ",";
        oss << "\"" << k << "\":\"" << v << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::string make_collection_name(const std::string& tenant_id, const std::string& ns) {
    return tenant_id + "__" + ns;
}

}

HTTPServer::HTTPServer(int port, std::shared_ptr<VectorStorage> storage)
    : port_(port), storage_(std::move(storage)) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::start() {
    running_ = true;
    server_thread_ = std::thread(&HTTPServer::run_server, this);
}

void HTTPServer::stop() {
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void HTTPServer::run_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int recv_buf_size = 16 * 1024 * 1024;
    int send_buf_size = 16 * 1024 * 1024;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
    setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << std::endl;
        close(server_fd);
        return;
    }

    if (listen(server_fd, 128) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        close(server_fd);
        return;
    }

    std::cout << "HTTP Server listening on port " << port_ << std::endl;
    std::cout << "Max payload: 500MB+ | Parallel search: 100K+" << std::endl;

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    constexpr size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);

    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) continue;

        setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));

        timeval client_tv{};
        client_tv.tv_sec = 300;
        client_tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &client_tv, sizeof(client_tv));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &client_tv, sizeof(client_tv));

        std::string request;
        ssize_t bytes;

        bytes = read(client_fd, buffer.data(), buffer.size() - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            request.assign(buffer.data(), bytes);
        }

        size_t content_length = 0;
        std::string cl_header = "Content-Length:";
        auto cl_pos = request.find(cl_header);
        if (cl_pos != std::string::npos) {
            auto cl_end = request.find("\r\n", cl_pos);
            std::string cl_val = request.substr(cl_pos + cl_header.length(), cl_end - cl_pos - cl_header.length());
            size_t start = cl_val.find_first_not_of(" \t");
            if (start != std::string::npos) {
                cl_val = cl_val.substr(start);
            }
            content_length = std::stoull(cl_val);
        }

        auto header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos && content_length > 0) {
            size_t header_size = header_end + 4;
            size_t body_received = request.length() - header_size;

            if (content_length > body_received) {
                request.reserve(header_size + content_length + 1);
            }

            while (body_received < content_length) {
                size_t remaining = content_length - body_received;
                size_t to_read = std::min(remaining, buffer.size() - 1);

                bytes = read(client_fd, buffer.data(), to_read);
                if (bytes <= 0) {
                    if (bytes == 0) break;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    break;
                }

                request.append(buffer.data(), bytes);
                body_received += bytes;

                if (content_length > 50 * 1024 * 1024 && body_received % (50 * 1024 * 1024) < static_cast<size_t>(bytes)) {
                    std::cout << "Receiving: " << (body_received / (1024 * 1024)) << "MB / "
                              << (content_length / (1024 * 1024)) << "MB" << std::endl;
                }
            }
        }

        if (!request.empty()) {
            std::string method, path;
            std::istringstream iss(request);
            iss >> method >> path;

            std::string body;
            auto body_pos = request.find("\r\n\r\n");
            if (body_pos != std::string::npos) {
                body = request.substr(body_pos + 4);
            }

            std::string response = handle_request(method, path, body);

            size_t total_sent = 0;
            while (total_sent < response.size()) {
                ssize_t sent = write(client_fd, response.c_str() + total_sent, response.size() - total_sent);
                if (sent <= 0) break;
                total_sent += sent;
            }
        }

        close(client_fd);
    }

    close(server_fd);
}

std::string HTTPServer::handle_request(const std::string& method,
                                        const std::string& path,
                                        const std::string& body) {
    try {
        if (method == "GET" && path == "/health") return handle_health();
        if (method == "GET" && path == "/collections") return handle_list_collections();
        if (method == "POST" && path == "/collections") return handle_create_collection(body);
        if (method == "POST" && path == "/search") return handle_search(body);
        if (method == "POST" && path == "/batch_search") return handle_batch_search(body);
        if (method == "POST" && path == "/insert") return handle_insert(body);
        if (method == "POST" && path == "/batch_insert") return handle_batch_insert(body);
        if (method == "POST" && path == "/search_with_filter") return handle_search_with_filter(body);
        if (method == "POST" && path == "/save") return handle_save(parse_json_string(body, "collection"));
        if (method == "POST" && path == "/save_all") return handle_save("");

        if (path.rfind("/collections/", 0) == 0) {
            std::string name = path.substr(13);
            if (method == "DELETE") return handle_delete_collection(name);
            if (method == "GET") return handle_stats(name);
        }

        if (path.rfind("/stats/", 0) == 0) {
            return handle_stats(path.substr(7));
        }

        if (path.rfind("/index/", 0) == 0) {
            return handle_index_stats(path.substr(7));
        }

        if (path.rfind("/count/", 0) == 0) {
            return handle_count(path.substr(7));
        }

        if (path.rfind("/vectors/", 0) == 0) {
            auto second_slash = path.find('/', 9);
            if (second_slash != std::string::npos) {
                std::string collection = path.substr(9, second_slash - 9);
                std::string id = path.substr(second_slash + 1);
                if (method == "GET") return handle_get_vector(collection, id);
                if (method == "PUT") return handle_update_vector(collection, id, body);
                if (method == "DELETE") return handle_delete_vector(collection, id);
            }
        }

        return route_tenant_endpoints(method, path, body);

    } catch (const std::exception& e) {
        return error_response(500, e.what());
    }
}

std::string HTTPServer::route_tenant_endpoints(const std::string& method,
                                                const std::string& path,
                                                const std::string& body) {
    if (path.rfind("/tenants/", 0) != 0) {
        return error_response(404, "Not found");
    }

    std::string rest = path.substr(9);
    size_t slash1 = rest.find('/');

    if (slash1 == std::string::npos) {
        return error_response(404, "Not found");
    }

    std::string tenant_id = rest.substr(0, slash1);
    std::string remaining = rest.substr(slash1 + 1);

    if (remaining == "namespaces") {
        if (method == "GET") return handle_list_namespaces(tenant_id);
        if (method == "POST") return handle_create_namespace(tenant_id, body);
    }

    if (remaining == "search") {
        if (method == "POST") return handle_tenant_search(tenant_id, body);
    }

    if (remaining == "stats") {
        if (method == "GET") return handle_tenant_stats(tenant_id);
    }

    size_t slash2 = remaining.find('/');
    if (slash2 == std::string::npos) {
        return error_response(404, "Not found");
    }

    std::string ns = remaining.substr(0, slash2);
    std::string action = remaining.substr(slash2 + 1);

    if (action == "faq") {
        if (method == "POST") return handle_add_faq(tenant_id, ns, body);
    }

    if (action == "faq/bulk") {
        if (method == "POST") return handle_bulk_faq(tenant_id, ns, body);
    }

    if (action == "search") {
        if (method == "POST") return handle_namespace_search(tenant_id, ns, body);
    }

    if (action == "stats") {
        if (method == "GET") return handle_namespace_stats(tenant_id, ns);
    }

    if (action.rfind("faq/", 0) == 0) {
        std::string faq_id = action.substr(4);
        if (method == "GET") return handle_get_faq(tenant_id, ns, faq_id);
        if (method == "PUT") return handle_update_faq(tenant_id, ns, faq_id, body);
        if (method == "DELETE") return handle_delete_faq(tenant_id, ns, faq_id);
    }

    return error_response(404, "Not found");
}

std::string HTTPServer::handle_health() {
    return json_response(200, R"({"healthy":true,"version":"1.0.0"})");
}

std::string HTTPServer::handle_list_collections() {
    auto names = storage_->list_collections();

    std::ostringstream oss;
    oss << "{\"collections\":[";
    bool first = true;
    for (const auto& name : names) {
        auto stats = storage_->get_stats(name);
        if (stats) {
            if (!first) oss << ",";
            oss << "{\"name\":\"" << name << "\","
                << "\"dimension\":" << stats->dimension << ","
                << "\"count\":" << stats->vector_count << ","
                << "\"metric\":\"" << stats->metric << "\"}";
            first = false;
        }
    }
    oss << "]}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_create_collection(const std::string& body) {
    std::string name = parse_json_string(body, "name");
    int dimension = parse_json_int(body, "dimension");
    std::string metric = parse_json_string(body, "metric");
    int m = parse_json_int(body, "m", 16);
    int ef_construction = parse_json_int(body, "ef_construction", 200);
    int ef_search = parse_json_int(body, "ef_search", 50);

    CollectionConfig config;
    config.name = name;
    config.dimension = dimension;

    if (metric == "euclidean") {
        config.metric = DistanceMetric::Euclidean;
    } else if (metric == "dot_product") {
        config.metric = DistanceMetric::DotProduct;
    } else {
        config.metric = DistanceMetric::Cosine;
    }

    config.hnsw_config.M = m;
    config.hnsw_config.ef_construction = ef_construction;
    config.hnsw_config.ef_search = ef_search;
    config.hnsw_config.metric = config.metric;

    bool success = storage_->create_collection(config);

    if (success) {
        return json_response(200, R"({"success":true,"message":"Collection created"})");
    }
    return json_response(400, R"({"success":false,"message":"Collection already exists"})");
}

std::string HTTPServer::handle_delete_collection(const std::string& name) {
    bool success = storage_->delete_collection(name);

    if (success) {
        return json_response(200, R"({"success":true})");
    }
    return json_response(404, R"({"success":false,"message":"Collection not found"})");
}

std::string HTTPServer::handle_stats(const std::string& collection) {
    auto stats = storage_->get_stats(collection);

    if (stats) {
        std::ostringstream oss;
        oss << "{\"total_vectors\":" << stats->vector_count
            << ",\"memory_usage_bytes\":" << stats->memory_usage
            << ",\"dimension\":" << stats->dimension
            << ",\"metric\":\"" << stats->metric << "\"}";
        return json_response(200, oss.str());
    }
    return error_response(404, "Collection not found");
}

std::string HTTPServer::handle_search(const std::string& body) {
    std::string collection = parse_json_string(body, "collection");
    auto query = parse_json_float_array(body, "query");
    int top_k = parse_json_int(body, "top_k", 10);

    auto start = std::chrono::high_resolution_clock::now();
    auto results = storage_->search(collection, query, top_k);
    auto end = std::chrono::high_resolution_clock::now();
    float time_ms = std::chrono::duration<float, std::milli>(end - start).count();

    std::ostringstream oss;
    oss << "{\"results\":[";
    bool first = true;
    for (const auto& r : results) {
        if (!first) oss << ",";
        oss << "{\"id\":\"" << r.id << "\",\"score\":" << r.distance;
        if (r.data) {
            oss << ",\"metadata\":" << metadata_to_json(r.data->metadata);
        }
        oss << "}";
        first = false;
    }
    oss << "],\"search_time_ms\":" << time_ms << "}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_batch_search(const std::string& body) {
    std::string collection = parse_json_string(body, "collection");
    int top_k = parse_json_int(body, "top_k", 10);

    std::vector<std::vector<float>> queries;

    size_t queries_pos = body.find("\"queries\"");
    if (queries_pos != std::string::npos) {
        size_t arr_start = body.find('[', queries_pos);
        if (arr_start != std::string::npos) {
            size_t pos = arr_start + 1;
            while (pos < body.length()) {
                size_t values_pos = body.find("\"values\"", pos);
                if (values_pos == std::string::npos) break;

                auto q = parse_json_float_array(body.substr(values_pos), "values");
                if (!q.empty()) {
                    queries.push_back(std::move(q));
                }

                size_t next_obj = body.find('}', values_pos);
                if (next_obj == std::string::npos) break;
                pos = next_obj + 1;
            }
        }
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto all_results = storage_->batch_search(collection, queries, top_k);
    auto end_time = std::chrono::high_resolution_clock::now();
    float time_ms = std::chrono::duration<float, std::milli>(end_time - start).count();

    std::ostringstream oss;
    oss << "{\"results\":[";
    bool first_batch = true;
    for (const auto& results : all_results) {
        if (!first_batch) oss << ",";
        oss << "{\"results\":[";
        bool first = true;
        for (const auto& r : results) {
            if (!first) oss << ",";
            oss << "{\"id\":\"" << r.id << "\",\"score\":" << r.distance << "}";
            first = false;
        }
        oss << "]}";
        first_batch = false;
    }
    oss << "],\"total_queries\":" << queries.size()
        << ",\"total_time_ms\":" << time_ms
        << ",\"avg_time_per_query_ms\":" << (queries.empty() ? 0 : time_ms / queries.size()) << "}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_insert(const std::string& body) {
    std::string collection = parse_json_string(body, "collection");
    std::string id;
    std::vector<float> values;

    size_t vec_pos = body.find("\"vector\"");
    if (vec_pos != std::string::npos) {
        size_t brace_pos = body.find('{', vec_pos);
        if (brace_pos != std::string::npos) {
            size_t id_pos = body.find("\"id\"", brace_pos);
            if (id_pos != std::string::npos) {
                size_t colon = body.find(':', id_pos);
                if (colon != std::string::npos) {
                    size_t quote_start = body.find('"', colon);
                    if (quote_start != std::string::npos) {
                        size_t quote_end = body.find('"', quote_start + 1);
                        if (quote_end != std::string::npos) {
                            id = body.substr(quote_start + 1, quote_end - quote_start - 1);
                        }
                    }
                }
            }
            values = parse_json_float_array(body, "values");
        }
    }

    std::string result_id = storage_->insert(collection, values, id);

    std::ostringstream oss;
    oss << "{\"success\":true,\"id\":\"" << result_id << "\"}";
    return json_response(200, oss.str());
}

std::string HTTPServer::handle_batch_insert(const std::string& body) {
    std::string collection = parse_json_string(body, "collection");
    std::vector<VectorData> vectors;

    size_t vectors_pos = body.find("\"vectors\"");
    if (vectors_pos == std::string::npos) {
        return error_response(400, "Missing vectors array");
    }

    size_t arr_start = body.find('[', vectors_pos);
    if (arr_start == std::string::npos) {
        return error_response(400, "Invalid vectors format");
    }

    size_t pos = arr_start + 1;
    size_t body_len = body.length();

    while (pos < body_len) {
        while (pos < body_len && (body[pos] == ' ' || body[pos] == '\n' ||
               body[pos] == '\r' || body[pos] == '\t' || body[pos] == ',')) {
            pos++;
        }

        if (pos >= body_len || body[pos] == ']') break;

        if (body[pos] != '{') {
            pos++;
            continue;
        }

        size_t obj_start = pos;
        int brace_count = 1;
        pos++;

        while (pos < body_len && brace_count > 0) {
            if (body[pos] == '{') brace_count++;
            else if (body[pos] == '}') brace_count--;
            else if (body[pos] == '"') {
                pos++;
                while (pos < body_len && body[pos] != '"') {
                    if (body[pos] == '\\') pos++;
                    pos++;
                }
            }
            pos++;
        }

        if (brace_count == 0) {
            std::string obj = body.substr(obj_start, pos - obj_start);

            VectorData v;

            size_t id_pos = obj.find("\"id\"");
            if (id_pos != std::string::npos) {
                size_t colon = obj.find(':', id_pos);
                if (colon != std::string::npos) {
                    size_t q1 = obj.find('"', colon);
                    if (q1 != std::string::npos) {
                        size_t q2 = obj.find('"', q1 + 1);
                        if (q2 != std::string::npos) {
                            v.id = obj.substr(q1 + 1, q2 - q1 - 1);
                        }
                    }
                }
            }

            v.values = parse_json_float_array(obj, "values");

            size_t meta_pos = obj.find("\"metadata\"");
            if (meta_pos != std::string::npos) {
                size_t meta_start = obj.find('{', meta_pos);
                size_t meta_end = obj.find('}', meta_start);
                if (meta_start != std::string::npos && meta_end != std::string::npos) {
                    std::string meta_str = obj.substr(meta_start, meta_end - meta_start + 1);
                    size_t mpos = 1;
                    while (mpos < meta_str.length()) {
                        size_t key_start = meta_str.find('"', mpos);
                        if (key_start == std::string::npos) break;
                        size_t key_end = meta_str.find('"', key_start + 1);
                        if (key_end == std::string::npos) break;
                        std::string key = meta_str.substr(key_start + 1, key_end - key_start - 1);

                        size_t val_start = meta_str.find('"', key_end + 1);
                        if (val_start == std::string::npos) break;
                        size_t val_end = meta_str.find('"', val_start + 1);
                        if (val_end == std::string::npos) break;
                        std::string val = meta_str.substr(val_start + 1, val_end - val_start - 1);

                        v.metadata[key] = val;
                        mpos = val_end + 1;
                    }
                }
            }

            if (!v.values.empty()) {
                vectors.push_back(std::move(v));
            }
        }
    }

    size_t count = storage_->batch_insert(collection, vectors);

    std::ostringstream oss;
    oss << "{\"success\":true,\"inserted_count\":" << count << ",\"total_received\":" << vectors.size() << "}";
    return json_response(200, oss.str());
}

std::string HTTPServer::handle_delete_vector(const std::string& collection, const std::string& id) {
    bool success = storage_->remove(collection, id);

    if (success) {
        return json_response(200, R"({"success":true})");
    }
    return json_response(404, R"({"success":false,"message":"Vector not found"})");
}

std::string HTTPServer::handle_get_vector(const std::string& collection, const std::string& id) {
    auto* data = storage_->get(collection, id);

    if (data) {
        std::ostringstream oss;
        oss << "{\"id\":\"" << data->id << "\",\"values\":" << float_array_to_json(data->values)
            << ",\"metadata\":" << metadata_to_json(data->metadata) << "}";
        return json_response(200, oss.str());
    }
    return error_response(404, "Vector not found");
}

std::string HTTPServer::handle_update_vector(const std::string& collection,
                                              const std::string& id,
                                              const std::string& body) {
    auto values = parse_json_float_array(body, "values");

    std::unordered_map<std::string, std::string> metadata;
    size_t meta_pos = body.find("\"metadata\"");
    if (meta_pos != std::string::npos) {
        size_t meta_start = body.find('{', meta_pos);
        size_t meta_end = body.find('}', meta_start);
        if (meta_start != std::string::npos && meta_end != std::string::npos) {
            std::string meta_str = body.substr(meta_start, meta_end - meta_start + 1);
            size_t mpos = 1;
            while (mpos < meta_str.length()) {
                size_t key_start = meta_str.find('"', mpos);
                if (key_start == std::string::npos) break;
                size_t key_end = meta_str.find('"', key_start + 1);
                if (key_end == std::string::npos) break;
                std::string key = meta_str.substr(key_start + 1, key_end - key_start - 1);

                size_t val_start = meta_str.find('"', key_end + 1);
                if (val_start == std::string::npos) break;
                size_t val_end = meta_str.find('"', val_start + 1);
                if (val_end == std::string::npos) break;
                std::string val = meta_str.substr(val_start + 1, val_end - val_start - 1);

                metadata[key] = val;
                mpos = val_end + 1;
            }
        }
    }

    bool removed = storage_->remove(collection, id);
    if (!removed) {
        return error_response(404, "Vector not found");
    }

    std::string new_id = storage_->insert(collection, values, id, metadata);

    std::ostringstream oss;
    oss << "{\"success\":true,\"id\":\"" << new_id << "\"}";
    return json_response(200, oss.str());
}

std::string HTTPServer::handle_search_with_filter(const std::string& body) {
    std::string collection = parse_json_string(body, "collection");
    auto query = parse_json_float_array(body, "query");
    int top_k = parse_json_int(body, "top_k", 10);
    int ef = parse_json_int(body, "ef", 0);

    std::unordered_map<std::string, std::string> filters;
    size_t filter_pos = body.find("\"filter\"");
    if (filter_pos != std::string::npos) {
        size_t filter_start = body.find('{', filter_pos);
        size_t filter_end = body.find('}', filter_start);
        if (filter_start != std::string::npos && filter_end != std::string::npos) {
            std::string filter_str = body.substr(filter_start, filter_end - filter_start + 1);
            size_t fpos = 1;
            while (fpos < filter_str.length()) {
                size_t key_start = filter_str.find('"', fpos);
                if (key_start == std::string::npos) break;
                size_t key_end = filter_str.find('"', key_start + 1);
                if (key_end == std::string::npos) break;
                std::string key = filter_str.substr(key_start + 1, key_end - key_start - 1);

                size_t val_start = filter_str.find('"', key_end + 1);
                if (val_start == std::string::npos) break;
                size_t val_end = filter_str.find('"', val_start + 1);
                if (val_end == std::string::npos) break;
                std::string val = filter_str.substr(val_start + 1, val_end - val_start - 1);

                filters[key] = val;
                fpos = val_end + 1;
            }
        }
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto results = storage_->search(collection, query, top_k * 3, ef);
    auto end_time = std::chrono::high_resolution_clock::now();
    float time_ms = std::chrono::duration<float, std::milli>(end_time - start).count();

    std::vector<HNSWResult> filtered;
    for (const auto& r : results) {
        if (filtered.size() >= static_cast<size_t>(top_k)) break;

        if (filters.empty()) {
            filtered.push_back(r);
            continue;
        }

        if (r.data) {
            bool match_all = true;
            for (const auto& [fk, fv] : filters) {
                auto it = r.data->metadata.find(fk);
                if (it == r.data->metadata.end() || it->second != fv) {
                    match_all = false;
                    break;
                }
            }
            if (match_all) {
                filtered.push_back(r);
            }
        }
    }

    std::ostringstream oss;
    oss << "{\"results\":[";
    bool first = true;
    for (const auto& r : filtered) {
        if (!first) oss << ",";
        oss << "{\"id\":\"" << r.id << "\",\"score\":" << r.distance;
        if (r.data) {
            oss << ",\"metadata\":" << metadata_to_json(r.data->metadata);
        }
        oss << "}";
        first = false;
    }
    oss << "],\"search_time_ms\":" << time_ms << ",\"total_candidates\":" << results.size() << "}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_index_stats(const std::string& collection) {
    auto stats = storage_->get_stats(collection);

    if (stats) {
        std::ostringstream oss;
        oss << "{\"collection\":\"" << collection << "\","
            << "\"total_vectors\":" << stats->vector_count << ","
            << "\"dimension\":" << stats->dimension << ","
            << "\"memory_usage_bytes\":" << stats->memory_usage << ","
            << "\"memory_usage_mb\":" << (stats->memory_usage / (1024.0 * 1024.0)) << ","
            << "\"metric\":\"" << stats->metric << "\","
            << "\"bytes_per_vector\":" << (stats->vector_count > 0 ? stats->memory_usage / stats->vector_count : 0)
            << "}";
        return json_response(200, oss.str());
    }
    return error_response(404, "Collection not found");
}

std::string HTTPServer::handle_save(const std::string& collection) {
    if (collection.empty()) {
        bool success = storage_->save_all();
        if (success) {
            return json_response(200, R"({"success":true,"message":"All collections saved"})");
        }
        return error_response(500, "Failed to save collections");
    }

    auto stats = storage_->get_stats(collection);
    if (!stats) {
        return error_response(404, "Collection not found");
    }

    bool success = storage_->save_all();
    if (success) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"collection\":\"" << collection << "\"}";
        return json_response(200, oss.str());
    }
    return error_response(500, "Failed to save collection");
}

std::string HTTPServer::handle_count(const std::string& collection) {
    auto stats = storage_->get_stats(collection);

    if (stats) {
        std::ostringstream oss;
        oss << "{\"collection\":\"" << collection << "\",\"count\":" << stats->vector_count << "}";
        return json_response(200, oss.str());
    }
    return error_response(404, "Collection not found");
}

std::string HTTPServer::handle_list_namespaces(const std::string& tenant_id) {
    auto collections = storage_->list_collections();
    std::string prefix = tenant_id + "__";

    std::ostringstream oss;
    oss << "{\"tenant_id\":\"" << tenant_id << "\",\"namespaces\":[";
    bool first = true;
    for (const auto& col : collections) {
        if (col.rfind(prefix, 0) == 0) {
            if (!first) oss << ",";
            std::string ns = col.substr(prefix.length());
            auto stats = storage_->get_stats(col);
            oss << "{\"name\":\"" << ns << "\"";
            if (stats) {
                oss << ",\"vector_count\":" << stats->vector_count;
                oss << ",\"dimension\":" << stats->dimension;
            }
            oss << "}";
            first = false;
        }
    }
    oss << "]}";
    return json_response(200, oss.str());
}

std::string HTTPServer::handle_create_namespace(const std::string& tenant_id, const std::string& body) {
    std::string ns = parse_json_string(body, "namespace");
    int dimension = parse_json_int(body, "dimension", 384);
    std::string metric = parse_json_string(body, "metric");
    if (metric.empty()) metric = "cosine";

    std::string col_name = make_collection_name(tenant_id, ns);

    CollectionConfig config;
    config.name = col_name;
    config.dimension = dimension;
    if (metric == "euclidean") config.metric = DistanceMetric::Euclidean;
    else if (metric == "dot_product") config.metric = DistanceMetric::DotProduct;
    else config.metric = DistanceMetric::Cosine;

    bool success = storage_->create_collection(config);
    if (success) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"tenant_id\":\"" << tenant_id
            << "\",\"namespace\":\"" << ns
            << "\",\"dimension\":" << dimension << "}";
        return json_response(201, oss.str());
    }
    return error_response(409, "Namespace already exists");
}

std::string HTTPServer::handle_add_faq(const std::string& tenant_id, const std::string& ns, const std::string& body) {
    std::string col_name = make_collection_name(tenant_id, ns);

    if (!storage_->collection_exists(col_name)) {
        return error_response(404, "Namespace not found");
    }

    std::string faq_id = parse_json_string(body, "id");
    std::string question = parse_json_string(body, "question");
    std::string answer = parse_json_string(body, "answer");
    std::string category = parse_json_string(body, "category");
    auto values = parse_json_float_array(body, "vector");

    if (values.empty()) {
        return error_response(400, "Vector is required");
    }

    std::unordered_map<std::string, std::string> metadata;
    metadata["question"] = question;
    metadata["answer"] = answer;
    metadata["category"] = category;
    metadata["type"] = "faq";
    metadata["tenant_id"] = tenant_id;
    metadata["namespace"] = ns;

    std::string result_id = storage_->insert(col_name, values, faq_id, metadata);

    std::ostringstream oss;
    oss << "{\"success\":true,\"id\":\"" << result_id
        << "\",\"tenant_id\":\"" << tenant_id
        << "\",\"namespace\":\"" << ns << "\"}";
    return json_response(201, oss.str());
}

std::string HTTPServer::handle_bulk_faq(const std::string& tenant_id, const std::string& ns, const std::string& body) {
    std::string col_name = make_collection_name(tenant_id, ns);

    if (!storage_->collection_exists(col_name)) {
        return error_response(404, "Namespace not found");
    }

    std::vector<VectorData> vectors;

    size_t items_pos = body.find("\"items\"");
    if (items_pos == std::string::npos) {
        items_pos = body.find('[');
    } else {
        items_pos = body.find('[', items_pos);
    }

    if (items_pos != std::string::npos) {
        size_t pos = items_pos + 1;
        while (pos < body.length()) {
            while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\n' ||
                   body[pos] == '\r' || body[pos] == '\t' || body[pos] == ',')) {
                pos++;
            }

            if (pos >= body.length() || body[pos] == ']') break;

            if (body[pos] != '{') {
                pos++;
                continue;
            }

            size_t obj_start = pos;
            int brace_count = 1;
            pos++;

            while (pos < body.length() && brace_count > 0) {
                if (body[pos] == '{') brace_count++;
                else if (body[pos] == '}') brace_count--;
                else if (body[pos] == '"') {
                    pos++;
                    while (pos < body.length() && body[pos] != '"') {
                        if (body[pos] == '\\') pos++;
                        pos++;
                    }
                }
                pos++;
            }

            if (brace_count == 0) {
                std::string item = body.substr(obj_start, pos - obj_start);
                VectorData v;
                v.id = parse_json_string(item, "id");
                v.values = parse_json_float_array(item, "vector");
                v.metadata["question"] = parse_json_string(item, "question");
                v.metadata["answer"] = parse_json_string(item, "answer");
                v.metadata["category"] = parse_json_string(item, "category");
                v.metadata["type"] = "faq";
                v.metadata["tenant_id"] = tenant_id;
                v.metadata["namespace"] = ns;

                if (!v.values.empty()) {
                    vectors.push_back(std::move(v));
                }
            }
        }
    }

    size_t count = storage_->batch_insert(col_name, vectors);

    std::ostringstream oss;
    oss << "{\"success\":true,\"inserted_count\":" << count
        << ",\"tenant_id\":\"" << tenant_id
        << "\",\"namespace\":\"" << ns << "\"}";
    return json_response(201, oss.str());
}

std::string HTTPServer::handle_namespace_search(const std::string& tenant_id, const std::string& ns, const std::string& body) {
    std::string col_name = make_collection_name(tenant_id, ns);

    if (!storage_->collection_exists(col_name)) {
        return error_response(404, "Namespace not found");
    }

    auto query = parse_json_float_array(body, "query");
    int top_k = parse_json_int(body, "top_k", 5);
    std::string category = parse_json_string(body, "category");

    auto start = std::chrono::high_resolution_clock::now();

    size_t search_k = category.empty() ? top_k : top_k * 3;
    auto results = storage_->search(col_name, query, search_k);

    auto end_time = std::chrono::high_resolution_clock::now();
    float time_ms = std::chrono::duration<float, std::milli>(end_time - start).count();

    std::ostringstream oss;
    oss << "{\"results\":[";
    bool first = true;
    int count = 0;
    for (const auto& r : results) {
        if (count >= top_k) break;

        if (!category.empty() && r.data) {
            auto cat_it = r.data->metadata.find("category");
            if (cat_it == r.data->metadata.end() || cat_it->second != category) {
                continue;
            }
        }

        if (!first) oss << ",";
        oss << "{\"id\":\"" << r.id << "\",\"score\":" << r.distance;
        if (r.data) {
            auto q_it = r.data->metadata.find("question");
            auto a_it = r.data->metadata.find("answer");
            auto c_it = r.data->metadata.find("category");
            if (q_it != r.data->metadata.end()) {
                oss << ",\"question\":\"" << q_it->second << "\"";
            }
            if (a_it != r.data->metadata.end()) {
                oss << ",\"answer\":\"" << a_it->second << "\"";
            }
            if (c_it != r.data->metadata.end()) {
                oss << ",\"category\":\"" << c_it->second << "\"";
            }
        }
        oss << "}";
        first = false;
        count++;
    }
    oss << "],\"search_time_ms\":" << time_ms
        << ",\"tenant_id\":\"" << tenant_id
        << "\",\"namespace\":\"" << ns << "\"}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_tenant_search(const std::string& tenant_id, const std::string& body) {
    auto query = parse_json_float_array(body, "query");
    int top_k = parse_json_int(body, "top_k", 5);
    std::string category = parse_json_string(body, "category");

    std::vector<std::string> namespaces;
    size_t ns_pos = body.find("\"namespaces\"");
    if (ns_pos != std::string::npos) {
        size_t arr_start = body.find('[', ns_pos);
        if (arr_start != std::string::npos) {
            size_t arr_end = body.find(']', arr_start);
            std::string ns_str = body.substr(arr_start, arr_end - arr_start);
            size_t pos = 0;
            while ((pos = ns_str.find('"', pos)) != std::string::npos) {
                size_t end = ns_str.find('"', pos + 1);
                if (end != std::string::npos) {
                    namespaces.push_back(ns_str.substr(pos + 1, end - pos - 1));
                    pos = end + 1;
                } else break;
            }
        }
    }

    if (namespaces.empty()) {
        auto collections = storage_->list_collections();
        std::string prefix = tenant_id + "__";
        for (const auto& col : collections) {
            if (col.rfind(prefix, 0) == 0) {
                namespaces.push_back(col.substr(prefix.length()));
            }
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::tuple<std::string, float, const VectorData*>> all_results;

    for (const auto& ns : namespaces) {
        std::string col_name = make_collection_name(tenant_id, ns);
        if (!storage_->collection_exists(col_name)) continue;

        auto results = storage_->search(col_name, query, top_k * 2);
        for (const auto& r : results) {
            all_results.emplace_back(r.id, r.distance, r.data);
        }
    }

    std::sort(all_results.begin(), all_results.end(),
              [](const auto& a, const auto& b) { return std::get<1>(a) < std::get<1>(b); });

    auto end_time = std::chrono::high_resolution_clock::now();
    float time_ms = std::chrono::duration<float, std::milli>(end_time - start).count();

    std::ostringstream oss;
    oss << "{\"results\":[";
    bool first = true;
    int count = 0;
    for (const auto& [id, score, data] : all_results) {
        if (count >= top_k) break;

        if (!category.empty() && data) {
            auto cat_it = data->metadata.find("category");
            if (cat_it == data->metadata.end() || cat_it->second != category) {
                continue;
            }
        }

        if (!first) oss << ",";
        oss << "{\"id\":\"" << id << "\",\"score\":" << score;
        if (data) {
            auto q_it = data->metadata.find("question");
            auto a_it = data->metadata.find("answer");
            auto c_it = data->metadata.find("category");
            auto ns_it = data->metadata.find("namespace");
            if (q_it != data->metadata.end()) {
                oss << ",\"question\":\"" << q_it->second << "\"";
            }
            if (a_it != data->metadata.end()) {
                oss << ",\"answer\":\"" << a_it->second << "\"";
            }
            if (c_it != data->metadata.end()) {
                oss << ",\"category\":\"" << c_it->second << "\"";
            }
            if (ns_it != data->metadata.end()) {
                oss << ",\"namespace\":\"" << ns_it->second << "\"";
            }
        }
        oss << "}";
        first = false;
        count++;
    }
    oss << "],\"search_time_ms\":" << time_ms
        << ",\"tenant_id\":\"" << tenant_id
        << "\",\"namespaces_searched\":" << namespaces.size() << "}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_get_faq(const std::string& tenant_id, const std::string& ns, const std::string& faq_id) {
    std::string col_name = make_collection_name(tenant_id, ns);

    auto* data = storage_->get(col_name, faq_id);
    if (!data) {
        return error_response(404, "FAQ not found");
    }

    std::ostringstream oss;
    oss << "{\"id\":\"" << data->id << "\"";

    auto q_it = data->metadata.find("question");
    auto a_it = data->metadata.find("answer");
    auto c_it = data->metadata.find("category");

    if (q_it != data->metadata.end()) oss << ",\"question\":\"" << q_it->second << "\"";
    if (a_it != data->metadata.end()) oss << ",\"answer\":\"" << a_it->second << "\"";
    if (c_it != data->metadata.end()) oss << ",\"category\":\"" << c_it->second << "\"";

    oss << ",\"vector\":" << float_array_to_json(data->values);
    oss << ",\"tenant_id\":\"" << tenant_id << "\",\"namespace\":\"" << ns << "\"}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_delete_faq(const std::string& tenant_id, const std::string& ns, const std::string& faq_id) {
    std::string col_name = make_collection_name(tenant_id, ns);

    bool success = storage_->remove(col_name, faq_id);
    if (success) {
        return json_response(200, R"({"success":true})");
    }
    return error_response(404, "FAQ not found");
}

std::string HTTPServer::handle_update_faq(const std::string& tenant_id, const std::string& ns,
                                           const std::string& faq_id, const std::string& body) {
    std::string col_name = make_collection_name(tenant_id, ns);

    auto* existing = storage_->get(col_name, faq_id);
    if (!existing) {
        return error_response(404, "FAQ not found");
    }

    std::string question = parse_json_string(body, "question");
    std::string answer = parse_json_string(body, "answer");
    std::string category = parse_json_string(body, "category");
    auto values = parse_json_float_array(body, "vector");

    if (values.empty()) {
        values = existing->values;
    }

    storage_->remove(col_name, faq_id);

    std::unordered_map<std::string, std::string> metadata;
    metadata["question"] = question.empty() ? existing->metadata.at("question") : question;
    metadata["answer"] = answer.empty() ? existing->metadata.at("answer") : answer;
    metadata["category"] = category.empty() ? existing->metadata.at("category") : category;
    metadata["type"] = "faq";
    metadata["tenant_id"] = tenant_id;
    metadata["namespace"] = ns;

    std::string new_id = storage_->insert(col_name, values, faq_id, metadata);

    std::ostringstream oss;
    oss << "{\"success\":true,\"id\":\"" << new_id << "\"}";
    return json_response(200, oss.str());
}

std::string HTTPServer::handle_namespace_stats(const std::string& tenant_id, const std::string& ns) {
    std::string col_name = make_collection_name(tenant_id, ns);
    auto stats = storage_->get_stats(col_name);

    if (!stats) {
        return error_response(404, "Namespace not found");
    }

    std::ostringstream oss;
    oss << "{\"tenant_id\":\"" << tenant_id
        << "\",\"namespace\":\"" << ns
        << "\",\"vector_count\":" << stats->vector_count
        << ",\"dimension\":" << stats->dimension
        << ",\"memory_usage_bytes\":" << stats->memory_usage
        << ",\"metric\":\"" << stats->metric << "\"}";

    return json_response(200, oss.str());
}

std::string HTTPServer::handle_tenant_stats(const std::string& tenant_id) {
    auto collections = storage_->list_collections();
    std::string prefix = tenant_id + "__";

    size_t total_vectors = 0;
    size_t total_memory = 0;
    int namespace_count = 0;

    std::ostringstream ns_oss;
    ns_oss << "[";
    bool first = true;

    for (const auto& col : collections) {
        if (col.rfind(prefix, 0) == 0) {
            auto stats = storage_->get_stats(col);
            if (stats) {
                total_vectors += stats->vector_count;
                total_memory += stats->memory_usage;
                namespace_count++;

                if (!first) ns_oss << ",";
                std::string ns = col.substr(prefix.length());
                ns_oss << "{\"name\":\"" << ns
                       << "\",\"vector_count\":" << stats->vector_count << "}";
                first = false;
            }
        }
    }
    ns_oss << "]";

    std::ostringstream oss;
    oss << "{\"tenant_id\":\"" << tenant_id
        << "\",\"namespace_count\":" << namespace_count
        << ",\"total_vectors\":" << total_vectors
        << ",\"total_memory_bytes\":" << total_memory
        << ",\"namespaces\":" << ns_oss.str() << "}";

    return json_response(200, oss.str());
}

std::string HTTPServer::json_response(int code, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " OK\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string HTTPServer::error_response(int code, const std::string& message) {
    std::ostringstream body;
    body << "{\"error\":\"" << message << "\"}";
    return json_response(code, body.str());
}

}
