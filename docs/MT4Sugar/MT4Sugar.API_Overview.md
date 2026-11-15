---
title: MT4Sugar — Overview
---

# MT4Sugar — High-Level One-Liners (Layer 3 of 3)

> The shortest path to getting things done: *"what's my balance?"*, *"what's the bid?"*, *"buy 0.1 lots"*, *"close everything"*. Every method is **self-contained** — no context, no request objects, no protobuf — so prototype code reads like plain English.

Defined in `include/metarpc/mt4_sugar.hpp` + `src/mt4_sugar.cpp`. The header is protobuf-free and compiles fast.

```text
MT4Sugar (HIGH)    ← you are here — one-liners, pip math, risk sizing
   │ owns
MT4Service (MID)   friendly structs, UTC
   │ owns
MT4Account (LOW)   raw gRPC
```

Each `MT4Sugar` **owns** an `MT4Service` (and the `MT4Account` under it). Drop down with `sugar.service()` for the friendly facade or `sugar.account()` for raw RPCs.

---

## Conventions

* **Orders, not positions.** MT4 has no "positions" — `getOpenOrders()` returns market **and** pending orders. Sugar talks *orders* throughout.
* **Never throws on an ordinary rejection.** A rejected trade throws `ApiError` from Layer 1; the sugar helpers **catch it** and report a `false` / `0` ticket instead. (A genuine transport failure can still throw `TransportError`.)
* **Pips.** A pip is `point × 10` on 3/5-digit symbols, else `point`.
* **UTC by default** — times in/out are UTC unless you constructed with `utcTimes = false`.

```cpp
#include <metarpc/mt4_sugar.hpp>
using namespace metarpc;

MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");   // or sugar.quickConnect() from env

double lots = sugar.calculatePositionSize("EURUSD", /*risk%*/ 1.0, /*SL pips*/ 20);
int ticket  = sugar.buyMarketWithPips("EURUSD", lots, /*SL*/ 20, /*TP*/ 40);
if (ticket) std::printf("opened #%d\n", ticket);
```

---

## Navigation by category

### 🔌 [1] Connection & Account → [Docs](./1.%20Connection_Account.md)
`connect` · `quickConnect` · `isConnected` · `ping` · `disconnect` · `getBalance` · `getEquity` · `getMargin` · `getFreeMargin` · `getMarginLevel` · `getProfit` · `getAccountInfo`

### 💹 [2] Prices → [Docs](./2.%20Prices.md)
`getBid` · `getAsk` · `getSpread` · `getPriceInfo` · `waitForPrice`

### 🟢 [3] Trading → [Docs](./3.%20Trading.md)
`buyMarket` · `sellMarket` · `buyLimit` · `sellLimit` · `buyStop` · `sellStop` · `buy/sellMarketWithSLTP` · `buy/sellLimitWithSLTP` · `buy/sellMarketWithPips`

### 🛠️ [4] Order Management & Info → [Docs](./4.%20Order_Management.md)
`closeOrder` · `closeOrderPartial` · `closeAllOrders` · `closeAllBySymbol` · `modifyOrderSL` · `modifyOrderTP` · `modifyOrderSLTP` · `getOpenOrders` · `getOrdersBySymbol` · `getOrderByTicket` · `hasOpenOrder` · `countOpenOrders` · `getTotalProfit` · `getProfitBySymbol`

### 📜 [5] History & Statistics → [Docs](./5.%20History_Statistics.md)
`getHistoryToday` · `getHistoryYesterday` · `getHistoryThisWeek` · `getHistoryThisMonth` · `getHistoryDateRange` · `getProfitToday` · `getProfitThisWeek` · `getProfitThisMonth` · `getDailyStats`

### 📐 [6] Symbols & Risk → [Docs](./6.%20Symbols_Risk.md)
`getSymbolInfo` · `getAllSymbols` · `isSymbolAvailable` · `getMinStopLevel` · `getSymbolDigits` · `calculatePositionSize` · `getMaxLotSize` · `canOpenOrder` · `calculateRequiredMargin` · `calculateSLTP`

---

## Sugar-specific result structs

```cpp
struct PriceInfo {   // from getPriceInfo() / waitForPrice()
    std::string symbol;
    double bid, ask, spread;   // spread = ask - bid (price units)
    int64_t time;              // UTC seconds by default
};

struct DailyStats {  // from getDailyStats()
    int totalOrders, winningOrders, losingOrders;
    double winRate;            // winning / total, percent
    double totalProfit;        // net (incl. swap & commission)
    double bestOrder, worstOrder;
};
```

Other returns reuse the MT4Service structs (`Order`, `HistoryOrder`, `SymbolInfo`, `AccountInfo`) — see [MT4Service](../MT4Service/MT4Service.Overview.md).

---

## 🆚 When to use which layer

| Need | Layer |
|------|-------|
| *"buy 0.1 lots, close all"* — fastest to write | **MT4Sugar** (here) |
| Named struct fields, explicit control, UTC | [MT4Service](../MT4Service/MT4Service.Overview.md) |
| A field the facades don't expose / raw request | [MT4Account](../MT4Account/MT4Account.Master.Overview.md) |

---

**Section pages:** [1. Connection & Account](./1.%20Connection_Account.md) · [2. Prices](./2.%20Prices.md) · [3. Trading](./3.%20Trading.md) · [4. Order Management](./4.%20Order_Management.md) · [5. History & Statistics](./5.%20History_Statistics.md) · [6. Symbols & Risk](./6.%20Symbols_Risk.md)
