# Writing Your Own Code (Scratchpad)

> How to spin up your own CppMT4 program with the connection boilerplate already handled — the fastest way to try an idea against a live demo terminal.

Unlike the Python SDK's `18_usercode.py` slot, CppMT4 is a compiled library, so "your sandbox" is simply **your own `.cpp` that links the SDK**. There are two easy starting points.

---

## Option A — start from `quickstart.cpp`

The repo ships `examples/quickstart.cpp`: connect → read balance → tiny trade → close. Copy it and edit the middle:

```cpp
#include <metarpc/mt4_sugar.hpp>
#include <cstdio>
using namespace metarpc;

int main() {
    MT4Sugar sugar("mt4.mrpc.pro", 443);          // empty key → TRIAL access
    sugar.connect(1235959737, "password", "EGlobal-Demo");   // your demo creds

    // ────────────────────────────────────────────────
    //  YOUR CODE HERE
    // ────────────────────────────────────────────────
    std::printf("Balance: %.2f\n", sugar.getBalance());
    std::printf("EURUSD %.5f / %.5f\n", sugar.getBid("EURUSD"), sugar.getAsk("EURUSD"));

    // e.g. open and close a tiny demo order:
    // int t = sugar.buyMarket("EURUSD", 0.01);
    // if (t) sugar.closeOrder(t);
    // ────────────────────────────────────────────────
    return 0;
}
```

Build & run:

```bash
cmake -B build -S . -DBUILD_EXAMPLES=ON
cmake --build build --target quickstart
./build/quickstart
```

---

## Option B — the ready sandbox demo (ships in the repo)

A ready-to-edit scratchpad already ships at **`examples/demos/sandbox/99_sandbox.cpp`**, wired into the demos menu as **[18]**. It reuses the demos' connection + config helpers, so you never hard-code credentials — `sugar` arrives already connected. Just edit the marked block:

```cpp
void demo_sandbox(MT4Sugar& sugar, const Config& cfg) {
    printSub("Sandbox — write your own code");

    // ────────────────────────────────────────────────
    //  YOUR CODE HERE — sugar is already connected
    std::cout << "balance " << sugar.getBalance() << "\n";
    for (const auto& o : sugar.getOpenOrders())
        std::cout << "  #" << o.ticket << " " << o.symbol << " P/L " << o.profit << "\n";

    //  ⚠ uncomment to trade:
    //  int t = sugar.buyMarket(cfg.testSymbol, cfg.testVolume);
    //  if (t) sugar.closeOrder(t);
    // ────────────────────────────────────────────────
}
```

Credentials come from the environment, so nothing secret goes in the file:

```bash
export MT4_LOGIN=... MT4_PASSWORD=... MT4_SERVER="EGlobal-Demo"
cmake -B build -S . -DBUILD_DEMOS=ON
cmake --build build --target demos
./build/demos 18        # runs your sandbox directly, or pick [18] from the menu
```

> It's already registered in `examples/demos/demos.hpp` + `main.cpp` (menu entry [18]); the CMake glob (`examples/demos/*.cpp`, recursive) compiles it automatically. Add more sandbox files beside it the same way.

---

## What's already handled for you

| Concern | Taken care of by |
|---------|------------------|
| **Connection** | `sugar.connect(...)` (or the demos' `helpers/connection.hpp`) performs ConnectEx and blocks until ready |
| **Session id / API key** | attached to every request automatically — you never call `GetId` |
| **Reconnect / retry** | the SDK retries transient errors and re-attaches a dropped terminal |
| **Credentials** | `MT4_LOGIN` / `MT4_PASSWORD` / `MT4_SERVER` (+ `MRPC_API_KEY`) env vars via `config/config.hpp` |

You reach down a layer any time: `sugar.service()` for the friendly facade, `sugar.account()` for raw protobuf RPCs.

---

## Handy one-liners to try

```cpp
sugar.getBalance();  sugar.getEquity();  sugar.getProfit();          // account money
sugar.getBid("EURUSD");  sugar.getSpread("EURUSD");                  // prices
sugar.buyMarketWithPips("EURUSD", 0.01, /*SL*/ 20, /*TP*/ 40);      // trade with pips
sugar.getOpenOrders();  sugar.getTotalProfit();  sugar.closeAllOrders();
sugar.getHistoryToday();  sugar.getDailyStats();                     // history / stats
sugar.calculatePositionSize("EURUSD", /*risk%*/ 1.0, /*SL pips*/ 20); // risk sizing
```

Full menus: [MT4Sugar](../MT4Sugar/MT4Sugar.API_Overview.md) · [MT4Service](../MT4Service/MT4Service.Overview.md) · [MT4Account](../MT4Account/MT4Account.Master.Overview.md).

> ⚠️ Trading one-liners run **live** on the connected account. Keep volumes tiny on demo, and remember `buyMarket` returns `0` (never throws) when the broker rejects — check the return.

---

**See also:** [Your First Project](Your_First_Project.md) · [Getting Started](GETTING_STARTED.md) · [MT4Sugar Overview](../MT4Sugar/MT4Sugar.API_Overview.md)
