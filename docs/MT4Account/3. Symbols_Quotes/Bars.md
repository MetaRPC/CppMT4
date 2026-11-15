# 📊 Fetching OHLC Bars

> **Request:** OHLC **bars** (`QuoteHistoryBarsData`, a list of `QuoteHistoryBar`) for a symbol and timeframe, from **MT4**'s dedicated `QuoteHistory` service — paginated, with raw `int64` times.

**API Information:**

* **Low-level API:** `MT4Account::bars(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.QuoteHistory`
* **Proto definition:** `ShowQuoteHistory` / bars RPC (defined in `mt4-term-api-quote-history.proto`)

### RPC

* **Service:** `mt4_term_api.QuoteHistory`
* **Method:** `Bars(QuoteHistoryBarsRequest) → QuoteHistoryBarsReply`
* **Low-level client (generated):** `QuoteHistory::Stub` bars method `(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning **OHLC bars** for a symbol/timeframe over a window, with pagination.
* **Why you need it.** Classic candlestick data for charts, indicators and backtests.
* **Sanity check.** `bars_size()` > 0; each bar's `time()` falls inside your window.

> **⏱️ Timeframe enum alert.** `bars` uses **`QUOTE_HISTORY_TIMEFRAME`** with **minute counts** (`QHB_PERIOD_H1 = 60`), **not** the index enum that [`quoteHistory`](./QuoteHistory.md) uses. The friendly `MT4Service::Timeframe` matches *this* scheme.
>
> **⚠️ `from_time` is required.** The MT4 gateway rejects a zero/implicit window (`QUOTE_HISTORY_FROM_TIME_REQUIRED`) — always set `from_time` (or `from_time_utc`). `MT4Service::lastBars()` computes a safe window for you.

---

## 🎯 Purpose

* Build candlestick charts and compute indicators (MA, RSI, ATR…).
* Backtest on OHLC data.
* Prefer `bars` over [`quoteHistory`](./QuoteHistory.md) when you want minute-count timeframes, pagination and raw `int64` times.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // OHLC bars for a symbol/timeframe (timeframe in minutes; from_time REQUIRED).
    // Throws ApiError / TransportError on failure.
    pb::QuoteHistoryBarsData bars(const pb::QuoteHistoryBarsRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
QuoteHistoryBarsRequest {
  string symbol = 1;
  QUOTE_HISTORY_TIMEFRAME timeframe = 2;   // QHB_PERIOD_* (minute counts)
  int64  from_time     = 3;                // window start, broker clock (unix sec) — REQUIRED
  int64  to_time       = 4;                // window end,   broker clock (unix sec)
  int32  page_size     = 5;                // bars per page
  string page_token    = 6;                // cursor for the next page
  int64  from_time_utc = 7;                // window start, UTC (unix sec)
  int64  to_time_utc   = 8;                // window end,   UTC (unix sec)
}
```

Times here are **raw `int64` unix seconds**, not `Timestamp`. Use the broker-clock pair *or* the UTC pair.

**Reply message:**

```protobuf
QuoteHistoryBarsReply {
  oneof response {
    QuoteHistoryBarsData data  = 1;
    Error                error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                               | Description                                   |
| --------- | ---------------------------------- | --------------------------------------------- |
| `req`     | `const pb::QuoteHistoryBarsRequest&` | Symbol, timeframe, window, paging.            |

```cpp
#include <ctime>
int64_t now  = (int64_t)std::time(nullptr);
int64_t from = now - 7 * 24 * 3600;        // last week

pb::QuoteHistoryBarsRequest req;
req.set_symbol("EURUSD");
req.set_timeframe(pb::QHB_PERIOD_H1);      // H1 = 60 here!
req.set_from_time_utc(from);               // REQUIRED
req.set_to_time_utc(now);
req.set_page_size(500);

pb::QuoteHistoryBarsData d = account.bars(req);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `QuoteHistoryBarsData`

