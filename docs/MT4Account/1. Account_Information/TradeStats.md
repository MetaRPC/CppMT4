# 📊 Getting Trade Statistics

> **Request:** aggregated **performance statistics** (`StatsData`) over the account's closed trades — win rate, profit factor, drawdowns, Sharpe, expectancy and more — from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::tradeStats(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradeStats`
* **Proto definition:** `TradeStats` (defined in `mt4-term-api-trade-stats.proto`)

### RPC

* **Service:** `mt4_term_api.TradeStats`
* **Method:** `TradeStats(TradeStatsRequest) → TradeStatsReply`
* **Low-level client (generated):** `TradeStats::Stub::TradeStats(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC that returns the server-computed performance report for the account.
* **Why you need it.** Dashboards and journals without recomputing stats yourself from [OrdersHistory](../2.%20Orders_History/OrdersHistory.md).
* **Sanity check.** `trades()` > 0 and `profit_factor()` is finite.

---

## 🎯 Purpose

* Show win rate, profit factor, expectancy, drawdowns at a glance.
* Report performance over the whole account history, or from a start date.
* Get the metrics MT4's terminal would show — straight from the server.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Aggregated trade statistics over an optional time window.
    // Throws ApiError / TransportError on failure.
    pb::StatsData tradeStats(const pb::TradeStatsRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
TradeStatsRequest {
  optional string from     = 1;   // broker-time start, "yyyy-MM-ddTHH:mm:ss"
  optional string from_utc = 2;   // UTC start, same format
}
```

> **Times are strings**, not `Timestamp`. Send **one** of `from` / `from_utc`; omit both for the full history.

**Reply message:**

```protobuf
TradeStatsReply {
  oneof response {
    StatsData data  = 1;
    Error     error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                        | Description                                       |
| --------- | --------------------------- | ------------------------------------------------- |
| `req`     | `const pb::TradeStatsRequest&` | Optional `from` / `from_utc` start; omit for all. |

```cpp
pb::TradeStatsRequest req;                 // no `from` → full account history
// req.set_from_utc("2026-01-01T00:00:00"); // or from a date
pb::StatsData s = account.tradeStats(req);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `StatsData` (key fields)

| Field (proto)                   | C++ accessor                      | C++ type              | Description                              |
| ------------------------------- | --------------------------------- | --------------------- | ---------------------------------------- |
| `trades`                        | `trades()`                        | `int64_t`             | Number of trades counted.                |
| `profit_factor`                 | `profit_factor()`                 | `double`              | Gross profit / gross loss.               |
| `pips`                          | `pips()`                          | `double`              | Net pips.                                |
| `lots`                          | `lots()`                          | `double`              | Total lots traded.                       |
| `comissions` *(sic)*            | `comissions()`                    | `double`              | Total commissions. **Single “m”.**       |
| `sharpe_ratio`                  | `sharpe_ratio()`                  | `double`              | Sharpe ratio.                            |
| `standard_deviation`            | `standard_deviation()`            | `double`              | Std-dev of returns.                      |
| `ghpr`                          | `ghpr()`                          | `double`              | Geometric holding-period return.         |
| `growth_percent`                | `growth_percent()`                | `double`              | Account growth (%).                      |
| `max_balance_drawdown_raw`      | `max_balance_drawdown_raw()`      | `double`              | Max balance drawdown (money).            |
| `max_balance_drawdown_relative` | `max_balance_drawdown_relative()` | `double`              | Max balance drawdown (%).                |
| `max_equity_drawdown_raw`       | `max_equity_drawdown_raw()`       | `double`              | Max equity drawdown (money).             |
| `max_equity_drawdown_relative`  | `max_equity_drawdown_relative()`  | `double`              | Max equity drawdown (%).                 |
| `deposits`                      | `deposits()`                      | `double`              | Total deposits.                          |
| `withdrawals`                   | `withdrawals()`                   | `double`              | Total withdrawals.                       |

### Nested sub-messages

