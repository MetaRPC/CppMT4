# 📈 Getting the Equity Curve

> **Request:** the balance/equity-curve points (`EquityHistoryData`) over an optional window — the lightweight way to draw an equity chart, from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::equityHistory(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradeStats`
* **Proto definition:** `EquityHistory` (defined in `mt4-term-api-trade-stats.proto`)

### RPC

* **Service:** `mt4_term_api.TradeStats`
* **Method:** `EquityHistory(EquityHistoryRequest) → EquityHistoryReply`
* **Low-level client (generated):** `TradeStats::Stub::EquityHistory(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning just the **equity-curve points** (`EquityPointData`): time, balance, equity, drawdowns, realized/unrealized P/L.
* **Why you need it.** Draw an equity/balance chart without the full stats bundle.
* **Sanity check.** `points_size()` > 0 and points are time-ordered.

---

## 🎯 Purpose

* Plot the balance and equity curves.
* Compute peak equity and max drawdown yourself.
* Get the curve alone when you don't need the [TradeStats](./TradeStats.md) numbers.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // The balance/equity-curve points over an optional window.
    // Throws ApiError / TransportError on failure.
    pb::EquityHistoryData equityHistory(const pb::EquityHistoryRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
EquityHistoryRequest {
  optional string from     = 1;   // broker-time start, "yyyy-MM-ddTHH:mm:ss"
  optional bool   exclude_same_bars = 2;   // collapse consecutive equal-equity points
  optional string from_utc = 3;   // UTC start, same format
}
```

Send **one** of `from` / `from_utc`; omit both for the full history.

**Reply message:**

```protobuf
EquityHistoryReply {
  oneof response {
    EquityHistoryData data  = 1;
    Error             error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                           | Description                                 |
| --------- | ------------------------------ | ------------------------------------------- |
| `req`     | `const pb::EquityHistoryRequest&` | Optional `from` / `from_utc`, `exclude_same_bars`. |

```cpp
pb::EquityHistoryRequest req;
req.set_exclude_same_bars(true);
// req.set_from_utc("2026-01-01T00:00:00");
pb::EquityHistoryData h = account.equityHistory(req);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `EquityHistoryData`

| Field (proto) | C++ accessor | C++ type                            | Description                      |
| ------------- | ------------ | ----------------------------------- | -------------------------------- |
| `points`      | `points()`   | `RepeatedPtrField<EquityPointData>` | The curve points. Use `points_size()`. |

### `EquityPointData` (one point)

| Field (proto)                   | C++ accessor                       | C++ type                      | Description                  |
| ------------------------------- | ---------------------------------- | ----------------------------- | ---------------------------- |
| `time` / `time_utc`             | `time()` / `time_utc()`            | `google::protobuf::Timestamp` | Point time (broker / UTC).   |
| `balance`                       | `balance()`                        | `double`                      | Balance at the point.        |
| `equity`                        | `equity()`                         | `double`                      | Equity at the point.         |
| `balance_drawdown_raw`          | `balance_drawdown_raw()`           | `double`                      | Balance drawdown (money).    |
| `balance_drawdown_relative`     | `balance_drawdown_relative()`      | `double`                      | Balance drawdown (%).        |
| `equity_drawdown_raw`           | `equity_drawdown_raw()`            | `double`                      | Equity drawdown (money).     |
| `equity_drawdown_relative`      | `equity_drawdown_relative()`       | `double`                      | Equity drawdown (%).         |
| `realized_pl`                   | `realized_pl()`                    | `double`                      | Realized P/L to date.        |
| `unrealized_pl`                 | `unrealized_pl()`                  | `double`                      | Floating P/L at the point.   |
| `cash_flow`                     | `cash_flow()`                      | `double`                      | Deposit/withdrawal at the point. |

---

## 🧩 Notes & Tips

* **Lighter than the combined call** — if you also want the KPIs, use [TradeStatsEquityHistory](./TradeStatsEquityHistory.md) instead of two calls.
* **`exclude_same_bars`** thins flat stretches — smaller payload, same visible shape.
* **String dates** — `from` / `from_utc` are ISO-8601 strings (not `Timestamp`).
* **Times come in pairs** — `time()` (broker) and `time_utc()`.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — print the curve

```cpp
#include <metarpc/mt4_account.hpp>
#include <ctime>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::EquityHistoryData h = account.equityHistory(pb::EquityHistoryRequest{});
    for (const auto& p : h.points()) {
        std::time_t t = (std::time_t)p.time_utc().seconds();
        char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d", std::gmtime(&t));
        std::printf("%s  bal %.2f  eq %.2f\n", buf, p.balance(), p.equity());
    }
    return 0;
}
```

### 2) Peak equity and max drawdown

```cpp
void maxDrawdown(const pb::EquityHistoryData& h) {
    double peak = 0, maxDD = 0, maxDDpct = 0;
    for (const auto& p : h.points()) {
        peak = std::max(peak, p.equity());
        double dd = peak - p.equity();
        if (dd > maxDD) { maxDD = dd; maxDDpct = peak > 0 ? dd / peak * 100 : 0; }
    }
    std::printf("Peak %.2f  max DD %.2f (%.1f%%)\n", peak, maxDD, maxDDpct);
}
```

### 3) Net realized P/L (last point)

```cpp
double realizedToDate(const pb::EquityHistoryData& h) {
    return h.points_size() ? h.points(h.points_size() - 1).realized_pl() : 0.0;
}
```

### 4) Export to CSV

```cpp
#include <fstream>
void toCsv(const pb::EquityHistoryData& h, const std::string& path) {
    std::ofstream f(path);
    f << "time_utc,balance,equity,equity_dd\n";
    for (const auto& p : h.points())
        f << p.time_utc().seconds() << ',' << p.balance() << ','
          << p.equity() << ',' << p.equity_drawdown_relative() << '\n';
}
```

### 5) Since a date, thinned

```cpp
pb::EquityHistoryData curveSince(MT4Account& account, const std::string& isoUtc) {
    pb::EquityHistoryRequest req;
    req.set_from_utc(isoUtc);
    req.set_exclude_same_bars(true);
    return account.equityHistory(req);
}
```

### 6) Error handling

```cpp
bool tryEquity(MT4Account& account, pb::EquityHistoryData& out) {
    try { out = account.equityHistory(pb::EquityHistoryRequest{}); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Min/max equity window

```cpp
std::pair<double,double> equityRange(const pb::EquityHistoryData& h) {
    double lo = 1e18, hi = -1e18;
    for (const auto& p : h.points()) { lo = std::min(lo, p.equity()); hi = std::max(hi, p.equity()); }
    return { lo, hi };
}
```

---

## 📚 See Also

* [TradeStatsEquityHistory](./TradeStatsEquityHistory.md) — the curve **plus** stats and trades
* [TradeStats](./TradeStats.md) — aggregated statistics alone
* [AccountSummary](./AccountSummary.md) — the current balance/equity snapshot
* **Friendly layer:** `MT4Service::equityHistory(...)` → `std::vector<EquityPoint>` with UTC times.
