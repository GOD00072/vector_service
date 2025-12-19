# Vector Service

บริการฐานข้อมูลเวกเตอร์ประสิทธิภาพสูง เขียนด้วย C++ พร้อม HNSW indexing และ SIMD acceleration

## คุณสมบัติ

- **HNSW Index** - ค้นหาเวกเตอร์ใกล้เคียงแบบรวดเร็วด้วย usearch library
- **SIMD Acceleration** - รองรับ AVX2/AVX-512 สำหรับการคำนวณเวกเตอร์
- **Dual API** - gRPC (พอร์ต 50051) และ HTTP (พอร์ต 50052)
- **Multi-collection** - รองรับหลาย collection
- **Metadata Filtering** - กรองผลลัพธ์ด้วย metadata
- **Docker Ready** - มี Dockerfile พร้อมใช้งาน

## ความต้องการระบบ

- CMake 3.20+
- C++20 compiler (GCC 11+ / Clang 14+)
- gRPC และ Protobuf
- Linux ที่รองรับ AVX2 (แนะนำ)

## การ Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_AVX2=ON ..
make -j$(nproc)
```

## การรัน

```bash
./build/vector_server --port 50051 --http-port 50052 --data ./data
```

ตัวแปรสภาพแวดล้อม:
- `VECTOR_PORT` - พอร์ต gRPC (ค่าเริ่มต้น: 50051)
- `VECTOR_HTTP_PORT` - พอร์ต HTTP (ค่าเริ่มต้น: 50052)
- `VECTOR_DATA_DIR` - โฟลเดอร์เก็บข้อมูล (ค่าเริ่มต้น: ./data)

## Docker

```bash
docker build -t vector-service .
docker run -p 50051:50051 -p 50052:50052 -v ./data:/data vector-service
```

## API

### gRPC

ดู `proto/vector_service.proto` สำหรับ API เต็มรูปแบบ

การทำงานหลัก:
- `CreateCollection` - สร้าง collection ใหม่
- `Insert` / `BatchInsert` - เพิ่มเวกเตอร์
- `Search` / `BatchSearch` - ค้นหาเวกเตอร์ที่คล้ายกัน
- `Delete` - ลบเวกเตอร์
- `Health` / `Stats` - สถานะบริการ

### HTTP

- `GET /health` - ตรวจสอบสถานะ
- `POST /collections` - สร้าง collection
- `POST /collections/{name}/vectors` - เพิ่มเวกเตอร์
- `POST /collections/{name}/search` - ค้นหาเวกเตอร์

## License

MIT
