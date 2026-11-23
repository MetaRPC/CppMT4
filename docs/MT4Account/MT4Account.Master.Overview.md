# MT4Account — Master Overview

> One page to **orient fast**: what lives where, how to choose the right API, and jump links to every **overview** and **method spec** in this docs set.

---

## 🚦 Start here — Section Overviews

* **[Account Information — Overview](./1.%20Account_Information/Account_Information.Overview.md)**
  The money picture and trading analytics: account snapshot, aggregated stats, equity curve, per-order floating-P&L extrema.

* **[Orders & History — Overview](./2.%20Orders_History/Orders_History.Overview.md)**
  Live orders (market **and** pending in one list — MT4 has no "positions"), ticket lists, closed-order history.

* **[Symbols & Quotes — Overview](./3.%20Symbols_Quotes/Symbols_Quotes.Overview.md)**
  Live quotes, OHLC bars, the symbol inventory, Market Watch management, full symbol parameters, tick value, broker timezone.

* **[Trading Operations — Overview](./4.%20Trading_Operations/Trading_Operations.Overview.md)**
  Place, modify, close/delete and close-by orders.

* **[Streaming Methods — Overview](./5.%20Streaming_Methods/Streaming_Methods.Overview.md)**
  Real-time streams: ticks, trade events, order-profit snapshots, ticket snapshots, connection state.

---

## 🧭 How to pick an API

| If you need…                    | Go to…                | Typical calls                                                   |
| ------------------------------- | --------------------- | -------------------------------------------------------------- |
| Account snapshot                | Account Information   | `accountSummary`                                               |
| Performance analytics           | Account Information   | `tradeStats`, `equityHistory`, `tradeUnrealizedPL`            |
| Current orders (market+pending) | Orders & History      | `openedOrders`, `openedOrdersTickets`                          |
| Closed order history            | Orders & History      | `ordersHistory`                                               |
| Live quotes                     | Symbols & Quotes      | `quote`, `quoteMany`                                           |
| Symbol inventory / properties   | Symbols & Quotes      | `symbols`, `symbolSelect`, `symbolParamsMany`, `tickValueWithSize` |
| Historical quotes / OHLC bars   | Symbols & Quotes      | `quoteHistory`, `bars`                                         |
| Broker timezone / UTC offset    | Symbols & Quotes      | `serverTimezone`                                              |
| Trading operations              | Trading Operations    | `orderSend`, `orderModify`, `orderCloseDelete`, `orderCloseBy` |
| Real-time updates               | Streaming Methods     | `onSymbolTick`, `onTrade`, `onOpenedOrdersTickets`            |

---

## 🆚 MT4 vs MT5 — what's different here

MT4 is **order-centric**, not position-centric. If you know the MT5 SDK, keep these in mind:

* **No "positions".** There are only **orders**. `openedOrders()` returns live **market orders and pending orders in one list** (`order_infos`). There is no `positionsTotal` / `PositionInfo` split.
* **Tickets are `int32`**, not `int64`.
* **6 order types** (`OO_OP_BUY` … `OO_OP_SELLSTOP`) — no stop-limit. History may also carry `Balance`/`Credit` pseudo-orders.
* **No Depth-of-Market (DOM)** and **no `orderCalcMargin` / `orderCalcProfit` / `orderCheck`** — MT4 has no server-side pre-trade calculators. Use `tickValueWithSize` + `symbolParamsMany` and compute client-side (the `MT4Service` / `MT4Sugar` layers do this for you).
* **Quotes, not ticks.** `quote` / `quoteMany` return bid/ask plus session high/low (no `last`/`volume`).
* **Trading has no retcode.** A rejected trade **throws `ApiError`** carrying the MQL4 error code (e.g. `130` invalid stops, `132` market closed, `134` not enough money) — there is no `10009`-style status field on `OrderSendData`.
* **Timeframes are in MINUTES** (M1=1, H1=60, D1=1440) on the wire.
* **Every message carries both server time and a `_utc` field** (e.g. `open_time` / `open_time_utc`); the raw layer gives you both, the `MT4Service` layer picks one for you.

---

## 🔌 Usage pattern (raw gRPC)

Every method is called on an `MT4Account` (`#include <metarpc/mt4_account.hpp>`, namespace `metarpc`). It owns the gRPC channel, sends the terminal id and API key as metadata, retries transient failures, re-attaches a dropped terminal session, and unwraps the reply — returning the protobuf `*Data` payload or throwing.

