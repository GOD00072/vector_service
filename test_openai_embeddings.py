#!/usr/bin/env python3
"""Test vector service with OpenAI text-embedding-3-large (3072 dimensions)"""

import requests
import json
import os
from openai import OpenAI

BASE_URL = "http://localhost:50052"
DIMENSION = 3072  # text-embedding-3-large dimension

# Initialize OpenAI client
client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY"))

def get_embedding(text: str) -> list[float]:
    """Get embedding from OpenAI text-embedding-3-large"""
    response = client.embeddings.create(
        model="text-embedding-3-large",
        input=text
    )
    return response.data[0].embedding

def test_vector_service():
    print("=" * 60)
    print("Testing Vector Service with OpenAI text-embedding-3-large")
    print("=" * 60)

    tenant_id = "demo_tenant"
    namespace = "faq"

    # 1. Create collection with 3072 dimensions
    print("\n[1] Creating collection with 3072 dimensions...")
    resp = requests.post(f"{BASE_URL}/collections", json={
        "name": f"{tenant_id}__{namespace}",
        "dimension": DIMENSION,
        "metric": "cosine"
    })
    print(f"    Status: {resp.status_code}")
    print(f"    Response: {resp.text[:200]}")

    # 2. Add FAQ entries with embeddings
    faqs = [
        {
            "question": "วิธีการสมัครสมาชิก",
            "answer": "สามารถสมัครสมาชิกได้ที่หน้าเว็บไซต์หลัก คลิกที่ปุ่ม 'สมัครสมาชิก' และกรอกข้อมูล",
            "category": "membership"
        },
        {
            "question": "ค่าบริการเท่าไหร่",
            "answer": "ค่าบริการเริ่มต้นที่ 299 บาทต่อเดือน สำหรับแพ็กเกจพื้นฐาน",
            "category": "pricing"
        },
        {
            "question": "ติดต่อฝ่ายบริการลูกค้าอย่างไร",
            "answer": "สามารถติดต่อได้ที่ Line: @support หรือโทร 02-xxx-xxxx ตลอด 24 ชั่วโมง",
            "category": "support"
        },
        {
            "question": "วิธีการยกเลิกการสมัคร",
            "answer": "ไปที่หน้าการตั้งค่าบัญชี เลือก 'ยกเลิกการเป็นสมาชิก' และยืนยัน",
            "category": "membership"
        },
        {
            "question": "รองรับการชำระเงินแบบไหนบ้าง",
            "answer": "รองรับบัตรเครดิต/เดบิต, PromptPay, และการโอนผ่านธนาคาร",
            "category": "payment"
        }
    ]

    print("\n[2] Adding FAQ entries with OpenAI embeddings...")
    for i, faq in enumerate(faqs):
        # Get embedding for the question
        embedding = get_embedding(faq["question"])
        print(f"    [{i+1}/{len(faqs)}] Embedding {faq['question'][:30]}... ({len(embedding)} dims)")

        # Insert into vector service
        resp = requests.post(f"{BASE_URL}/vectors", json={
            "collection": f"{tenant_id}__{namespace}",
            "vectors": [{
                "id": f"faq_{i+1}",
                "values": embedding,
                "metadata": {
                    "question": faq["question"],
                    "answer": faq["answer"],
                    "category": faq["category"]
                }
            }]
        })
        if resp.status_code == 200:
            print(f"         Inserted successfully")
        else:
            print(f"         Error: {resp.status_code} - {resp.text[:100]}")

    # 3. Search with a query
    print("\n[3] Searching for similar FAQs...")
    query = "อยากสมัครใช้งาน"
    print(f"    Query: '{query}'")

    query_embedding = get_embedding(query)
    print(f"    Got query embedding ({len(query_embedding)} dims)")

    resp = requests.post(f"{BASE_URL}/search", json={
        "collection": f"{tenant_id}__{namespace}",
        "vector": query_embedding,
        "top_k": 3,
        "include_metadata": True
    })

    print(f"    Status: {resp.status_code}")
    if resp.status_code == 200:
        results = resp.json()
        print(f"\n    Found {len(results.get('results', []))} results:")
        for j, r in enumerate(results.get("results", [])):
            print(f"\n    [{j+1}] Score: {r.get('score', 'N/A'):.4f}")
            meta = r.get("metadata", {})
            print(f"        Q: {meta.get('question', 'N/A')}")
            print(f"        A: {meta.get('answer', 'N/A')[:60]}...")
            print(f"        Category: {meta.get('category', 'N/A')}")
    else:
        print(f"    Error: {resp.text}")

    # 4. Try another query
    print("\n[4] Second search test...")
    query2 = "จ่ายเงินยังไง"
    print(f"    Query: '{query2}'")

    query_embedding2 = get_embedding(query2)
    resp = requests.post(f"{BASE_URL}/search", json={
        "collection": f"{tenant_id}__{namespace}",
        "vector": query_embedding2,
        "top_k": 2,
        "include_metadata": True
    })

    if resp.status_code == 200:
        results = resp.json()
        print(f"\n    Found {len(results.get('results', []))} results:")
        for j, r in enumerate(results.get("results", [])):
            print(f"\n    [{j+1}] Score: {r.get('score', 'N/A'):.4f}")
            meta = r.get("metadata", {})
            print(f"        Q: {meta.get('question', 'N/A')}")

    # 5. Get stats
    print("\n[5] Collection stats...")
    resp = requests.get(f"{BASE_URL}/collections/{tenant_id}__{namespace}")
    print(f"    {resp.text}")

    print("\n" + "=" * 60)
    print("Test completed!")
    print("=" * 60)

if __name__ == "__main__":
    test_vector_service()
