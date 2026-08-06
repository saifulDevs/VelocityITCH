# Synthetic ITCH Data Generator (DSE Mock Server)

A ultra-low-latency, zero-copy C++ synthetic market data generator and UDP broadcaster designed to simulate the **Dhaka Stock Exchange (DSE) X-stream ITCH protocol** for high-pressure load testing (capable of pushing **7,000,000+ Messages Per Second**).

---

## Technical Highlights & Features

- **Strict DSE Protocol Layout**: All ITCH structs use `#pragma pack(push, 1)` and unsigned Big-Endian binary fields (`__builtin_bswap16/32/64`).
- **O(1) HFT Dense State Manager**: Uses the **Swap-and-Pop** pattern with a contiguous `std::vector` and a custom zero-allocation open-addressing flat hash map (`FlatIndexMap`) to eliminate node allocation overhead and cache misses during random sampling.
- **Ultra-Fast PRNG**: Integrated `XorShift128+` generator for lightweight random event generation without the computational bloat of Mersenne Twister.
- **MoldUDP64 & Raw Framing**: Supports standard **MoldUDP64 protocol framing** (session headers, sequence numbers, block headers) and **Raw ITCH payload** modes.
- **Cloud VPC Compatibility (AWS / DigitalOcean)**: Supports both **UDP Multicast** (`239.255.0.1`) and **UDP Unicast point-to-point** (`--unicast`) modes to bypass cloud VPC multicast limitations.
- **Precision Rate Limiter**: Microsecond-precision token bucket rate limiter with hybrid spin-wait or unthrottled maximum throughput mode.

---

## ITCH Message Structures Supported

| Message Code | Name | Byte Size | Description |
| :--- | :--- | :--- | :--- |
| **`[S]`** | System Event | 18 bytes | Session control (`'O'` Start of Messages, `'S'` Start of System Hours) |
| **`[R]`** | Orderbook Directory | 97 bytes | Reference ticker symbol metadata (`GP`, `SQURPHARMA`, `BATBC`, `BRACBANK`, `BEXIMCO`) |
| **`[A]`** | Add Order | 30 bytes | Generates new order and assigns unique 64-bit Order Number |
| **`[E]`** | Order Executed | 25 bytes | References active Order Number and generates trade execution |
| **`[U]`** | Order Replace | 29 bytes | Cancels original Order Number and assigns new Order Number |
| **`[D]`** | Order Delete | 13 bytes | Cancels active Order Number |

---

## Directory Structure

```
VelocityITCH/
├── CMakeLists.txt
├── README.md
├── include/
│   └── itch/
│       ├── types.hpp            # Packed structs, endianness helpers & MoldUDP64 definitions
│       ├── prng.hpp             # XorShift128+ fast pseudo-random generator
│       ├── state_manager.hpp    # DenseOrderTracker (Swap-and-Pop O(1) order state manager)
│       ├── event_generator.hpp  # Synthetic DSE market event stream generator
│       └── network_dispatcher.hpp# UDP Multicast / Unicast zero-copy network sender & rate limiter
├── src/
│   └── main.cpp                 # Generator executable driver with CLI flags and telemetry
└── tests/
    ├── test_protocol.cpp        # Unit test suite for protocol packing and state manager
    └── receiver.cpp             # Client receiver utility tool for validating streams
```

---

## Build Instructions

### Prerequisites
- C++17 compliant compiler (`clang++` or `g++`)
- CMake (optional, standard compiler build commands provided below)

### Building via CMake
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

### Direct Build via Clang++ / G++
```bash
mkdir -p build
clang++ -O3 -march=native -ffast-math -std=c++17 -Iinclude src/main.cpp -o build/dse_itch_generator
clang++ -O3 -march=native -ffast-math -std=c++17 -Iinclude tests/test_protocol.cpp -o build/test_protocol
clang++ -O3 -march=native -ffast-math -std=c++17 -Iinclude tests/receiver.cpp -o build/itch_receiver
```

---

## Usage Guide

### 1. Run Unit Tests
```bash
./build/test_protocol
```

### 2. High-Pressure Load Testing Options

```bash
./build/dse_itch_generator --help
```

#### Command Line Options
- `--ip <address>`: Target IP address (Default: `239.255.0.1`)
- `--port <number>`: Target UDP port (Default: `12345`)
- `--mode <multicast|unicast>`: Network mode (`multicast` or `unicast`)
- `--framing <moldudp64|raw>`: Framing mode (`moldudp64` or `raw`)
- `--rate <mps>`: Rate limit in Messages Per Second (`0` for unlimited / max throughput)
- `--count <num>`: Total messages to generate (`0` for continuous stream)

#### Load Test Examples

**Max Throughput Unthrottled Test (Local Loopback / Unicast)**:
```bash
./build/dse_itch_generator --count 10000000 --rate 0 --mode unicast --ip 127.0.0.1
```

**Targeted 500,000 MPS Multicast Load Test**:
```bash
./build/dse_itch_generator --rate 500000 --mode multicast --ip 239.255.0.1 --port 12345
```

**Receive Stream via ITCH Receiver Utility**:
```bash
./build/itch_receiver --unicast --ip 127.0.0.1
```

---

## Cloud Deployment (AWS & DigitalOcean Pro-Tips)

1. **Multicast Constraints**: AWS VPCs and DigitalOcean private networks block standard UDP Multicast (`239.255.0.1`). When running load tests across two separate cloud VMs, launch the generator in **Unicast mode** specifying the receiver VM's private IP (`--mode unicast --ip <PRIVATE_IP>`).
2. **Socket Buffer Tuning**: The generator sets `SO_SNDBUF` to 16MB automatically. Ensure system sysctl limits permit large buffers:
   ```bash
   sudo sysctl -w net.core.wmem_max=16777216
   sudo sysctl -w net.core.rmem_max=16777216
   ```
