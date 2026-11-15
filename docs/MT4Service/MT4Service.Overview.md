---
title: MT4Service — Overview
---

# MT4Service — Mid-Level Facade (Layer 2 of 3)

> The friendly facade over [`MT4Account`](../MT4Account/MT4Account.Master.Overview.md): the **same operations**, but you pass **plain arguments** (`std::string` symbols, tickets, typed enums) and get back **clean C++ structs** — no protobuf — with **times already in UTC**.

Defined in `include/metarpc/mt4_service.hpp` + `src/mt4_service.cpp`. The header is **protobuf-free** (it forward-declares `MT4Account`), so strategy code that includes it compiles fast.

```text
MT4Sugar (HIGH)    one-liners, pip math, risk sizing
   │ owns
MT4Service (MID)   ← you are here — friendly args, clean structs, UTC
   │ owns
MT4Account (LOW)   raw gRPC, protobuf *Data payloads
```

Each `MT4Service` **owns** one `MT4Account`. Reach the raw layer any time via `service.account()` for a field the facade doesn't surface.

---

## What the facade adds

* **Friendly arguments** — build the `*Request` and cast enums for you (`quote("EURUSD")`, `orderModify(ticket, sl, tp)`).
* **Clean returns** — simple structs (`AccountInfo`, `Quote`, `Order`, `Bar`, …) instead of protobuf getters; `std::vector<…>` instead of repeated fields.
* **UTC by default** — every time returned is UTC and every time you pass in is treated as UTC. Construct with `utcTimes = false` to work in raw broker-server time.
* **Same error model** — a server rejection throws `metarpc::ApiError`, a transport failure throws `metarpc::TransportError`. (Trades still have no retcode — see [Trading](./4.%20Trading_Operations.md).)

> **Field tables live in MT4Account.** Each struct below mirrors a protobuf `*Data`; for the full per-field proto detail, follow the links to the matching [MT4Account](../MT4Account/MT4Account.Master.Overview.md) method page.

---

## Construction & connection

```cpp
#include <metarpc/mt4_service.hpp>
using namespace metarpc;

// host/port/apiKey/name as MT4Account; utcTimes defaults to true
MT4Service svc("mt4.mrpc.pro", 443);
svc.connect(1235959737, "password", "EGlobal-Demo");   // ConnectEx, blocks until ready

AccountInfo a = svc.accountSummary();
std::printf("Equity %.2f %s\n", a.equity, a.currency.c_str());
```

| Method | Purpose |
|--------|---------|
| `connect(login, password, server, name="", timeoutSeconds=120)` | ConnectEx by broker server name; stores creds for auto-reconnect |
| `reconnect()` / `isConnected()` / `terminalGuid()` | re-issue ConnectEx / state / terminal id |
| `disconnect(reason="")` | Disconnect RPC — **stops** the terminal on the gateway |
| `close()` | drop the local channel only; the terminal keeps running |
| `options()` | mutable `ClientOptions` (deadlines, retries) |
| `account()` | the underlying `MT4Account`, for raw RPCs not surfaced here |

---

## Section pages

* **[1. Account Information](./1.%20Account_Information.md)** — summary + trade stats / equity curve / unrealized-P&L extrema.
* **[2. Orders & History](./2.%20Orders_History.md)** — open orders, tickets, closed history.
* **[3. Symbols & Quotes](./3.%20Symbols_Quotes.md)** — quotes, symbols, params, tick value, bars, and time helpers.
* **[4. Trading Operations](./4.%20Trading_Operations.md)** — send / modify / close / close-by.
* **[5. Streaming Methods](./5.%20Streaming_Methods.md)** — ticks, trades, order-profit, ticket snapshots, connect state.

---

## Complete method index

### 1. Account Information
| Method | Returns |
|--------|---------|
| `accountSummary()` | `AccountInfo` |
| `tradeStats()` | `TradeStats` |
| `equityHistory()` | `std::vector<EquityPoint>` |
| `tradeUnrealizedPL()` | `std::vector<UnrealizedExtremum>` |

### 2. Orders & History
| Method | Returns |
|--------|---------|
| `openedOrders()` | `std::vector<Order>` |
| `openedOrders(magic)` | `std::vector<Order>` (filtered) |
| `openedOrdersTickets()` | `std::vector<int32_t>` |
| `ordersHistory(fromUnix, toUnix)` | `std::vector<HistoryOrder>` |

### 3. Symbols & Quotes
| Method | Returns |
|--------|---------|
| `quote(symbol)` | `Quote` |
| `quoteMany(symbols)` | `std::vector<Quote>` |
| `symbolSelect(symbol, select=true)` | `bool` |
| `symbolList()` | `std::vector<std::string>` |
| `symbolParams(symbol)` | `SymbolInfo` |
| `symbolParamsMany(nameFilter="")` | `std::vector<SymbolInfo>` |
| `tickValueWithSize(symbols)` | `std::vector<TickValueSize>` |
| `bars(symbol, tf, fromUnix, toUnix, maxBars=0)` | `std::vector<Bar>` |
| `lastBars(symbol, tf, count)` | `std::vector<Bar>` |
| `serverOffsetSeconds()` / `toUtc(t)` / `toServerTime(t)` | `int64_t` |
| `serverTimezone()` | `ServerTimezoneInfo` |

### 4. Trading Operations
| Method | Returns |
|--------|---------|
| `orderSend(OrderRequest)` | `OrderResult` |
| `orderModify(ticket, sl, tp, price=0, expiration=0)` | `bool` |
| `orderClose(ticket, lots=0, price=0, slippage=10)` | `void` |
| `orderCloseBy(ticket, oppositeTicket)` | `CloseByResult` |

### 5. Streaming Methods
| Method | Delivers |
|--------|----------|
| `subscribeTicks(symbols, cb)` | `Tick` |
| `streamTradeUpdates(cb)` | `TradeEvent` |
| `streamOrderProfits(cb, periodMs=1000)` | `OrderProfitEvent` |
| `streamOpenedTickets(cb, intervalMs=1000)` | `TicketsSnapshot` |
| `streamConnectState(cb)` | `ConnectState` |

All streams return a `std::unique_ptr<ServiceStream>` (`stop()` / `running()` / `count()` / `lastError()`).

---

## 🆚 vs MT4Account

| | MT4Account (low) | MT4Service (mid) |
|---|---|---|
| Arguments | build `*Request` protobuf | plain values |
| Returns | protobuf `*Data` (getters) | friendly structs / `std::vector` |
| Times | broker + `_utc` fields | one `int64_t`, UTC by default |
| Header | pulls in protobuf | protobuf-free (fast compile) |
| Enums | `pb::OO_OP_*` / `pb::OC_OP_*` | `OrderType`, `Timeframe`, `TradeMode` |

---

**Up next:** [MT4Sugar](../MT4Sugar/MT4Sugar.API_Overview.md) — one-liners on top of this facade.