```cpp
#include <metarpc/mt4_account.hpp>
using namespace metarpc;
namespace pb = mt4_term_api;              // the generated protobuf namespace

// Create the account (prepares the gRPC channel)
MT4Account account("mt4.mrpc.pro", 443);

// Connect the terminal (login, password, broker server name)
account.connect(1235959737, "password", "EGlobal-Demo");
// The gateway returns a terminal session GUID; later calls reuse it automatically.

// Call a method: returns the Data payload, or throws ApiError / TransportError
pb::AccountSummaryData s = account.accountSummary();
std::printf("Balance: $%.2f\n", s.account_balance());
```

---

Every method follows the same shape:

* **Proto Service/Method:** `Service.Method(Request) → Reply`
* **C++ call:** `account.method(const pb::Request&)` returns `pb::MethodData` (by value)
* **Reply structure:** the gRPC reply carries either a `Data` payload or an `Error`; `MT4Account` returns the `Data` and turns `Error` into a thrown **`ApiError`** (server-side error) — transport failures past the retry budget throw **`TransportError`**
* **Trading errors:** a rejected trade throws `ApiError` with the **MQL4 error code** in `mqlErrorCode` (e.g. `132` market closed) — there is no success return code.

**Timestamps:** proto uses `google.protobuf.Timestamp` (or raw `int64` unix seconds on some messages). The raw layer gives you broker/server time **and** a `_utc` field; the friendly `MT4Service` layer selects UTC for you.

**Streaming methods:** return a `std::unique_ptr<AccountStream>`. The SDK reads the stream on a **background thread** and invokes your callback for each message; keep the handle alive and call `stop()` to end it.

---

# 📚 Full Index · All Method Specs

---

## 📄 Account Information

* **Overview:** [Account_Information.Overview.md](./1.%20Account_Information/Account_Information.Overview.md)

### Snapshot

* [AccountSummary.md](./1.%20Account_Information/AccountSummary.md) — all account info at once (balance, equity, margin, leverage, currency, server time).

### Performance Analytics

* [TradeStats.md](./1.%20Account_Information/TradeStats.md) — performance analytics (profit factor, drawdowns, Sharpe…).
* [TradeStatsEquityHistory.md](./1.%20Account_Information/TradeStatsEquityHistory.md) — stats + equity points + trades in one call.
* [EquityHistory.md](./1.%20Account_Information/EquityHistory.md) — the balance/equity curve points.
* [TradeUnrealizedPL.md](./1.%20Account_Information/TradeUnrealizedPL.md) — max/min floating P&L per open order.

---

## 📦 Orders & History

* **Overview:** [Orders_History.Overview.md](./2.%20Orders_History/Orders_History.Overview.md)

### Current State

* [OpenedOrders.md](./2.%20Orders_History/OpenedOrders.md) — full details of all live orders (market **and** pending, one list).
* [OpenedOrdersTickets.md](./2.%20Orders_History/OpenedOrdersTickets.md) — ticket numbers only (lightweight).

### Historical Data

* [OrdersHistory.md](./2.%20Orders_History/OrdersHistory.md) — closed orders within a time range (with pagination).

---

## 📊 Symbols & Quotes

* **Overview:** [Symbols_Quotes.Overview.md](./3.%20Symbols_Quotes/Symbols_Quotes.Overview.md)

### Live Quotes

* [Quote.md](./3.%20Symbols_Quotes/Quote.md) — latest bid/ask (+ session high/low) for one symbol.
* [QuoteMany.md](./3.%20Symbols_Quotes/QuoteMany.md) — latest quotes for several symbols in one call.

### Historical Quotes & Bars

* [QuoteHistory.md](./3.%20Symbols_Quotes/QuoteHistory.md) — historical quotes for a symbol.
* [Bars.md](./3.%20Symbols_Quotes/Bars.md) — OHLC bars for a symbol and timeframe (timeframe in minutes).

### Symbol Inventory & Management

