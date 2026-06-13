# IICPC Summer Trading Hackathon: HFT Benchmarking Sandbox

An institutional-grade, microsecond-precision testing environment engineered to evaluate contestant algorithmic trading engines under simulated peak market volatility. 

This platform explicitly abandons standard web paradigms (HTTP/JSON) in favor of native C++ asynchronous event loops, custom binary memory structs, and strict Docker hypervisor CPU pinning. This guarantees that latency telemetry measures the actual algorithmic efficiency of the contestant's code, completely sterilized from the background noise of the host operating system.

## 🏗️ Architecture Overview

The system is fully decoupled into four distinct microservices:
1. **The API Gateway (Python/FastAPI):** Asynchronous control plane for code ingestion, dynamic compilation (`g++ -O3`), and strict container lifecycle management.
2. **The Contestant Sandbox (Docker/Ubuntu):** A rigidly isolated container pinned to a single physical CPU core (`--cpuset-cpus=0`), housing the contestant's Limit Order Book (LOB).
3. **The Attack Fleet (C++):** A standalone load generator utilizing asynchronous event-driven I/O (`kqueue`/`poll`) to simulate microbursts (5,000 concurrent orders) without OS thread saturation.
4. **The Telemetry Dashboard (NGINX/React):** An Alpine-based frontend visualizing hardware-level nanosecond latencies in real-time.

## 🚀 Quick Start Guide

### Prerequisites
* Docker Desktop & Docker Compose
* C++ Compiler (`g++` or `clang++` for the load generator)
* macOS or Linux (Required for `kqueue`/`poll` asynchronous event loops)

### 1. Boot the Cloud Infrastructure
Clone the repository and launch the decoupled environment:
git clone https://github.com/ATHARV-IIT-BOMBAY/iicpc-trading-sandbox.git
cd iicpc-trading-sandbox
docker-compose up --build
*(This command spins up the FastAPI Gateway on Port 8000 and the Telemetry Dashboard on Port 3000).*

### 2. Arm the Sandbox (Upload Engine)
1. Open the OpenAPI UI at `http://localhost:8000/docs`.
2. Navigate to the `POST /upload-engine/` endpoint.
3. Click **Try it out** -> **Choose File** -> Select the `matching_engine.cpp` file from the root directory.
4. Click **Execute**. 
*(The Python gateway dynamically compiles the C++, locks it inside a CPU-pinned Docker container, and binds it to Port 9000).*

### 3. Unleash the Attack Fleet
Open a second terminal tab, compile, and run the C++ load generator to simulate a peak market microburst:
cd load-generator
g++ -O3 bot_fleet.cpp -o attack_fleet
./attack_fleet

### 4. View Real-Time Telemetry
Open your browser to `http://localhost:3000`. 
The attack fleet calculates round-trip hardware-level percentiles (`std::chrono`) and beams them to the dashboard, displaying microsecond-accurate **p50 (Median)**, **p90**, and **p99 (Tail)** latencies.

## ⚡ Key Engineering Highlights

* **The Network Nuke (`TCP_NODELAY`):** Bypasses Nagle's Algorithm at the kernel level to instantly fire unbuffered packets.
* **Custom Binary Protocol:** Serializes orders into a strictly padded 29-byte memory struct (`#pragma pack(1)`), eliminating the serialization overhead of JSON.
* **Asynchronous I/O (The C10k Solution):** A single-threaded event loop maintains 200 concurrent TCP sockets, allowing the injection of 5,000 orders in milliseconds with zero thread context-switching contention.
* **Hardware CPU Pinning:** Prevents "noisy neighbor" interference and L1/L2 cache misses by physically locking the contestant sandbox to Core 0.

## 📁 Repository Structure

* `/sandbox-api` - FastAPI orchestration gateway and Docker-in-Docker logic.
* `/load-generator` - C++ Asynchronous `kqueue` Attack Fleet.
* `/frontend` - NGINX/React live telemetry visualizer.
* `/infrastructure` - Dockerfiles defining the Ubuntu sandbox execution environment.
* `matching_engine.cpp` - Reference Limit Order Book utilizing an O(log N) Price-Time Priority memory tree (`std::map`).