| Field (proto)           | C++ accessor              | C++ type                          | Description                          |
| ----------------------- | ------------------------- | --------------------------------- | ------------------------------------ |
| `symbol`                | `symbol()`                | `const std::string&`              | Instrument echoed back.              |
| `timeframe`             | `timeframe()`             | `QUOTE_HISTORY_TIMEFRAME`         | The timeframe echoed back.           |
| `bars`                  | `bars()`                  | `RepeatedPtrField<QuoteHistoryBar>` | The bars. Use `bars_size()`.       |
| `next_page_token`       | `next_page_token()`       | `const std::string&`              | Cursor for the next page (empty = last). |
| `server_first_time`     | `server_first_time()`     | `int64_t`                         | Earliest available bar time (broker).|
| `server_first_time_utc` | `server_first_time_utc()` | `int64_t`                         | Earliest available bar time (UTC).   |

### `QuoteHistoryBar` (one bar)

| Field (proto) | C++ accessor    | C++ type   | Description                     |
| ------------- | --------------- | ---------- | ------------------------------- |
| `time`        | `time()`        | `int64_t`  | Bar open time, broker clock (unix sec). |
| `time_utc`    | `time_utc()`    | `int64_t`  | Bar open time, UTC (unix sec).  |
| `open`        | `open()`        | `double`   | Open price.                     |
| `high`        | `high()`        | `double`   | High price.                     |
| `low`         | `low()`         | `double`   | Low price.                      |
| `close`       | `close()`       | `double`   | Close price.                    |
| `tick_volume` | `tick_volume()` | `int64_t`  | Tick (quote) volume.            |
| `real_volume` | `real_volume()` | `int64_t`  | Real (exchange) volume, if any. |
| `spread`      | `spread()`      | `int32_t`  | Spread in points.               |

> **Times are `int64` unix seconds** — no `Timestamp` conversion needed (pass straight to `std::gmtime`).

---

## 🧱 Related enum — `QUOTE_HISTORY_TIMEFRAME`

```protobuf
enum QUOTE_HISTORY_TIMEFRAME {
  QHB_PERIOD_UNSPECIFIED = 0;
  QHB_PERIOD_M1  = 1;    QHB_PERIOD_M5  = 5;    QHB_PERIOD_M15 = 15;
  QHB_PERIOD_M30 = 30;   QHB_PERIOD_H1  = 60;   QHB_PERIOD_H4  = 240;
  QHB_PERIOD_D1  = 1440; QHB_PERIOD_W1  = 10080; QHB_PERIOD_MN1 = 43200;
}
```

> **These are minute counts.** `QHB_PERIOD_H1 = 60`, `QHB_PERIOD_D1 = 1440`. The friendly `MT4Service::Timeframe` uses the same numeric values.

---

## 🧩 Notes & Tips

* **`from_time` (or `from_time_utc`) is mandatory** — omit it and the gateway returns `QUOTE_HISTORY_FROM_TIME_REQUIRED`.
* **Minute-count timeframe** — don't pass a `QH_PERIOD_*` index here.
* **Paginate** with `page_size` + `page_token` → `next_page_token` for long ranges.
* **Raw `int64` times** — unlike `HistoryQuote`, no `Timestamp`.
* **`server_first_time`** tells you how far back the broker's history goes.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — last week of H1 bars

```cpp
#include <metarpc/mt4_account.hpp>
#include <ctime>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    int64_t now  = (int64_t)std::time(nullptr);
    pb::QuoteHistoryBarsRequest req;
    req.set_symbol("EURUSD");
    req.set_timeframe(pb::QHB_PERIOD_H1);
    req.set_from_time_utc(now - 7 * 24 * 3600);     // REQUIRED
    req.set_to_time_utc(now);
    req.set_page_size(500);

    pb::QuoteHistoryBarsData d = account.bars(req);
    std::printf("%d bars\n", d.bars_size());
    for (const auto& b : d.bars())
        std::printf("  O %.5f H %.5f L %.5f C %.5f  vol %lld\n",
                    b.open(), b.high(), b.low(), b.close(), (long long)b.tick_volume());
    return 0;
}
```

### 2) Page through a long range

