# Your First Project in 10 Minutes (C++)

> **Hands-on quick start** — build a working MetaTrader 4 bot with CppMT4 from scratch.

---

## Step 0 — API key (or TRIAL)

To reach the MetaRPC endpoint (`mt4.mrpc.pro:443`) you need either an API key or TRIAL access:

1. Register at [mrpc.pro/signup](https://mrpc.pro/signup) and copy a key from [mrpc.pro/my](https://mrpc.pro/my), **or**
2. leave the key empty to use TRIAL access (fine for demo accounts).

You'll also need **demo account credentials** — login, password, and the broker **server name** (e.g. `EGlobal-Demo`).

---

## Step 1 — get the SDK

Building a **C++ app**? Clone the whole project:

```bash
git clone https://github.com/MetaRPC/CppMT4.git
cd CppMT4
```

The SDK is header + source under `include/metarpc/` and `src/`; the gRPC stubs are generated at build time from `proto/mt4_term_api.desc`.

> Building in **another language** (or your own codegen) instead? You don't need the whole repo — just grab the single contract file [`proto/mt4_term_api.desc`](https://github.com/MetaRPC/CppMT4/blob/main/proto/mt4_term_api.desc) and run your own `protoc` on it. See [What to download](GETTING_STARTED.md#-what-to-download).

---

## Step 2 — write your bot

The fastest path is the high-level [`MT4Sugar`](../MT4Sugar/MT4Sugar.API_Overview.md) layer. Create `main.cpp`:

```cpp
#include <metarpc/mt4_sugar.hpp>
#include <cstdio>
using namespace metarpc;

int main() {
    // TRIAL access (empty API key). Pass your key as the 3rd arg if you have one.
    MT4Sugar sugar("mt4.mrpc.pro", 443);

    // login, password, broker server name
    sugar.connect(1235959737, "password", "EGlobal-Demo");
    std::printf("Connected. Balance %.2f  Equity %.2f\n",
                sugar.getBalance(), sugar.getEquity());

    // current price
    std::printf("EURUSD %.5f / %.5f\n", sugar.getBid("EURUSD"), sugar.getAsk("EURUSD"));

    // ⚠️ open a tiny demo BUY, then close it (returns 0 if the broker rejects it)
    int ticket = sugar.buyMarket("EURUSD", 0.01);
    if (ticket) {
        std::printf("opened #%d\n", ticket);
        sugar.closeOrder(ticket);
        std::puts("closed");
    } else {
        std::puts("order rejected (market closed? try a 24/7 symbol like BTCUSD)");
    }
    return 0;
}
```

> **🆚 MT4:** `buyMarket` returns an **`int` ticket** — `0` means the broker rejected it (it never throws for an ordinary rejection). There are no "positions": you close the **order** with `closeOrder(ticket)`.

---

## Step 3 — build & run

CppMT4 builds with CMake (MSYS2 UCRT64 / protobuf 6.x on Windows). The repo ships a ready `examples/quickstart.cpp` that does exactly the above:

```bash
cmake -B build -S . -DBUILD_EXAMPLES=ON
cmake --build build --target quickstart
./build/quickstart        # Windows: .\build\quickstart.exe
```

Pass your credentials via environment variables and run the full demo menu instead:

```bash
# MT4_LOGIN / MT4_PASSWORD / MT4_SERVER (+ optional MT4_SYMBOL / MT4_VOLUME)
cmake -B build -S . -DBUILD_DEMOS=ON
cmake --build build --target demos
./build/demos            # interactive menu of 14 demos
```

---

## 🚀 Next steps

You've made a live gRPC connection to MetaTrader 4. Next:

- **[Project Map](PROJECT_MAP.md)** — how the three layers fit together.
- **[gRPC Streaming](GRPC_STREAM_MANAGEMENT.md)** — subscribe to live ticks and trade events.
- **[MT4Sugar](../MT4Sugar/MT4Sugar.API_Overview.md)** — risk-based sizing and one-liner trading.
- **[MT4Account API](../API_Reference/MT4Account.md)** — every low-level terminal method.
