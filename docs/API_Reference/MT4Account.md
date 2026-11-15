# MT4Account — Low-Level API (Layer 1 of 3)

`metarpc::MT4Account` is the thin, 1:1-with-the-wire layer. Every method maps to a single
gRPC RPC: you pass a protobuf `*Request` and get back the protobuf `*Data` payload of the
reply. It owns the gRPC channel, the service stubs, the connection/identity state and the
transport machinery — transient-failure retry with exponential back-off and an automatic
`ConnectEx` re-attach on `TERMINAL_INSTANCE_NOT_FOUND`.

```cpp
#include <metarpc/mt4_account.hpp>   // pulls in the generated protobuf headers
```

!!! note "When to use this layer"
    Use `MT4Account` when you need a field the friendly layers do not expose, or full control
    over the exact request. For everyday work prefer [`MT4Service`](MT4Service.md) (friendly
    structs) or [`MT4Sugar`](MT4Sugar.md) (one-liners), which hide protobuf and compile fast.

!!! tip "Per-method deep dives"
    This page is the compact reference. For a **detailed write-up of every method** (signature,
    request/reply, input/output tables, enums, worked examples) see the
    [**MT4Account — Master Overview**](../MT4Account/MT4Account.Master.Overview.md) and its
    section pages.

## 🆚 MT4 vs MT5 (if you know the MT5 SDK)

- **No "positions".** There are only **orders** — `openedOrders()` returns market **and** pending orders in one list. Tickets are `int32`.
- **No DOM, no `orderCalcMargin` / `orderCalcProfit` / `orderCheck`.** MT4 has no server-side pre-trade calculators.
- **Trades have no retcode.** A rejected trade **throws `ApiError`** carrying the MQL4 error code (e.g. `132` market closed, `130` invalid stops) — there is no `10009`-style status.
- **Timeframes are in MINUTES** (M1=1, H1=60, D1=1440).
- **Every message carries both server time and a `_utc` field.**

## How a call works

- **Protobuf in, protobuf out.** `accountSummary()` returns a `pb::AccountSummaryData`; you read
  fields with the generated accessors (`.account_balance()`, `.account_login()`, …).
- **No UTC conversion.** Times are raw broker server time (plus a `_utc` field on each message).
  `MT4Service` is the layer that selects UTC for you.
- **Errors are exceptions** (never error codes):
    - `metarpc::ApiError` — the gateway/terminal answered with an error payload (bad login,
      unknown symbol, trade rejected, …). Carries `code`, `message`, and the MQL4 code (`mqlErrorCode`).
    - `metarpc::TransportError` — a gRPC transport failure that outlived the retry budget.
- **Retry & reconnect are automatic** (see `ClientOptions`). You do not retry yourself.

```cpp
using namespace metarpc;
namespace pb = mt4_term_api;

MT4Account acc("mt4.mrpc.pro", 443, /*apiKey*/ "");
acc.connect(1235959737, "password", "EGlobal-Demo");

try {
    pb::AccountSummaryData s = acc.accountSummary();
    std::cout << s.account_login() << "  " << s.account_balance() << " " << s.account_currency() << "\n";
} catch (const ApiError& e) {
    std::cerr << e.code << ": " << e.message << "\n";
}
```

## Construction & identity

```cpp
MT4Account(const std::string& host = "mt4.mrpc.pro", int port = 443,
           const std::string& apiKey = "", const std::string& name = "");
```

| Member | Purpose |
|--------|---------|
| `uuid5(ns, name)` *(static)* | RFC 4122 v5 UUID from a namespace + name — the fleet convention for a stable terminal id |
| `computeDeterministicId(login, password)` *(static)* | Legacy SHA-256 id from credentials |
| `getId(login, password)` / `setId(id)` / `getId()` | The terminal id (set **before** `connect()`; keep it stable per account) |
| `setApiKey(k)` / `getApiKey()` | Gateway API key (`""` → `MRPC_API_KEY` env → `"TRIAL"`) |
| `setName(n)` / `getName()` | Terminal instance name |
| `options()` | Mutable `ClientOptions` (deadlines, retries, TLS roots, verbosity) |

## Connection

| Method | Returns | Notes |
|--------|---------|-------|
| `connect(login, password, server, name="", timeoutSeconds=120)` | `void` | ConnectEx by broker server name; stores creds for auto-reconnect; blocks until ready |
| `reconnect()` | `void` | Re-issue ConnectEx (also done automatically on a lost terminal) |
| `isConnected()` | `bool` | |
| `terminalGuid()` | `std::string` | The id the gateway returned |
| `disconnect(reason="")` | `void` | **Stops** the terminal on the gateway — avoid on normal shutdown |
| `close()` | `void` | Drops the local channel only; the terminal keeps running |
| `serverOffsetSeconds()` | `int64_t` | Broker clock minus UTC (cached from one `AccountSummary`) |

