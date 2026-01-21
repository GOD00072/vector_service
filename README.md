# Vector Service

<p align="center">
  <strong>ระบบฐานข้อมูลเวกเตอร์ประสิทธิภาพสูงสำหรับ AI/ML Applications</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-20-blue.svg" alt="C++20">
  <img src="https://img.shields.io/badge/SIMD-AVX2%20|%20AVX512-green.svg" alt="SIMD">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License">
</p>

---

## ภาพรวม

**Vector Service** คือระบบฐานข้อมูลเวกเตอร์ที่ออกแบบมาเพื่อรองรับการค้นหาความคล้ายคลึง (Similarity Search) ด้วยความเร็วสูง เหมาะสำหรับ:

- **Semantic Search** - ค้นหาข้อความที่มีความหมายใกล้เคียง
- **Recommendation Systems** - ระบบแนะนำสินค้า/เนื้อหา
- **RAG (Retrieval-Augmented Generation)** - ดึงข้อมูลสำหรับ LLM
- **FAQ Chatbot** - ระบบตอบคำถามอัตโนมัติ
- **Image/Audio Search** - ค้นหาสื่อมัลติมีเดีย
- **Duplicate Detection** - ตรวจจับข้อมูลซ้ำ

---

## คุณสมบัติหลัก

### ประสิทธิภาพสูง
| คุณสมบัติ | รายละเอียด |
|-----------|------------|
| **HNSW Algorithm** | ใช้ [usearch](https://github.com/unum-cloud/usearch) - อัลกอริทึม Hierarchical Navigable Small World ที่เร็วที่สุด |
| **SIMD Acceleration** | รองรับ AVX2 และ AVX-512 สำหรับการคำนวณเวกเตอร์แบบขนาน |
| **Batch Operations** | รองรับ batch insert/search สำหรับ throughput สูงสุด |
| **Multi-threading** | Thread-safe ด้วย shared_mutex สำหรับ concurrent access |

### ความยืดหยุ่น
| คุณสมบัติ | รายละเอียด |
|-----------|------------|
| **Dual Protocol** | gRPC (พอร์ต 50051) และ REST HTTP (พอร์ต 50052) |
| **Multi-Collection** | รองรับหลาย collection แยกกันอิสระ |
| **Multi-Tenant** | รองรับหลาย tenant ด้วย namespace isolation |
| **Distance Metrics** | Cosine, Euclidean, Dot Product |
| **Metadata Storage** | เก็บ metadata แบบ key-value ร่วมกับเวกเตอร์ |

### ความน่าเชื่อถือ
| คุณสมบัติ | รายละเอียด |
|-----------|------------|
| **Persistence** | บันทึกข้อมูลลงดิสก์อัตโนมัติ |
| **Health Check** | HTTP endpoint สำหรับ monitoring |
| **Docker Ready** | Multi-stage Dockerfile สำหรับ production |
| **Graceful Shutdown** | รองรับ SIGINT/SIGTERM signals |

---


  API Routes:

  | Endpoint                     | Method         | Description             |
  |------------------------------|----------------|-------------------------|
  | /health                      | GET            | Health check            |
  | /collections                 | GET/POST       | List/Create collections |
  | /collections/:name           | GET/DELETE     | Get stats/Delete        |
  | /search                      | POST           | Single search           |
  | /batch_search                | POST           | Parallel batch search   |
  | /insert                      | POST           | Insert vector           |
  | /batch_insert                | POST           | Batch insert            |
  | /tenants/:id/namespaces      | GET/POST       | List/Create namespace   |
  | /tenants/:id/:ns/faq         | POST           | Add FAQ                 |
  | /tenants/:id/:ns/faq/bulk    | POST           | Bulk add FAQ            |
  | /tenants/:id/:ns/search      | POST           | Search in namespace     |
  | /tenants/:id/search          | POST           | Cross-namespace search  |
  | /tenants/:id/:ns/faq/:faq_id | GET/PUT/DELETE | FAQ CRUD                |
  | /tenants/:id/:ns/stats       | GET            | Namespace stats         |
  | /tenants/:id/stats           | GET            | Tenant stats            |

## สถาปัตยกรรม

```
┌─────────────────────────────────────────────────────────────┐
│                      Client Applications                      │
│              (Python, Node.js, Go, Java, etc.)               │
└─────────────────────┬───────────────────┬───────────────────┘
                      │                   │
                      ▼                   ▼
              ┌───────────────┐   ┌───────────────┐
              │   gRPC API    │   │   HTTP API    │
              │  (Port 50051) │   │  (Port 50052) │
              └───────┬───────┘   └───────┬───────┘
                      │                   │
                      └─────────┬─────────┘
                                ▼
              ┌─────────────────────────────────┐
              │         Vector Storage          │
              │    (Collection Management)      │
              └─────────────────┬───────────────┘
                                │
                                ▼
              ┌─────────────────────────────────┐
              │          HNSW Index             │
              │   (usearch + SIMD Operations)   │
              └─────────────────┬───────────────┘
                                │
                                ▼
              ┌─────────────────────────────────┐
              │        Disk Persistence         │
              │       (Binary Index Files)      │
              └─────────────────────────────────┘
```

---

## ความต้องการระบบ

### ขั้นต่ำ
- **OS**: Linux (Ubuntu 20.04+, Debian 11+, CentOS 8+)
- **CPU**: x86_64 พร้อม AVX2 support
- **RAM**: 4 GB
- **Disk**: 10 GB SSD

### แนะนำ
- **OS**: Ubuntu 22.04 LTS / 24.04 LTS
- **CPU**: Intel Xeon / AMD EPYC พร้อม AVX-512
- **RAM**: 16+ GB
- **Disk**: NVMe SSD

### Dependencies
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    libgrpc-dev \
    protobuf-compiler-grpc
```

---

## การติดตั้ง

### วิธีที่ 1: Build จาก Source

```bash
# Clone repository
git clone https://github.com/GOD00072/vector_service.git
cd vector_service

# สร้าง build directory
mkdir build && cd build

# Configure ด้วย CMake
cmake -DCMAKE_BUILD_TYPE=Release \
      -DUSE_AVX2=ON \
      -DBUILD_TESTS=ON \
      ..

# Build
make -j$(nproc)

# รัน tests (optional)
ctest --output-on-failure
```

### วิธีที่ 2: Docker

```bash
# Build image
docker build -t vector-service:latest .

# Run container
docker run -d \
    --name vector-service \
    -p 50051:50051 \
    -p 50052:50052 \
    -v $(pwd)/data:/data \
    vector-service:latest
```

### Docker Compose

```yaml
version: '3.8'
services:
  vector-service:
    build: .
    ports:
      - "50051:50051"
      - "50052:50052"
    volumes:
      - ./data:/data
    environment:
      - VECTOR_PORT=50051
      - VECTOR_HTTP_PORT=50052
      - VECTOR_DATA_DIR=/data
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "wget", "--spider", "-q", "http://localhost:50052/health"]
      interval: 30s
      timeout: 10s
      retries: 3
