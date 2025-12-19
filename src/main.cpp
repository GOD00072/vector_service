#include <iostream>
#include <csignal>
#include <memory>
#include <string>
#include <cstdlib>
#include "grpc_server.hpp"
#include "http_server.hpp"
#include "vector_storage.hpp"

namespace {
    std::unique_ptr<vectordb::GRPCServer> g_grpc_server;
    std::unique_ptr<vectordb::HTTPServer> g_http_server;
}

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_http_server) {
        g_http_server->stop();
    }
    if (g_grpc_server) {
        g_grpc_server->shutdown();
    }
}

int main(int argc, char* argv[]) {
    std::string grpc_address = "0.0.0.0:50051";
    int http_port = 50052;
    std::string data_dir = "./data";

    if (const char* env_port = std::getenv("VECTOR_PORT")) {
        grpc_address = std::string("0.0.0.0:") + env_port;
    }

    if (const char* env_http = std::getenv("VECTOR_HTTP_PORT")) {
        http_port = std::atoi(env_http);
    }

    if (const char* env_data = std::getenv("VECTOR_DATA_DIR")) {
        data_dir = env_data;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            grpc_address = std::string("0.0.0.0:") + argv[++i];
        } else if (arg == "--http-port" && i + 1 < argc) {
            http_port = std::atoi(argv[++i]);
        } else if (arg == "--data" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port PORT       gRPC port (default: 50051)\n"
                      << "  --http-port PORT  HTTP port (default: 50052)\n"
                      << "  --data DIR        Data directory (default: ./data)\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=================================\n";
    std::cout << "  Vector Service v1.0.0\n";
    std::cout << "  C++ HNSW with SIMD\n";
    std::cout << "=================================\n";
    std::cout << "gRPC: " << grpc_address << "\n";
    std::cout << "HTTP: 0.0.0.0:" << http_port << "\n";
    std::cout << "Data: " << data_dir << "\n";

#if defined(USE_AVX512)
    std::cout << "SIMD: AVX-512 enabled\n";
#elif defined(USE_AVX2)
    std::cout << "SIMD: AVX2 enabled\n";
#else
    std::cout << "SIMD: Scalar fallback\n";
#endif

    std::cout << "=================================\n";

    try {
        auto storage = std::make_shared<vectordb::VectorStorage>(data_dir);

        g_http_server = std::make_unique<vectordb::HTTPServer>(http_port, storage);
        g_http_server->start();

        g_grpc_server = std::make_unique<vectordb::GRPCServer>(grpc_address, storage);
        g_grpc_server->run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server stopped." << std::endl;
    return 0;
}