## Account

| Method | Returns |
|--------|---------|
| `accountSummary(req = {})` | `pb::AccountSummaryData` — full snapshot; also refreshes the cached server-time offset |

## Symbols & quotes

| Method | Returns |
|--------|---------|
| `quote(req)` | `pb::QuoteData` — latest bid/ask (+ session high/low) for one symbol |
| `quoteMany(req)` | `pb::QuoteManyData` — latest quotes for several symbols |
| `quoteHistory(req)` | `pb::QuoteHistoryData` — historical quotes |
| `symbols(req = {})` | `pb::SymbolsData` — every symbol name (+ index) |
| `symbolSelect(req)` | `pb::SymbolSelectData` — add/remove a symbol from Market Watch |
| `symbolParamsMany(req)` | `pb::SymbolParamsManyData` — full params for one/many symbols (~70 fields each) |
| `tickValueWithSize(req)` | `pb::TickValueWithSizeData` — tick value + size for several symbols |

## Orders (market + pending — one list)

| Method | Returns |
|--------|---------|
| `openedOrders(req)` | `pb::OpenedOrdersData` — all live orders (market **and** pending) |
| `openedOrdersTickets(req)` | `pb::OpenedOrdersTicketsData` — just the ticket list |
| `ordersHistory(req)` | `pb::OrdersHistoryData` — closed orders in a window (paginated) |

## Trading

!!! warning "No retcode"
    A rejected trade **throws `ApiError`** with the MQL4 error in `mqlErrorCode`. There is no success status field — a returned `*Data` means the trade was accepted.

| Method | Returns |
|--------|---------|
| `orderSend(req)` | `pb::OrderSendData` — place a market/pending order (`ticket`, `volume`, `price`, `open_time`) |
| `orderModify(req)` | `pb::OrderModifyData` — change SL/TP/price/expiry (`order_was_modified`) |
| `orderCloseDelete(req)` | `pb::OrderCloseDeleteData` — close a market order / delete a pending |
| `orderCloseBy(req)` | `pb::OrderCloseByData` — close one order by an opposite one |

## Bars

| Method | Returns |
|--------|---------|
| `bars(req)` | `pb::QuoteHistoryBarsData` — OHLC bars (timeframe in minutes; `from_time` is **required**) |

## Trade statistics

| Method | Returns |
|--------|---------|
| `tradeStats(req)` | `pb::StatsData` — profit factor, drawdowns, Sharpe, … |
| `tradeStatsEquityHistory(req)` | `pb::StatsWithChartsData` — stats + equity points + trades |
| `equityHistory(req)` | `pb::EquityHistoryData` — balance/equity curve points |
| `tradeUnrealizedPL(req)` | `pb::TradeUnrealizedPLData` — max/min floating P&L per open order |

## Server & time

| Method | Returns |
|--------|---------|
| `serverTimezone(req = {})` | `pb::ServerTimezoneData` — broker timezone / UTC offset |

## Streaming

Every `onX()` opens a server stream and delivers raw protobuf payloads to your callback on a
background thread. The returned `AccountStream` owns the subscription — keep it alive to keep
receiving, destroy it (or call `stop()`) to cancel.

| Method | Callback payload |
|--------|------------------|
| `onSymbolTick(req, cb)` | `pb::OnSymbolTickData` |
| `onTrade(req, cb)` | `pb::OnTradeData` |
| `onOpenedOrdersProfit(req, cb)` | `pb::OnOpenedOrdersProfitData` |
| `onOpenedOrdersTickets(req, cb)` | `pb::OnOpenedOrdersTicketsData` |
| `onConnectState(req, cb)` | `pb::ConnectStateData` |

`AccountStream` exposes `stop()`, `running()`, `count()`, `lastError()`.

```cpp
pb::OnSymbolTickRequest req;
req.add_symbol_names("EURUSD");
auto stream = acc.onSymbolTick(req, [](const pb::OnSymbolTickData& d) {
    if (d.has_symbol_tick())
        std::cout << d.symbol_tick().symbol() << " " << d.symbol_tick().bid() << "\n";
});
std::this_thread::sleep_for(std::chrono::seconds(5));
stream->stop();   // (also happens automatically when `stream` is destroyed)
```

## See also

- [**MT4Account — Master Overview**](../MT4Account/MT4Account.Master.Overview.md) — per-method deep dives, grouped.
- [MT4Service](MT4Service.md) — the same data as friendly C++ structs, with UTC conversion.
- [MT4Sugar](MT4Sugar.md) — one-line helpers on top of `MT4Service`.
