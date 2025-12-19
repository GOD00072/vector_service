FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    libgrpc-dev \
    protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY proto/ proto/
COPY include/ include/
COPY src/ src/
COPY tests/ tests/
COPY deps/ deps/

RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DUSE_AVX2=ON \
          -DBUILD_TESTS=OFF \
          .. && \
    make -j$(nproc)

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libprotobuf32 \
    libgrpc++1.51 \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/vector_server /usr/local/bin/

RUN mkdir -p /data

ENV VECTOR_PORT=50051
ENV VECTOR_HTTP_PORT=50052
ENV VECTOR_DATA_DIR=/data

EXPOSE 50051 50052

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:50052/health || exit 1

CMD ["vector_server"]
