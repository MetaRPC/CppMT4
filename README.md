# CppMT4 SDK

> High-performance, modern **C++17** SDK for MetaTrader 4 algorithmic trading automation over gRPC.

[![Docs](https://img.shields.io/badge/docs-CppMT4-blue.svg)](https://metarpc.github.io/CppMT4/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

📄 **Full documentation**: [https://metarpc.github.io/CppMT4/](https://metarpc.github.io/CppMT4/)

CppMT4 connects to real MetaTrader 4 terminals through the MetaRPC gRPC gateway — read the account, stream live prices, and place/manage orders from C++.

---

## 🧱 Three-layer architecture

| Layer | Type | What it gives you |
|-------|------|-------------------|
| Low | `MT4Account` | Raw gRPC: builds each proto `*Request`, attaches auth metadata, returns typed `*Data` payloads (or throws) |
| Mid | `MT4Service` | Friendly args and named C++ structs, times in UTC — a **protobuf-free** header |
| High | `MT4Sugar` | One-liners: balances, prices, order construction, pip-based SL/TP, risk sizing |

Each layer **owns** the one below (`sugar.service()`, `service.account()`); ready [Strategies](https://metarpc.github.io/CppMT4/Strategies/Strategies.Master.Overview/) build on top. Errors are exceptions (`ApiError` / `TransportError`); transient failures retry and a dropped terminal re-attaches automatically.

> **🆚 MT4, not MT5.** MT4 has **no "positions"** — market and pending orders share one list. There is **no DOM** and **no server-side pre-trade calculators**. A rejected trade carries **no retcode**: it throws `ApiError` with the **MQL4 error code** (e.g. `132` market closed, `130` invalid stops). Timeframes are in minutes (`Timeframe::H1 == 60`).

---

## 📦 Build

Requirements: a **C++17** compiler, **CMake ≥ 3.15** (Ninja recommended), and the **protobuf** + **grpc++** development packages (discovered via `pkg-config`).

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build pkg-config \
     libprotobuf-dev protobuf-compiler libgrpc++-dev protobuf-compiler-grpc

# MSYS2 (Windows, UCRT64)
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,protobuf,grpc}

# macOS (Homebrew)
brew install cmake ninja pkg-config protobuf grpc
```

```bash
git clone https://github.com/MetaRPC/CppMT4.git
cd CppMT4
cmake -G Ninja -B build
cmake --build build
```

This produces the `metarpc_cppmt4` static library plus the **`demos`** launcher and the offline **`protobuf_inspector`** (both on by default). The gRPC contract ships as one version-independent file, [`proto/mt4_term_api.desc`](https://github.com/MetaRPC/CppMT4/blob/main/proto/mt4_term_api.desc); CMake runs **your** `protoc` on it at build time, so the generated stubs always match your installed protobuf — download that one file to build a client in any language.

To also build the minimal `quickstart` program, add `-DBUILD_EXAMPLES=ON`.

---

## 🚀 Minimal example

```cpp
#include <iostream>
#include <metarpc/mt4_sugar.hpp>
using namespace metarpc;

int main() {
    // host, port (API key read from MRPC_API_KEY, or pass it to the constructor; empty → TRIAL)
    MT4Sugar sugar("mt4.mrpc.pro", 443);

    sugar.connect(1235959737, "password", "EGlobal-Demo");   // login, password, broker server name
    std::cout << "balance " << sugar.getBalance()
              << "  bid " << sugar.getBid("EURUSD") << "\n";

    int ticket = sugar.buyMarket("EURUSD", 0.01);            // returns 0 if the broker rejects it
    if (ticket) sugar.closeOrder(ticket);

    sugar.disconnect();
}
```

Link it:

```cmake
add_subdirectory(CppMT4)
target_link_libraries(my_app PRIVATE metarpc_cppmt4)
```

---

## 🏃 Running the examples

Settings come from `examples/demos/config/config.json` **and/or** environment variables — **env vars override the file**:

```json
{ "user": 1235959737, "password": "YourPassword", "grpc_server": "mt4.mrpc.pro:443",
  "mt_cluster": "EGlobal-Demo", "api_key": "", "test_symbol": "EURUSD", "test_volume": 0.01 }
```

```bash
export MT4_LOGIN=1235959737        # Windows PowerShell: $env:MT4_LOGIN="1235959737"
export MT4_PASSWORD="YourPassword"
export MT4_SERVER="EGlobal-Demo"
export MRPC_API_KEY="your_api_key"  # defaults to TRIAL if unset
```

Then launch the **interactive menu** and pick a demo by number, or run one directly:

```bash
./build/demos                # interactive menu (lowlevel / service / sugar / orchestrators / preset)
./build/demos 6              # run demo [6] (Sugar basics) directly
./build/protobuf_inspector   # offline: explore the gRPC messages, fields & enums
```

On Windows you can also use the helper script: `.\run.bat` (optionally `.\run.bat <YOUR_API_KEY>`).

---

## 🔑 API key & authentication

Connecting to MetaRPC production (`mt4.mrpc.pro:443`) uses one of:

1. **Your API key** — sign up free at [https://mrpc.pro/signup](https://mrpc.pro/signup) and generate a key in the portal at [https://mrpc.pro/my](https://mrpc.pro/my) under **API Keys**.
2. **TRIAL access** — leave the key empty to use the shared trial gateway (fine for demo accounts and evaluation).

The SDK reads the `MRPC_API_KEY` environment variable (falling back to `TRIAL`) and sends it as the `apikey` metadata header on every call. You can also pass the key to the layer constructor or set `api_key` in `config.json`.

### Automatic session id

Two things authenticate each request: your **`apikey`** and a terminal **session id**. You don't fetch the id — `connect()` obtains it from the gateway (`ConnectEx` → `terminalInstanceGuid`), and the SDK attaches it to every later call and stream automatically (read it via `terminalGuid()`).

---

## 🌐 Production endpoints

| Environment | Host | Port | Protocol |
| :--- | :--- | :--- | :--- |
| **Production** | `mt4.mrpc.pro` | `443` | TLS / gRPC |
| **Direct API UI (Swagger)** | `https://mt4.mrpc.pro/apiui` | `443` | HTTPS |
| **Portal Dashboard** | `https://mrpc.pro/my` | `443` | HTTPS |
| **Registration / API Key** | `https://mrpc.pro/signup` | `443` | HTTPS |

---

## 📄 Documentation & guides

Full docs at [https://metarpc.github.io/CppMT4/](https://metarpc.github.io/CppMT4/):

- 🚀 **[Getting Started](https://metarpc.github.io/CppMT4/All_Guides/GETTING_STARTED/)** & **[Your First Project](https://metarpc.github.io/CppMT4/All_Guides/Your_First_Project/)**
- 🧱 **[Project Map](https://metarpc.github.io/CppMT4/All_Guides/PROJECT_MAP/)** — the three layers and source layout
- 📡 **[gRPC Streaming](https://metarpc.github.io/CppMT4/All_Guides/GRPC_STREAM_MANAGEMENT/)** — live ticks, trade events, order-profit snapshots
- 📊 **[Return Codes](https://metarpc.github.io/CppMT4/All_Guides/RETURN_CODES_REFERENCE/)** & **[ENUMs](https://metarpc.github.io/CppMT4/All_Guides/ENUMS_USAGE_REFERENCE/)** & **[Protobuf Inspector](https://metarpc.github.io/CppMT4/All_Guides/PROTOBUF_INSPECTOR_GUIDE/)**
- 💼 **API reference** — [MT4Account](https://metarpc.github.io/CppMT4/MT4Account/MT4Account.Master.Overview/) · [MT4Service](https://metarpc.github.io/CppMT4/MT4Service/MT4Service.Overview/) · [MT4Sugar](https://metarpc.github.io/CppMT4/MT4Sugar/MT4Sugar.API_Overview/)

---

## 📜 License

MIT