* [Symbols.md](./3.%20Symbols_Quotes/Symbols.md) — every symbol the broker offers (name + index).
* [SymbolSelect.md](./3.%20Symbols_Quotes/SymbolSelect.md) — add/remove a symbol from Market Watch.
* [SymbolParamsMany.md](./3.%20Symbols_Quotes/SymbolParamsMany.md) — full parameters for one/many symbols (67 fields each).
* [TickValueWithSize.md](./3.%20Symbols_Quotes/TickValueWithSize.md) — tick value / size / contract size for P&L and sizing.

### Server & Time

* [ServerTimezone.md](./3.%20Symbols_Quotes/ServerTimezone.md) — broker server timezone / UTC offset.

---

## 🛠 Trading Operations

* **Overview:** [Trading_Operations.Overview.md](./4.%20Trading_Operations/Trading_Operations.Overview.md)

* [OrderSend.md](./4.%20Trading_Operations/OrderSend.md) — place a market or pending order.
* [OrderModify.md](./4.%20Trading_Operations/OrderModify.md) — modify SL/TP, price or expiry of an order.
* [OrderCloseDelete.md](./4.%20Trading_Operations/OrderCloseDelete.md) — close a market order / delete a pending order.
* [OrderCloseBy.md](./4.%20Trading_Operations/OrderCloseBy.md) — close one order by an opposite one.

---

## 📡 Streaming Methods

* **Overview:** [Streaming_Methods.Overview.md](./5.%20Streaming_Methods/Streaming_Methods.Overview.md)

### Real-Time Price Updates

* [OnSymbolTick.md](./5.%20Streaming_Methods/OnSymbolTick.md) — real-time tick stream for symbols.

### Trading Events

* [OnTrade.md](./5.%20Streaming_Methods/OnTrade.md) — order changes (new / updated / removed / new-history).
* [OnOpenedOrdersProfit.md](./5.%20Streaming_Methods/OnOpenedOrdersProfit.md) — periodic floating-P&L snapshots of open orders.
* [OnOpenedOrdersTickets.md](./5.%20Streaming_Methods/OnOpenedOrdersTickets.md) — periodic ticket snapshots (lightweight).

### Terminal State

* [OnConnectState.md](./5.%20Streaming_Methods/OnConnectState.md) — terminal connection-state changes.

---

## 🎯 Quick Navigation by Use Case

| I want to... | Use this method |
|-------------|-----------------|
| **ACCOUNT** | |
| Get complete account snapshot | [AccountSummary](./1.%20Account_Information/AccountSummary.md) |
| Get performance analytics | [TradeStats](./1.%20Account_Information/TradeStats.md) |
| Get the equity curve | [EquityHistory](./1.%20Account_Information/EquityHistory.md) |
| How far each open trade swung | [TradeUnrealizedPL](./1.%20Account_Information/TradeUnrealizedPL.md) |
| **ORDERS & HISTORY** | |
| Get all live orders (full details) | [OpenedOrders](./2.%20Orders_History/OpenedOrders.md) |
| Get order ticket numbers only | [OpenedOrdersTickets](./2.%20Orders_History/OpenedOrdersTickets.md) |
| Get closed-order history | [OrdersHistory](./2.%20Orders_History/OrdersHistory.md) |
| **SYMBOLS & QUOTES** | |
| Get current price for a symbol | [Quote](./3.%20Symbols_Quotes/Quote.md) |
| Get prices for many symbols at once | [QuoteMany](./3.%20Symbols_Quotes/QuoteMany.md) |
| Get historical quotes | [QuoteHistory](./3.%20Symbols_Quotes/QuoteHistory.md) |
| Get OHLC bars | [Bars](./3.%20Symbols_Quotes/Bars.md) |
| List all symbols | [Symbols](./3.%20Symbols_Quotes/Symbols.md) |
| Add a symbol to Market Watch | [SymbolSelect](./3.%20Symbols_Quotes/SymbolSelect.md) (true) |
| Get full symbol parameters (batch) | [SymbolParamsMany](./3.%20Symbols_Quotes/SymbolParamsMany.md) |
| Get tick value for P&L / sizing | [TickValueWithSize](./3.%20Symbols_Quotes/TickValueWithSize.md) |
| Get broker timezone / UTC offset | [ServerTimezone](./3.%20Symbols_Quotes/ServerTimezone.md) |
| **TRADING** | |
| Open a market BUY/SELL | [OrderSend](./4.%20Trading_Operations/OrderSend.md) (OP_BUY/OP_SELL) |
| Place a pending order | [OrderSend](./4.%20Trading_Operations/OrderSend.md) (BUYLIMIT/SELLLIMIT/BUYSTOP/SELLSTOP) |
| Modify SL/TP / price | [OrderModify](./4.%20Trading_Operations/OrderModify.md) |
| Close a market order / delete a pending | [OrderCloseDelete](./4.%20Trading_Operations/OrderCloseDelete.md) |
| Close one order by an opposite one | [OrderCloseBy](./4.%20Trading_Operations/OrderCloseBy.md) |
| **REAL-TIME** | |
| Stream live prices | [OnSymbolTick](./5.%20Streaming_Methods/OnSymbolTick.md) |
| Monitor trade events | [OnTrade](./5.%20Streaming_Methods/OnTrade.md) |
| Track ticket changes | [OnOpenedOrdersTickets](./5.%20Streaming_Methods/OnOpenedOrdersTickets.md) |

