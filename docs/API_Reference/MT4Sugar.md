# MT4Sugar — One-Liner Helpers (Layer 3 of 3)

`metarpc::MT4Sugar` is the high-level layer: self-contained one-liners for everyday tasks — *"what's my balance?"*, *"buy 0.1 lots"*, *"close everything"*. No context, no request objects, no protobuf. The header compiles fast.

```cpp
#include <metarpc/mt4_sugar.hpp>
```

!!! tip "Full write-up"
    This page is the compact reference. For grouped deep-dives with examples, see the [**MT4Sugar — Overview**](../MT4Sugar/MT4Sugar.API_Overview.md) and its section pages.

!!! note "Never throws on an ordinary rejection"
    A rejected trade throws `ApiError` from Layer 1; the sugar helpers **catch it** and report `false` / `0`. Trading helpers return the ticket (`0` on rejection); close/modify return `bool`.

```cpp
using namespace metarpc;

MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");   // or sugar.quickConnect() from env

double lots = sugar.calculatePositionSize("EURUSD", /*risk%*/ 1.0, /*SL pips*/ 20);
int ticket  = sugar.buyMarketWithPips("EURUSD", lots, 20, 40);
```

Each `MT4Sugar` **owns** an `MT4Service`; reach it via `sugar.service()`, or the raw layer via `sugar.account()`.

## Connection & account · [details](../MT4Sugar/1.%20Connection_Account.md)

| Method | Returns |
|--------|---------|
| `connect(login, password, server, name="", timeoutSeconds=120)` / `quickConnect()` | `void` |
| `isConnected()` / `ping()` | `bool` |
| `disconnect()` | `void` |
| `getBalance()` / `getEquity()` / `getMargin()` / `getFreeMargin()` / `getMarginLevel()` / `getProfit()` | `double` |
| `getAccountInfo()` | `AccountInfo` |

## Prices · [details](../MT4Sugar/2.%20Prices.md)

| Method | Returns |
|--------|---------|
| `getBid(symbol)` / `getAsk(symbol)` / `getSpread(symbol)` | `double` |
| `getPriceInfo(symbol)` / `waitForPrice(symbol, targetBid, timeoutMs=10000)` | `PriceInfo` |

## Trading · [details](../MT4Sugar/3.%20Trading.md)

| Method | Returns |
|--------|---------|
| `buyMarket` / `sellMarket` / `buyLimit` / `sellLimit` / `buyStop` / `sellStop` | `int` (ticket, 0 = rejected) |
| `buy/sellMarketWithSLTP` · `buy/sellLimitWithSLTP` · `buy/sellMarketWithPips` | `int` |

## Order management & info · [details](../MT4Sugar/4.%20Order_Management.md)

| Method | Returns |
|--------|---------|
| `closeOrder(ticket)` / `closeOrderPartial(ticket, lots)` | `bool` |
| `closeAllOrders()` / `closeAllBySymbol(symbol)` | `int` (count) |
| `modifyOrderSL` / `modifyOrderTP` / `modifyOrderSLTP` | `bool` |
| `getOpenOrders()` / `getOrdersBySymbol(symbol)` | `std::vector<Order>` |
| `getOrderByTicket(ticket, found&)` | `Order` |
| `hasOpenOrder(symbol)` | `bool` |
| `countOpenOrders()` | `int` |
| `getTotalProfit()` / `getProfitBySymbol(symbol)` | `double` |

## History & statistics · [details](../MT4Sugar/5.%20History_Statistics.md)

| Method | Returns |
|--------|---------|
| `getHistoryToday/Yesterday/ThisWeek/ThisMonth()` / `getHistoryDateRange(fromUnix, toUnix)` | `std::vector<HistoryOrder>` |
| `getProfitToday/ThisWeek/ThisMonth()` | `double` |
| `getDailyStats()` | `DailyStats` |

## Symbols & risk · [details](../MT4Sugar/6.%20Symbols_Risk.md)

| Method | Returns |
|--------|---------|
| `getSymbolInfo(symbol)` | `SymbolInfo` |
| `getAllSymbols()` | `std::vector<std::string>` |
| `isSymbolAvailable(symbol)` | `bool` |
| `getMinStopLevel(symbol)` / `getSymbolDigits(symbol)` | `int` |
| `calculatePositionSize(symbol, riskPercent, stopLossPips)` | `double` |
| `getMaxLotSize(symbol)` / `calculateRequiredMargin(symbol, volume)` | `double` |
| `canOpenOrder(symbol, volume, reason&)` | `bool` |
| `calculateSLTP(symbol, direction, entry, slPips, tpPips, slOut&, tpOut&)` | `void` |

## See also

- [**MT4Sugar — Overview**](../MT4Sugar/MT4Sugar.API_Overview.md) — grouped deep-dives with examples.
- [MT4Service](MT4Service.md) — the friendly facade beneath (named structs, UTC).
- [MT4Account](MT4Account.md) — the raw gRPC layer (full proto field tables).
