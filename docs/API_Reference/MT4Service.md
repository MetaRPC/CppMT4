# MT4Service — Friendly Facade (Layer 2 of 3)

`metarpc::MT4Service` is the mid-level layer: the same operations as [`MT4Account`](MT4Account.md), but with **plain arguments**, **clean C++ structs** (no protobuf) and **times in UTC**. Its header is protobuf-free, so strategy code compiles fast.

```cpp
#include <metarpc/mt4_service.hpp>   // no protobuf headers pulled in
```

!!! tip "Full write-up"
    This page is the compact reference. For the grouped deep-dive — every method's signature, return struct and a worked example — see the [**MT4Service — Overview**](../MT4Service/MT4Service.Overview.md) and its section pages.

```cpp
using namespace metarpc;

MT4Service svc("mt4.mrpc.pro", 443);         // utcTimes defaults to true
svc.connect(1235959737, "password", "EGlobal-Demo");

AccountInfo a = svc.accountSummary();
std::printf("Equity %.2f %s\n", a.equity, a.currency.c_str());
```

Each `MT4Service` **owns** one `MT4Account`; reach the raw layer via `svc.account()`.

## Construction & connection

| Method | Returns | Notes |
|--------|---------|-------|
| `MT4Service(host="mt4.mrpc.pro", port=443, apiKey="", name="", utcTimes=true)` | — | `utcTimes=false` → raw broker time |
| `connect(login, password, server, name="", timeoutSeconds=120)` | `void` | ConnectEx; blocks until ready |
| `reconnect()` / `isConnected()` / `terminalGuid()` | `void`/`bool`/`std::string` | |
| `disconnect(reason="")` | `void` | **stops** the terminal |
| `close()` | `void` | drop the channel only |
| `options()` | `ClientOptions&` | deadlines / retries |
| `account()` | `MT4Account&` | the raw layer |

## Account · [details](../MT4Service/1.%20Account_Information.md)

| Method | Returns |
|--------|---------|
| `accountSummary()` | `AccountInfo` |
| `tradeStats()` | `TradeStats` |
| `equityHistory()` | `std::vector<EquityPoint>` |
| `tradeUnrealizedPL()` | `std::vector<UnrealizedExtremum>` |

## Orders & history · [details](../MT4Service/2.%20Orders_History.md)

| Method | Returns |
|--------|---------|
| `openedOrders()` / `openedOrders(magic)` | `std::vector<Order>` |
| `openedOrdersTickets()` | `std::vector<int32_t>` |
| `ordersHistory(fromUnix, toUnix)` | `std::vector<HistoryOrder>` |

## Symbols & quotes · [details](../MT4Service/3.%20Symbols_Quotes.md)

| Method | Returns |
|--------|---------|
| `quote(symbol)` / `quoteMany(symbols)` | `Quote` / `std::vector<Quote>` |
| `symbolSelect(symbol, select=true)` | `bool` |
| `symbolList()` | `std::vector<std::string>` |
| `symbolParams(symbol)` / `symbolParamsMany(filter="")` | `SymbolInfo` / `std::vector<SymbolInfo>` |
| `tickValueWithSize(symbols)` | `std::vector<TickValueSize>` |
| `bars(symbol, tf, fromUnix, toUnix, maxBars=0)` / `lastBars(symbol, tf, count)` | `std::vector<Bar>` |
| `serverOffsetSeconds()` / `toUtc(t)` / `toServerTime(t)` | `int64_t` |
| `serverTimezone()` | `ServerTimezoneInfo` |

## Trading · [details](../MT4Service/4.%20Trading_Operations.md)

!!! warning "No retcode"
    A rejected trade throws `ApiError` (MQL4 code in `mqlErrorCode`). `orderModify` returns `bool`; the others surface the throw.

| Method | Returns |
|--------|---------|
| `orderSend(OrderRequest)` | `OrderResult` |
| `orderModify(ticket, sl, tp, price=0, expiration=0)` | `bool` |
| `orderClose(ticket, lots=0, price=0, slippage=10)` | `void` |
| `orderCloseBy(ticket, oppositeTicket)` | `CloseByResult` |

## Streaming · [details](../MT4Service/5.%20Streaming_Methods.md)

Each returns a `std::unique_ptr<ServiceStream>` (`stop()`/`running()`/`count()`/`lastError()`) and delivers friendly structs on a background thread.

| Method | Delivers |
|--------|----------|
| `subscribeTicks(symbols, cb)` | `Tick` |
| `streamTradeUpdates(cb)` | `TradeEvent` |
| `streamOrderProfits(cb, periodMs=1000)` | `OrderProfitEvent` |
| `streamOpenedTickets(cb, intervalMs=1000)` | `TicketsSnapshot` |
| `streamConnectState(cb)` | `ConnectState` |

## See also

- [**MT4Service — Overview**](../MT4Service/MT4Service.Overview.md) — grouped deep-dives with examples.
- [MT4Account](MT4Account.md) — the raw gRPC layer beneath (full proto field tables).
- [MT4Sugar](MT4Sugar.md) — one-line helpers on top.