```

---

## การใช้งาน

### เริ่มต้น Server

```bash
# รันด้วย default settings
./build/vector_server

# รันด้วย custom ports
./build/vector_server --port 50051 --http-port 50052 --data ./data

# หรือใช้ environment variables
VECTOR_PORT=50051 VECTOR_HTTP_PORT=50052 VECTOR_DATA_DIR=/data ./build/vector_server
```

### Output เมื่อ Server เริ่มทำงาน

```
=================================
  Vector Service v1.0.0
  C++ HNSW with SIMD
=================================
gRPC: 0.0.0.0:50051
HTTP: 0.0.0.0:50052
Data: ./data
SIMD: AVX2 enabled
=================================
HTTP Server listening on port 50052
Max payload: 500MB+ | Parallel search: 100K+
```

---

## HTTP API Reference

### Health Check

```bash
GET /health
```

**Response:**
```json
{
  "healthy": true,
  "version": "1.0.0"
}
```

---

### Collection Management

#### สร้าง Collection

```bash
POST /collections
Content-Type: application/json

{
  "name": "products",
  "dimension": 384,
  "metric": "cosine",
  "m": 16,
  "ef_construction": 200,
  "ef_search": 50
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `name` | string | required | ชื่อ collection |
| `dimension` | int | required | จำนวน dimensions ของเวกเตอร์ |
| `metric` | string | `cosine` | `cosine`, `euclidean`, `dot_product` |
| `m` | int | `16` | HNSW M parameter (connections per layer) |
| `ef_construction` | int | `200` | ความแม่นยำขณะ build index |
| `ef_search` | int | `50` | ความแม่นยำขณะ search |

#### แสดงรายการ Collections

```bash
GET /collections
```

**Response:**
```json
{
  "collections": [
    {
      "name": "products",
      "dimension": 384,
      "count": 10000,
      "metric": "cosine"
    }
  ]
}
```

#### ลบ Collection

```bash
DELETE /collections/{name}
```

---

### Vector Operations

#### เพิ่มเวกเตอร์

```bash
POST /insert
Content-Type: application/json

{
  "collection": "products",
  "vector": {
    "id": "prod-001",
    "values": [0.1, 0.2, 0.3, ...],
    "metadata": {
      "name": "iPhone 15",
      "category": "electronics"
    }
  }
}
```

#### เพิ่มเวกเตอร์แบบ Batch

```bash
POST /batch_insert
Content-Type: application/json

{
  "collection": "products",
  "vectors": [
    {
      "id": "prod-001",
      "values": [0.1, 0.2, ...],
      "metadata": {"name": "Product 1"}
    },
    {
      "id": "prod-002",
      "values": [0.3, 0.4, ...],
      "metadata": {"name": "Product 2"}
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "inserted_count": 2,
  "total_received": 2
}
```

#### ค้นหาเวกเตอร์

```bash
POST /search
Content-Type: application/json

{
  "collection": "products",
  "query": [0.1, 0.2, 0.3, ...],
  "top_k": 10
}
```

**Response:**
```json
{
  "results": [
    {
      "id": "prod-001",
      "score": 0.95,
      "metadata": {"name": "iPhone 15"}
    }
  ],
  "search_time_ms": 0.52
}
```

#### ค้นหาแบบ Batch

```bash
POST /batch_search
Content-Type: application/json

{
  "collection": "products",
  "queries": [
    {"values": [0.1, 0.2, ...]},
    {"values": [0.3, 0.4, ...]}
  ],
  "top_k": 5
}
```

#### ค้นหาพร้อม Filter

```bash
POST /search_with_filter
Content-Type: application/json

{
  "collection": "products",
  "query": [0.1, 0.2, ...],
  "top_k": 10,
  "filter": {
    "category": "electronics"
  }
}
```

#### ดึงเวกเตอร์ตาม ID

```bash
GET /vectors/{collection}/{id}
```

#### อัพเดทเวกเตอร์

```bash
PUT /vectors/{collection}/{id}
Content-Type: application/json

{
  "values": [0.1, 0.2, ...],
  "metadata": {"name": "Updated Name"}
}
```

#### ลบเวกเตอร์

```bash
DELETE /vectors/{collection}/{id}
```

---

### Multi-Tenant API

สำหรับระบบที่ต้องรองรับหลาย tenant (ลูกค้า) แยกกัน

#### สร้าง Namespace

```bash
POST /tenants/{tenant_id}/namespaces
Content-Type: application/json

{
  "namespace": "faq",
  "dimension": 384,
  "metric": "cosine"
}
```

#### เพิ่ม FAQ

```bash
POST /tenants/{tenant_id}/{namespace}/faq
Content-Type: application/json

{
  "id": "faq-001",
  "question": "วิธีการชำระเงิน?",
  "answer": "รองรับบัตรเครดิต, โอนเงิน, และ QR Code",
  "category": "payment",
  "vector": [0.1, 0.2, ...]
}
```

#### เพิ่ม FAQ แบบ Bulk

```bash
POST /tenants/{tenant_id}/{namespace}/faq/bulk
Content-Type: application/json

{
  "items": [
    {
      "id": "faq-001",
      "question": "คำถาม 1",
      "answer": "คำตอบ 1",
      "vector": [...]
    }
  ]
}
```

#### ค้นหาใน Namespace

```bash
POST /tenants/{tenant_id}/{namespace}/search
Content-Type: application/json

{
  "query": [0.1, 0.2, ...],
  "top_k": 5,
  "category": "payment"
}
```

#### ค้นหาข้าม Namespaces

```bash
POST /tenants/{tenant_id}/search
Content-Type: application/json

{
  "query": [0.1, 0.2, ...],
  "top_k": 5,
  "namespaces": ["faq", "docs"]
}
```

#### สถิติ Tenant

```bash
GET /tenants/{tenant_id}/stats
```

**Response:**
```json
{
  "tenant_id": "company-a",
  "namespace_count": 3,
  "total_vectors": 15000,
  "total_memory_bytes": 52428800,
  "namespaces": [
    {"name": "faq", "vector_count": 5000},
    {"name": "docs", "vector_count": 10000}
  ]
}
```

---

### Statistics & Monitoring

#### สถิติ Collection

```bash
GET /stats/{collection}
```

**Response:**
```json
{
  "total_vectors": 10000,
  "memory_usage_bytes": 41943040,
  "dimension": 384,
  "metric": "cosine"
}
```

#### สถิติ Index

```bash
GET /index/{collection}
```

**Response:**
```json
{
  "collection": "products",
  "total_vectors": 10000,
  "dimension": 384,
  "memory_usage_bytes": 41943040,
  "memory_usage_mb": 40.0,
  "metric": "cosine",
  "bytes_per_vector": 4194
}
```

#### นับจำนวนเวกเตอร์

```bash
GET /count/{collection}
```

---

### Persistence

#### บันทึก Collection

```bash
POST /save
Content-Type: application/json

{"collection": "products"}
```

#### บันทึกทุก Collections

```bash
POST /save_all
```

---

## gRPC API

ดู `proto/vector_service.proto` สำหรับ API definition เต็มรูปแบบ

### Python Client Example

```python
import grpc
from vector_service_pb2 import *
from vector_service_pb2_grpc import VectorServiceStub

# เชื่อมต่อ
channel = grpc.insecure_channel('localhost:50051')
stub = VectorServiceStub(channel)

# สร้าง collection
response = stub.CreateCollection(CreateCollectionRequest(
    name="products",
    dimension=384,
    metric="cosine"
))

# เพิ่มเวกเตอร์
response = stub.Insert(InsertRequest(
    collection="products",
    vector=Vector(
        id="prod-001",
        values=[0.1, 0.2, 0.3, ...],
        metadata={"name": "Product 1"}
    )
))

# ค้นหา
response = stub.Search(SearchRequest(
    collection="products",
    query=[0.1, 0.2, 0.3, ...],
    top_k=10
))

for result in response.results:
    print(f"ID: {result.id}, Score: {result.score}")
```

---

## HNSW Parameters Tuning

### Parameter Guide

| Parameter | ค่า | ผลกระทบ |
|-----------|-----|---------|
| **M** | 8-64 | สูง = แม่นยำขึ้น แต่ใช้ memory มาก |
| **ef_construction** | 100-500 | สูง = build ช้าแต่ index คุณภาพดี |
| **ef_search** | 50-200 | สูง = ค้นหาช้าแต่แม่นยำมาก |

### Recommended Settings

| Use Case | M | ef_construction | ef_search |
|----------|---|-----------------|-----------|
| **Speed Priority** | 8 | 100 | 30 |
| **Balanced** | 16 | 200 | 50 |
| **Accuracy Priority** | 32 | 400 | 100 |
| **Maximum Accuracy** | 64 | 500 | 200 |

---

## Performance Benchmarks

ทดสอบบน Intel Xeon E5-2680 v4 @ 2.40GHz, 128GB RAM

### Insert Performance

| Vectors | Dimension | Time | Throughput |
|---------|-----------|------|------------|
| 100,000 | 384 | 2.3s | 43,478 vec/s |
| 1,000,000 | 384 | 28s | 35,714 vec/s |
| 100,000 | 1536 | 8.1s | 12,345 vec/s |

### Search Performance (top_k=10)

| Vectors | Dimension | Latency (p50) | Latency (p99) |
|---------|-----------|---------------|---------------|
| 100,000 | 384 | 0.3ms | 0.8ms |
| 1,000,000 | 384 | 0.5ms | 1.5ms |
| 10,000,000 | 384 | 0.8ms | 2.5ms |

### Batch Search Performance

| Queries | Vectors | Total Time | QPS |
|---------|---------|------------|-----|
| 1,000 | 1,000,000 | 0.8s | 1,250 |
| 10,000 | 1,000,000 | 7.5s | 1,333 |
| 100,000 | 1,000,000 | 72s | 1,388 |

---

## การ Deploy ใน Production

### Systemd Service

```ini
# /etc/systemd/system/vector-service.service
[Unit]
Description=Vector Service
After=network.target

[Service]
Type=simple
User=vectordb
Group=vectordb
WorkingDirectory=/opt/vector-service
ExecStart=/opt/vector-service/vector_server
Environment=VECTOR_PORT=50051
Environment=VECTOR_HTTP_PORT=50052
Environment=VECTOR_DATA_DIR=/var/lib/vector-service
Restart=always
RestartSec=5
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable vector-service
sudo systemctl start vector-service
```

### Nginx Reverse Proxy

```nginx
upstream vector_http {
    server 127.0.0.1:50052;
    keepalive 32;
}

server {
    listen 443 ssl http2;
    server_name vector.example.com;

    ssl_certificate /etc/ssl/certs/vector.crt;
    ssl_certificate_key /etc/ssl/private/vector.key;

    location / {
        proxy_pass http://vector_http;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_read_timeout 300s;
        proxy_send_timeout 300s;
        client_max_body_size 500M;
    }
}
```

---

## Monitoring

### Prometheus Metrics (via HTTP)

```bash
# ตรวจสอบ health
curl http://localhost:50052/health

# ดู stats ของแต่ละ collection
curl http://localhost:50052/stats/products
```

### Health Check Script

```bash
#!/bin/bash
response=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:50052/health)
if [ "$response" -eq 200 ]; then
    echo "OK"
    exit 0
else
    echo "FAIL"
    exit 1
fi
```

---

## Troubleshooting

### ปัญหาที่พบบ่อย

#### 1. Build Error: grpc_cpp_plugin not found

```bash
sudo apt-get install protobuf-compiler-grpc
```

#### 2. SIMD ไม่ทำงาน

ตรวจสอบ CPU support:
```bash
grep -o 'avx2\|avx512' /proc/cpuinfo | head -1
```

Build ด้วย scalar fallback:
```bash
cmake -DUSE_AVX2=OFF -DUSE_AVX512=OFF ..
```

#### 3. Port ถูกใช้งานอยู่

```bash
# ตรวจสอบ port
sudo lsof -i :50051
sudo lsof -i :50052

# ใช้ port อื่น
./vector_server --port 50061 --http-port 50062
```

#### 4. Memory ไม่พอ

- ลด `M` parameter
- ลด `max_elements`
- ใช้ dimension ที่ต่ำลง

---

## โครงสร้างไฟล์

```
vector_service/
├── CMakeLists.txt          # Build configuration
├── Dockerfile              # Container image
├── README.md               # เอกสารนี้
├── include/
│   ├── grpc_server.hpp     # gRPC server
│   ├── http_server.hpp     # HTTP server
│   ├── http_router.hpp     # HTTP routing
│   ├── hnsw_index.hpp      # HNSW index wrapper
│   ├── simd_ops.hpp        # SIMD operations
│   ├── usearch_index.hpp   # usearch wrapper
│   └── vector_storage.hpp  # Storage layer
├── src/
│   ├── main.cpp            # Entry point
│   ├── grpc_server.cpp     # gRPC implementation
│   ├── http_server.cpp     # HTTP implementation
│   ├── http_router.cpp     # HTTP routing
│   ├── hnsw_index.cpp      # HNSW implementation
│   ├── simd_ops.cpp        # SIMD implementation
│   └── vector_storage.cpp  # Storage implementation
├── proto/
│   └── vector_service.proto # gRPC definitions
├── tests/
│   ├── test_hnsw.cpp       # HNSW tests
│   └── test_simd.cpp       # SIMD tests
├── deps/
│   ├── usearch/            # HNSW library
│   └── fp16/               # Half-precision floats
└── data/                   # Data storage
```

---

## License

MIT License - ใช้งานได้อย่างเสรีทั้งเชิงพาณิชย์และส่วนบุคคล

---

## Acknowledgments

- [usearch](https://github.com/unum-cloud/usearch) - HNSW implementation
- [gRPC](https://grpc.io/) - RPC framework
- [Protocol Buffers](https://developers.google.com/protocol-buffers) - Serialization