```cpp
#include <vector>
std::vector<pb::QuoteHistoryBar> allBars(MT4Account& account, const std::string& sym,
        pb::QUOTE_HISTORY_TIMEFRAME tf, int64_t fromUnix, int64_t toUnix) {
    std::vector<pb::QuoteHistoryBar> out;
    pb::QuoteHistoryBarsRequest req;
    req.set_symbol(sym); req.set_timeframe(tf);
    req.set_from_time_utc(fromUnix); req.set_to_time_utc(toUnix);
    req.set_page_size(1000);
    for (;;) {
        pb::QuoteHistoryBarsData d = account.bars(req);
        for (const auto& b : d.bars()) out.push_back(b);
        if (d.next_page_token().empty()) break;
        req.set_page_token(d.next_page_token());
    }
    return out;
}
```

### 3) Print bars with UTC time

```cpp
void printBars(const pb::QuoteHistoryBarsData& d) {
    for (const auto& b : d.bars()) {
        std::time_t t = (std::time_t)b.time_utc();   // already unix seconds
        char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::gmtime(&t));
        std::printf("[%s] O%.5f H%.5f L%.5f C%.5f\n",
                    buf, b.open(), b.high(), b.low(), b.close());
    }
}
```

### 4) ATR (Average True Range)

```cpp
double atr(const pb::QuoteHistoryBarsData& d, int period) {
    int n = d.bars_size();
    if (n < period + 1) return 0;
    double sum = 0;
    for (int i = n - period; i < n; ++i) {
        const auto& cur = d.bars(i);
        const auto& prv = d.bars(i - 1);
        double tr = std::max({ cur.high() - cur.low(),
                               std::fabs(cur.high() - prv.close()),
                               std::fabs(cur.low()  - prv.close()) });
        sum += tr;
    }
    return sum / period;
}
```

### 5) Daily bars for the last 30 days

```cpp
pb::QuoteHistoryBarsData dailyBars(MT4Account& account, const std::string& sym) {
    int64_t now = (int64_t)std::time(nullptr);
    pb::QuoteHistoryBarsRequest req;
    req.set_symbol(sym);
    req.set_timeframe(pb::QHB_PERIOD_D1);          // 1440
    req.set_from_time_utc(now - 30 * 24 * 3600);
    req.set_to_time_utc(now);
    req.set_page_size(60);
    return account.bars(req);
}
```

### 6) Error handling (missing from_time is a common mistake)

```cpp
bool tryBars(MT4Account& account, const pb::QuoteHistoryBarsRequest& req, pb::QuoteHistoryBarsData& out) {
    try { out = account.bars(req); return true; }
    catch (const ApiError& e) {
        std::printf("Bars rejected: %s\n", e.what());   // e.g. QUOTE_HISTORY_FROM_TIME_REQUIRED
    } catch (const TransportError& e) {
        std::printf("Transport error: %s\n", e.what());
    }
    return false;
}
```

---

## 🔧 Common Patterns

### Last N bars (mirror of `MT4Service::lastBars`)

```cpp
pb::QuoteHistoryBarsData lastBars(MT4Account& account, const std::string& sym,
                                  pb::QUOTE_HISTORY_TIMEFRAME tfMinutes, int count) {
    int64_t now  = (int64_t)std::time(nullptr);
    int64_t span = (int64_t)tfMinutes * 60 * count;
    int64_t from = now - span * 3 - 7 * 24 * 3600;    // pad for gaps/weekends
    pb::QuoteHistoryBarsRequest req;
    req.set_symbol(sym); req.set_timeframe(tfMinutes);
    req.set_from_time_utc(from); req.set_to_time_utc(now);
    req.set_page_size(count * 2);
    return account.bars(req);
}
```

---

## 📚 See Also

* [QuoteHistory](./QuoteHistory.md) — the `MarketInfo` history variant (index timeframe enum, `Timestamp` times)
* [Symbols & Quotes — Overview](./Symbols_Quotes.Overview.md) — the two-timeframe-enum table
* [Quote](./Quote.md) — the latest price
* **Friendly layer:** `MT4Service::bars(symbol, Timeframe, fromUnix, toUnix)` / `lastBars(symbol, Timeframe, count)` → `std::vector<Bar>` (UTC times; `Timeframe` values are the minute counts used here). `lastBars` supplies the required `from_time` for you.
* **Sugar layer:** strategy helpers build on `MT4Service::lastBars()`.
