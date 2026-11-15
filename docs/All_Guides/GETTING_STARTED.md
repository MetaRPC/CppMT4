# Getting Started with CppMT4

> **Quick Setup & Overview** — start automating your trading with **CppMT4** (C++ for MetaTrader 4) in minutes.

---

## 🎯 What is CppMT4?

**CppMT4** is a type-safe C++17 client library for driving **MetaTrader 4** terminals over high-performance **gRPC**. No DLL wrappers, no MQL4 bridge — you call a terminal in the cloud directly from C++.

### 🌟 Key advantages
- 🚀 **High throughput** — native gRPC streaming for live ticks and trade events.
- 🛡️ **Three-layer architecture** — raw gRPC ([`MT4Account`](../MT4Account/MT4Account.Master.Overview.md)), a friendly facade ([`MT4Service`](../MT4Service/MT4Service.Overview.md)), and one-liners ([`MT4Sugar`](../MT4Sugar/MT4Sugar.API_Overview.md)).
- 🔄 **Resilient connection** — automatic retry with back-off and transparent terminal re-attach.
- 🧩 **Orders, not positions** — MT4's order model (market + pending in one list) surfaced natively; a rejected trade throws `ApiError` with the MQL4 error code.

---

## 📦 What to download

Two options, depending on what you're building:

| You want… | Download | Why |
|-----------|----------|-----|
| **A C++ app with this SDK** | **the whole project** — `git clone https://github.com/MetaRPC/CppMT4.git` | you get the three layers, examples, demos and CMake build |
| **A client in another language / your own codegen** | **just one file** — [`proto/mt4_term_api.desc`](https://github.com/MetaRPC/CppMT4/blob/main/proto/mt4_term_api.desc) | it's the complete, version-independent gRPC contract; run your own `protoc` on it in any language |

```bash
git clone https://github.com/MetaRPC/CppMT4.git
```

CppMT4 is not on a package registry. It builds with CMake, and the protobuf/gRPC stubs are **generated at build time** by running *your* `protoc` on `proto/mt4_term_api.desc` — so they always match your installed protobuf, and the repo ships no pre-generated code. See [Your First Project](Your_First_Project.md) for the toolchain and a full build.

---

## 🔑 API key & authentication

Connecting to the MetaRPC endpoint (`mt4.mrpc.pro:443`) uses one of:

1. **An API key** — create an account at [mrpc.pro/signup](https://mrpc.pro/signup), then generate a key under **API Keys** in the [portal](https://mrpc.pro/my). Pass it to the constructor (or set `MRPC_API_KEY`).
2. **TRIAL access** — leave the API key empty to use the shared trial gateway (fine for demo accounts and evaluation).

```cpp
// explicit key:
metarpc::MT4Account account("mt4.mrpc.pro", 443, "mrpc_live_...");
// or TRIAL (empty key → MRPC_API_KEY env → "TRIAL"):
metarpc::MT4Account account("mt4.mrpc.pro", 443);
```

---

## 🆔 Automatic session ID

MetaRPC manages the session for you:

1. **`APIKey`** — your token (or `TRIAL`), sent as request metadata.
2. **`id`** — a terminal session GUID the gateway returns from `Connect` / `ConnectEx`.

> 💡 You do **not** call `GetId` or supply an `id` before connecting. `connect()` performs `ConnectEx`, the gateway returns a session GUID, and the SDK attaches it (alongside your `APIKey`) to every later request and subscription automatically.

---

## 🔌 Minimal connection example

The quickest path is the high-level [`MT4Sugar`](../MT4Sugar/MT4Sugar.API_Overview.md) layer:

```cpp
#include <metarpc/mt4_sugar.hpp>
#include <cstdio>
using namespace metarpc;

int main() {
    MT4Sugar sugar("mt4.mrpc.pro", 443);          // TRIAL access (empty key)
    sugar.connect(1235959737, "password", "EGlobal-Demo");

    std::printf("Balance: %.2f  Equity: %.2f\n", sugar.getBalance(), sugar.getEquity());
    return 0;
}
```

Prefer explicit control? The same thing on the low-level layer:

```cpp
#include <metarpc/mt4_account.hpp>
using namespace metarpc;
namespace pb = mt4_term_api;

MT4Account account("mt4.mrpc.pro", 443);
account.connect(1235959737, "password", "EGlobal-Demo");   // login, password, broker server name

pb::AccountSummaryData s = account.accountSummary();
std::printf("Balance: %.2f %s\n", s.account_balance(), s.account_currency().c_str());
```

> **Note:** protobuf scalar fields are **accessors** — `s.account_balance()`, not `s.account_balance`.

---

## 🗺️ Documentation road map

To get the most out of CppMT4, follow this reading order:

1. 🚀 **[Your First Project](Your_First_Project.md)** — build and run a working project in ~10 minutes.
2. 🗺️ **[Project Map](PROJECT_MAP.md)** — the 3 architectural layers and how they fit together.
3. 📖 **[Glossary](GLOSSARY.md)** — essential MetaTrader 4 and algo-trading terms.
4. 🐣 **[MT4 for Beginners](MT4_For_Beginners.md)** — terminal setup and the demo-account walkthrough.
5. 📡 **[gRPC Streaming](GRPC_STREAM_MANAGEMENT.md)** — subscribe to ticks, trade events, order-profit and ticket snapshots, and connection state.
6. 📊 **[Return Codes](RETURN_CODES_REFERENCE.md)** — MQL4 trade errors and gateway/transport error handling.

---

## 🏗️ Which layer should I use?

| You want… | Use | Example |
|-----------|-----|---------|
| The fastest one-liners | [`MT4Sugar`](../MT4Sugar/MT4Sugar.API_Overview.md) | `sugar.buyMarket("EURUSD", 0.1)` |
| Named struct fields, UTC times | [`MT4Service`](../MT4Service/MT4Service.Overview.md) | `svc.accountSummary().equity` |
| Full control / a raw field | [`MT4Account`](../MT4Account/MT4Account.Master.Overview.md) | `account.accountSummary().account_balance()` |

> **🆚 Coming from MT5?** MT4 has **no "positions"** (only orders — market + pending in one list), **no DOM**, and trades carry **no retcode** — a rejection throws `ApiError` with the MQL4 code. See [MT4 for Beginners](MT4_For_Beginners.md).