---

## 🏗️ API Architecture

### Layer 1: MT4Account (Low-Level) — You are here!

**What:** direct proto/gRPC communication with the MT4 terminal.

**When to use:**
- You need full control over the protocol
- You are building custom wrappers
- Proto-level integration is required

**Characteristics:**
- Works with proto `*Request` / `*Data` objects
- Raw gRPC method calls
- Complete access to all MT4 functions
- Highest complexity

**Location:** `include/metarpc/mt4_account.hpp` + `src/mt4_account.cpp` (on top of the gRPC stubs generated at build time from `proto/mt4_term_api.desc` into `build/generated/`)

**Documentation:** this folder (you are here!)

---

### Layer 2: MT4Service

**What:** a friendly facade over `MT4Account` — clean arguments, friendly C++ structs, times in UTC. Its header is **protobuf-free**, so strategy code compiles fast.

**When to use:**
- You want a clean API without touching protobuf
- You want named struct fields instead of proto getters
- You want times already converted to UTC

**Location:** `include/metarpc/mt4_service.hpp` + `src/mt4_service.cpp`

---

### Layer 3: MT4Sugar

**What:** high-level convenience API with ~62 one-liner methods.

**When to use:**
- Most trading scenarios (95% of cases)
- You want risk-based position sizing and smart defaults
- You are building strategies quickly

**Location:** `include/metarpc/mt4_sugar.hpp` + `src/mt4_sugar.cpp`

---

## 🎓 Learning Path

**Recommended sequence:** start from the foundation (MT4Account) → build up to the convenience layers (MT4Service → MT4Sugar).

Each layer **owns** the one below it: `MT4Sugar` owns an `MT4Service`, which owns an `MT4Account`. From Sugar you can always reach down with `sugar.service()` and `sugar.account()`.

**Summary:** MT4Account (foundation) → MT4Service (friendly facade) → MT4Sugar (one-liners).

---

## 💡 Key Concepts

### MQL4 trade error codes (thrown as `ApiError`)

MT4 trades have **no success return code** — a rejection throws `ApiError` whose `mqlErrorCode` is the MQL4 error:

* **0** = no error
* **130** = invalid stops (SL/TP too close to market)
* **131** = invalid trade volume
* **132** = market closed
* **133** = trade disabled
* **134** = not enough money
* **135** = price changed
* **136** = off quotes
* **138** = requote
* **146** = trade context busy

Catch `ApiError` around every trade call and inspect `e.mqlErrorCode` / `e.what()`.

---

## ⚠️ Important Notes

* **Demo account first:** always test on demo before live trading.
* **Trades throw on rejection:** there is no retcode — wrap `orderSend`/`orderModify`/`orderCloseDelete` in `try/catch (const ApiError&)`.
* **Orders, not positions:** `openedOrders()` returns market **and** pending orders together — tell them apart by `order_type()`.
* **Handle errors:** catch `ApiError` (server rejected the request) and `TransportError` (network/transport failed past retries).
* **Stream cleanup:** call `stop()` on the stream handle, and keep the handle alive while you need events.
* **Time zones:** raw-layer messages carry both a server-time field and a `_utc` field; the `MT4Service` layer selects UTC for you.
* **Broker limitations:** not all brokers support every symbol/feature; a symbol must be in Market Watch (`symbolSelect`) before it streams or trades.

---

"Trade safe, code clean, and may your gRPC streams always flow smoothly."