| Accessor           | Type                | Highlights                                                    |
| ------------------ | ------------------- | ------------------------------------------------------------ |
| `summary()`        | `TradeSummaryData`  | `open_trades()`, `day_profit()`, `week_profit()`, `month_profit()`, `total_profit()`, `open_profit()`. |
| `profitability()`  | `ProfitabilityData` | `won_trades()`, `won_trades_percent()`, `lost_trades()`, `lost_trades_percent()`. |
| `expectancy()`     | `ExpectancyData`    | `pips()`, `dollar()` — expectancy per trade.                 |
| `best_trade()` / `worst_trade()` | `ProfitData` | `profit()`, `tiket()` *(sic)*, `date()` of the extreme trade. |
| `average_win()` / `average_lost()` | `AveragePipsUsdData` | `average_pips()`, `average_usd()`.                  |
| `longs_won()` / `shorts_won()` | `WonData`   | `won_count()`, `all()`, `won_persent()` *(sic)*.             |

> Guard nested messages with `has_summary()`, `has_profitability()`, etc.

---

## 🧩 Notes & Tips

* **String dates.** `from` / `from_utc` are ISO-8601 **strings** (`"2026-01-01T00:00:00"`) — not protobuf `Timestamp`.
* **Server-computed.** No need to pull history and recompute — this is the terminal's own report.
* **Spelling quirks.** `comissions()` (one m), `tiket()`, `won_persent()` are proto typos frozen into the contract.
* **Win rate** = `profitability().won_trades_percent()`.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — full-history report

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::StatsData s = account.tradeStats(pb::TradeStatsRequest{});
    std::printf("Trades: %lld  Profit factor: %.2f  Sharpe: %.2f\n",
                (long long)s.trades(), s.profit_factor(), s.sharpe_ratio());
    if (s.has_profitability())
        std::printf("Win rate: %.1f%%  (%lld won / %lld lost)\n",
                    s.profitability().won_trades_percent(),
                    (long long)s.profitability().won_trades(),
                    (long long)s.profitability().lost_trades());
    return 0;
}
```

### 2) Since a date (UTC)

```cpp
pb::StatsData statsSince(MT4Account& account, const std::string& isoUtc) {
    pb::TradeStatsRequest req;
    req.set_from_utc(isoUtc);                 // e.g. "2026-01-01T00:00:00"
    return account.tradeStats(req);
}
```

### 3) Drawdown summary

```cpp
void drawdowns(const pb::StatsData& s) {
    std::printf("Balance DD: %.2f (%.1f%%)   Equity DD: %.2f (%.1f%%)\n",
                s.max_balance_drawdown_raw(), s.max_balance_drawdown_relative(),
                s.max_equity_drawdown_raw(),  s.max_equity_drawdown_relative());
}
```

### 4) Expectancy & averages

```cpp
void edge(const pb::StatsData& s) {
    if (s.has_expectancy())
        std::printf("Expectancy: %.2f pips / $%.2f per trade\n",
                    s.expectancy().pips(), s.expectancy().dollar());
    if (s.has_average_win() && s.has_average_lost())
        std::printf("Avg win $%.2f  /  avg loss $%.2f\n",
                    s.average_win().average_usd(), s.average_lost().average_usd());
}
```

### 5) Period profits from the summary

```cpp
void periodProfit(const pb::StatsData& s) {
    if (!s.has_summary()) return;
    const auto& sm = s.summary();
    std::printf("Day %.2f  Week %.2f  Month %.2f  Total %.2f  (open %d)\n",
                sm.day_profit(), sm.week_profit(), sm.month_profit(),
                sm.total_profit(), sm.open_trades());
}
```

### 6) Error handling

```cpp
bool tryStats(MT4Account& account, pb::StatsData& out) {
    try { out = account.tradeStats(pb::TradeStatsRequest{}); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### One-line health verdict

```cpp
const char* verdict(const pb::StatsData& s) {
    if (s.trades() == 0)        return "no trades yet";
    if (s.profit_factor() >= 1.5) return "strong";
    if (s.profit_factor() >= 1.0) return "profitable";
    return "losing";
}
```

---

## 📚 See Also

* [TradeStatsEquityHistory](./TradeStatsEquityHistory.md) — these stats **plus** the equity curve and trades
* [EquityHistory](./EquityHistory.md) — just the equity-curve points
* [OrdersHistory](../2.%20Orders_History/OrdersHistory.md) — the raw closed orders behind these numbers
* **Friendly layer:** `MT4Service::tradeStats(...)` returns a friendly `TradeStats` struct.
* **Sugar layer:** `MT4Sugar::getProfitToday()` / `getProfitThisWeek()` / `getProfitThisMonth()`.
