# tradeengine

A high-performance C++20 market-data feed handler for **HKEX OMD-D** and **TSE Arrowhead FLEX MBO** exchange protocols. Raw UDP packets are decoded, normalised into an exchange-agnostic event model, and fanned out to downstream handlers — all with zero virtual dispatch.

---

## Architecture

```
Packet Source          Adapter (CRTP)          Handler chain
─────────────          ──────────────          ─────────────
UdpSource        ──▶   HkexAdapter       ──▶   FanoutHandler ──▶ InstrumentRegistry
PcapSource       ──▶   TseAdapter        ──▶                 ──▶ LoggingHandler
TextFileSource
```

- **Feed layer** (`feed/`) reads raw UDP payloads from a live socket, a pcap capture, or a hex-text file and pumps them into the adapter via a lock-free MPMC queue on a dedicated reader thread.
- **Adapter layer** (`adapter/`) decodes wire messages and emits normalised `mde::` events. Each adapter is a CRTP template on its downstream handler — no virtual calls, no heap allocations per message.
- **Domain model** (`marketdata.h`) defines the exchange-agnostic structs (`InstrumentDef`, `OrderEvent`, `TradeEvent`, …) that every adapter outputs and every handler consumes.
- **Registry** (`registry.h`) stores the product catalogue populated by instrument-definition events. A compile-time `FanoutHandler` variadic template broadcasts every event to an arbitrary list of handlers simultaneously.
- **Order books** (`adapter/orderbook.h`) maintain a per-instrument `fastorderbook::OrderBook` updated from normalised `OrderEvent`s, supporting top-of-book and depth queries.

---

## Supported Exchanges

| Exchange | Protocol | Adapter |
|----------|----------|---------|
| Hong Kong Exchanges (HKEX) | OMD-D (Options Market Data) | `adapter/hkex/hkexadapter.h` |
| Tokyo Stock Exchange (TSE) | Arrowhead FLEX MBO | `adapter/tse/tseadapter.h` |

---

## Dependencies

| Library | Purpose | Notes |
|---------|---------|-------|
| **libpcap** | pcap/pcapng replay | Optional; enables pcap replay mode |
| **Boost.Asio** | Live UDP / multicast | Optional; file mode works without it |
| **Aeron C++ client** | IPC feed transport | Optional; enables Aeron source and XDP relayer |
| **libxdp + libbpf** | AF_XDP kernel-bypass capture | Optional; needed for `xdp_udp_relayer` on Linux |
| **NanoLog** | Low-latency structured logging | Bundled (`thirdparty/NanoLog`) |
| **fastorderbook** | Cache-friendly price-level order book | Bundled (`thirdparty/fastorderbook`) |
| **MPMCSimpleQueue** | Lock-free multi-producer/consumer queue | Bundled (`thirdparty/MPMCSimpleQueue`) |
| **GoogleTest** | Unit tests | Fetched automatically via CMake `FetchContent` |

---

## Build

```bash
# Prerequisites (macOS example)
# libpcap is optional but recommended for pcap replay
brew install boost
# Optional: brew install libpcap

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The build produces these targets:

| Target | Description |
|--------|-------------|
| `tradeengine` | Feed-handler executable |
| `testadapter` | GoogleTest unit-test binary |
| `xdp_udp_relayer` | Linux-only AF_XDP → Aeron IPC relayer (built only when dependencies are found) |

---

`thirdparty/xdpio` also supports standalone configure/build:

```bash
cmake -S thirdparty/xdpio -B build/xdpio
cmake --build build/xdpio
```

In the main build, `tradeengine` consumes Aeron IPC via the `xdpio::ipc` target.

---

## Usage

```bash
# Live UDP / multicast feed
./build/tradeengine udp <addr> <port> [iface]

# pcap / pcapng replay (optional UDP-port filter)
./build/tradeengine pcap <path> [udp-port]

# Hex-text replay (one packet per line)
./build/tradeengine file <path>
```

Logs are written to `./log/tradeengine.<n>.txt` via NanoLog's guaranteed logger.

---

## Running Tests

```bash
cd build
ctest --output-on-failure
# or run the binary directly
./testadapter
```

Tests feed synthetic wire packets through both adapters and assert that normalised events land correctly in the `InstrumentRegistry` via a compile-time `FanoutHandler`.

---

## Project Layout

```
tradeengine/
├── main.cpp              # Entry point — HKEX feed handler
├── marketdata.h          # Exchange-agnostic domain model
├── registry.h            # InstrumentRegistry + FanoutHandler
├── logginghandler.h      # NanoLog-backed event logger
├── adapter/
│   ├── base.h            # CRTP adapter base
│   ├── orderbook.h       # Per-instrument order book map
│   ├── hkex/             # HKEX OMD-D adapter
│   └── tse/              # TSE Arrowhead FLEX MBO adapter
├── feed/
│   ├── dispatch.h        # Lock-free threaded pump
│   ├── udp.h             # Live UDP / multicast source (Boost.Asio)
│   ├── pcap.h            # pcap/pcapng replay source
│   └── textfile.h        # Hex-text replay source
├── thirdparty/
│   ├── NanoLog/          # Low-latency logging
│   ├── fastorderbook/    # Price-level order book
│   ├── MPMCSimpleQueue/  # Lock-free MPMC queue
│   └── xdpio/         # XDP+Aeron module (Linux relayer + IPC headers)
├── tests/
│   └── testadapter.cpp   # GoogleTest suite
└── CMakeLists.txt
```
