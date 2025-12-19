cmake_minimum_required(VERSION 3.20)
project(vector_service VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_AVX2 "Enable AVX2 SIMD" ON)
option(USE_AVX512 "Enable AVX-512 SIMD" OFF)
option(BUILD_TESTS "Build tests" ON)

if(USE_AVX512)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512dq -mavx512vl")
    add_compile_definitions(USE_AVX512)
elseif(USE_AVX2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
    add_compile_definitions(USE_AVX2)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)

# Find grpc_cpp_plugin
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found")
endif()
set(grpc_cpp_plugin_location ${GRPC_CPP_PLUGIN})

set(PROTO_FILES ${CMAKE_SOURCE_DIR}/proto/vector_service.proto)

add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/src/vector_service.pb.cc
        ${CMAKE_SOURCE_DIR}/src/vector_service.pb.h
        ${CMAKE_SOURCE_DIR}/src/vector_service.grpc.pb.cc
        ${CMAKE_SOURCE_DIR}/src/vector_service.grpc.pb.h
    COMMAND protobuf::protoc
        --cpp_out=${CMAKE_SOURCE_DIR}/src
        --grpc_out=${CMAKE_SOURCE_DIR}/src
        --plugin=protoc-gen-grpc=${grpc_cpp_plugin_location}
        -I${CMAKE_SOURCE_DIR}/proto
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
)

add_library(vector_core STATIC
    src/simd_ops.cpp
    src/hnsw_index.cpp
    src/vector_storage.cpp
    src/vector_service.pb.cc
    src/vector_service.grpc.pb.cc
)

target_include_directories(vector_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/usearch/include
    ${CMAKE_SOURCE_DIR}/deps/fp16/include
)

target_include_directories(vector_core PUBLIC ${GRPC_INCLUDE_DIRS})
target_link_libraries(vector_core PUBLIC
    Threads::Threads
    protobuf::libprotobuf
    ${GRPC_LIBRARIES}
)

add_executable(vector_server src/main.cpp src/grpc_server.cpp src/http_server.cpp src/http_router.cpp)
target_link_libraries(vector_server PRIVATE vector_core)

if(BUILD_TESTS)
    enable_testing()
    add_executable(test_simd tests/test_simd.cpp)
    target_link_libraries(test_simd PRIVATE vector_core)
    add_test(NAME simd_test COMMAND test_simd)

    add_executable(test_hnsw tests/test_hnsw.cpp)
    target_link_libraries(test_hnsw PRIVATE vector_core)
    add_test(NAME hnsw_test COMMAND test_hnsw)
endif()

install(TARGETS vector_server DESTINATION bin)
