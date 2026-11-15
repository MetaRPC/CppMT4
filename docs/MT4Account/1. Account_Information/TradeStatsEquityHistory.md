# 📊 Getting Stats + Equity Curve (One Call)

> **Request:** the full statistics **plus** the equity-curve points **and** the trade list in a single call (`StatsWithChartsData`), from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::tradeStatsEquityHistory(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradeStats`
* **Proto definition:** `TradeStatsEquityHistory` (defined in `mt4-term-api-trade-stats.proto`)

### RPC

* **Service:** `mt4_term_api.TradeStats`
* **Method:** `TradeStatsEquityHistory(TradeStatsEquityHistoryRequest) → TradeStatsEquityHistoryReply`
* **Low-level client (generated):** `TradeStats::Stub::TradeStatsEquityHistory(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning `StatsData` (same as [TradeStats](./TradeStats.md)) **bundled** with the equity points and the trades.
* **Why you need it.** Draw a performance report — numbers + equity chart + trade list — without three separate calls.
* **Sanity check.** `has_stats()` is true and `charts_size()` > 0.

---

## 🎯 Purpose

* Build a complete report page (KPIs, equity curve, trade blotter) from one response.
* Avoid extra round-trips when you need both the stats and the curve.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Stats + equity points + trades in one call.
    // Throws ApiError / TransportError on failure.
    pb::StatsWithChartsData tradeStatsEquityHistory(const pb::TradeStatsEquityHistoryRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
TradeStatsEquityHistoryRequest {
  optional string from     = 1;   // broker-time start, "yyyy-MM-ddTHH:mm:ss"
  optional bool   exclude_same_bars = 2;   // collapse consecutive equal-equity points
  optional string from_utc = 3;   // UTC start, same format
}
```

Send **one** of `from` / `from_utc`; omit both for the full history. `exclude_same_bars` thins the curve.

**Reply message:**

```protobuf
TradeStatsEquityHistoryReply {
  oneof response {
    StatsWithChartsData data  = 1;
    Error               error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                                        | Description                                 |
| --------- | ------------------------------------------- | ------------------------------------------- |
| `req`     | `const pb::TradeStatsEquityHistoryRequest&`   | Optional `from` / `from_utc`, `exclude_same_bars`. |

```cpp
pb::TradeStatsEquityHistoryRequest req;
req.set_exclude_same_bars(true);
pb::StatsWithChartsData d = account.tradeStatsEquityHistory(req);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `StatsWithChartsData`

| Field (proto) | C++ accessor   | C++ type                                 | Description                               |
| ------------- | -------------- | ---------------------------------------- | ----------------------------------------- |
| `stats`       | `stats()`      | `StatsData`                              | The full statistics (guard `has_stats()`). See [TradeStats](./TradeStats.md). |
| `charts`      | `charts()`     | `RepeatedPtrField<EquityPointData>`      | Equity-curve points. Use `charts_size()`. |
| `trades`      | `trades()`     | `RepeatedPtrField<TradeHistoryItemData>` | Closed trades in the window.              |
| `open_trades` | `open_trades()`| `RepeatedPtrField<TradeHistoryItemData>` | Currently open trades.                    |

### `EquityPointData` (one curve point)

| Field (proto)               | C++ accessor                  | C++ type                      | Description                   |
| --------------------------- | ----------------------------- | ----------------------------- | ----------------------------- |
| `time` / `time_utc`         | `time()` / `time_utc()`       | `google::protobuf::Timestamp` | Point time (broker / UTC).    |
| `balance`                   | `balance()`                   | `double`                      | Balance at the point.         |
| `equity`                    | `equity()`                    | `double`                      | Equity at the point.          |
| `balance_drawdown_raw/relative` | `balance_drawdown_raw()` / `…_relative()` | `double`      | Balance drawdown (money / %). |
| `equity_drawdown_raw/relative`  | `equity_drawdown_raw()` / `…_relative()`  | `double`      | Equity drawdown (money / %).  |
| `realized_pl` / `unrealized_pl` | `realized_pl()` / `unrealized_pl()` | `double`              | Realized / floating P/L.      |
| `cash_flow`                 | `cash_flow()`                 | `double`                      | Deposits/withdrawals at the point. |

### `TradeHistoryItemData` (one trade)

| Field (proto)   | C++ accessor      | C++ type             | Description                  |
| --------------- | ----------------- | -------------------- | ---------------------------- |
| `ticket`        | `ticket()`        | `int64_t`            | Trade ticket.                |
| `symbol`        | `symbol()`        | `const std::string&` | Instrument.                  |
| `is_buy`        | `is_buy()`        | `bool`               | Buy (`true`) / sell.         |
| `lots`          | `lots()`          | `double`             | Volume.                      |
| `open_price` / `close_price` | `open_price()` / `close_price()` | `double` | Entry / exit price.  |
| `profit`        | `profit()`        | `double`             | Realized profit.             |
| `commission`    | `commission()`    | `double`             | Commission (two m's here).   |
| `swap`          | `swap()`          | `double`             | Swap.                        |
| `open_time` / `close_time` | `open_time()` / `close_time()` | `google::protobuf::Timestamp` | Times (+`_utc`). |

---

## 🧩 Notes & Tips

* **One call, three payloads** — stats, curve, trades. Guard each (`has_stats()`, `charts_size()`).
* **`exclude_same_bars`** collapses flat stretches of the equity curve — smaller payload, same shape.
* **`TradeHistoryItemData::commission()`** is spelled correctly (two m's), unlike `StatsData::comissions()`.
* **String dates** — same ISO-8601 string format as [TradeStats](./TradeStats.md).
* **Heavier than `equityHistory`** — if you only need the curve, call [EquityHistory](./EquityHistory.md).

---

## 🔗 Usage Examples

### 1) Basic — KPIs + curve length

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::StatsWithChartsData d = account.tradeStatsEquityHistory(pb::TradeStatsEquityHistoryRequest{});
    if (d.has_stats())
        std::printf("PF %.2f  over %lld trades\n", d.stats().profit_factor(), (long long)d.stats().trades());
    std::printf("equity points: %d   trades: %d\n", d.charts_size(), d.trades_size());
    return 0;
}
```

### 2) Dump the equity curve

```cpp
#include <ctime>
void dumpCurve(const pb::StatsWithChartsData& d) {
    for (const auto& p : d.charts()) {
        std::time_t t = (std::time_t)p.time_utc().seconds();
        char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d", std::gmtime(&t));
        std::printf("%s  bal %.2f  eq %.2f\n", buf, p.balance(), p.equity());
    }
}
```

### 3) Peak equity & max drawdown from the curve

```cpp
void peakDD(const pb::StatsWithChartsData& d) {
    double peak = 0, maxDD = 0;
    for (const auto& p : d.charts()) {
        peak = std::max(peak, p.equity());
        maxDD = std::max(maxDD, peak - p.equity());
    }
    std::printf("Peak equity %.2f  max DD %.2f\n", peak, maxDD);
}
```

### 4) List the trades

```cpp
void listTrades(const pb::StatsWithChartsData& d) {
    for (const auto& t : d.trades())
        std::printf("#%lld %-8s %s %.2f  P/L %.2f\n",
                    (long long)t.ticket(), t.symbol().c_str(),
                    t.is_buy() ? "BUY" : "SELL", t.lots(), t.profit());
}
```

### 5) Thin curve for charting

```cpp
pb::StatsWithChartsData compactCurve(MT4Account& account) {
    pb::TradeStatsEquityHistoryRequest req;
    req.set_exclude_same_bars(true);        // fewer points
    return account.tradeStatsEquityHistory(req);
}
```

### 6) Error handling

```cpp
bool tryReport(MT4Account& account, pb::StatsWithChartsData& out) {
    try { out = account.tradeStatsEquityHistory(pb::TradeStatsEquityHistoryRequest{}); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Build a report struct

```cpp
struct Report { double pf; int64_t trades; int points; double maxDD; };
Report buildReport(const pb::StatsWithChartsData& d) {
    double peak = 0, maxDD = 0;
    for (const auto& p : d.charts()) { peak = std::max(peak, p.equity()); maxDD = std::max(maxDD, peak - p.equity()); }
    return { d.has_stats() ? d.stats().profit_factor() : 0.0,
             d.has_stats() ? d.stats().trades() : 0, d.charts_size(), maxDD };
}
```

---

## 📚 See Also

* [TradeStats](./TradeStats.md) — the stats alone (lighter)
* [EquityHistory](./EquityHistory.md) — the curve alone (lighter)
* [OrdersHistory](../2.%20Orders_History/OrdersHistory.md) — the raw closed orders
* **Friendly layer:** `MT4Service::equityHistory(...)` returns the curve as `std::vector<EquityPoint>`.
