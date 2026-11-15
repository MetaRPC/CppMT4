# 📈 Fetching Historical Quotes

> **Request:** historical quote rows (`QuoteHistoryData`, a list of `HistoryQuote`) for a symbol and timeframe over a time window, from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::quoteHistory(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.MarketInfo`
* **Proto definition:** `QuoteHistory` (defined in `mt4-term-api-market-info.proto`)

### RPC

* **Service:** `mt4_term_api.MarketInfo`
* **Method:** `QuoteHistory(QuoteHistoryRequest) → QuoteHistoryReply`
* **Low-level client (generated):** `MarketInfo::Stub::QuoteHistory(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning historical **quote rows** (`HistoryQuote`: OHLC + spread + volume, with `Timestamp` times) for a symbol/timeframe in `[from, to]`.
* **Why you need it.** Charts, indicators and backtests when you want the `MarketInfo` series (times already as `Timestamp`).
* **Sanity check.** `historical_quotes_size()` > 0 and the rows' times fall inside your window.

> **⏱️ Timeframe enum alert.** `quoteHistory` uses **`ENUM_QUOTE_HISTORY_TIMEFRAME`** (small indices: `QH_PERIOD_H1 = 4`), **not** the minute-count enum that [`bars`](./Bars.md) uses. See the overview's [two-enum table](./Symbols_Quotes.Overview.md).

---

## 🎯 Purpose

* Pull an OHLC-style quote series for charting/indicators.
* Prefer this over [`bars`](./Bars.md) when you want `Timestamp` times and the `MarketInfo` row shape; prefer `bars` when you want raw `int64` times and built-in pagination.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Historical quote rows for a symbol/timeframe over a window.
    // Throws ApiError / TransportError on failure.
    pb::QuoteHistoryData quoteHistory(const pb::QuoteHistoryRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
QuoteHistoryRequest {
  string symbol = 1;
  ENUM_QUOTE_HISTORY_TIMEFRAME timeframe = 2;   // QH_PERIOD_* (indices 0..8)
  google.protobuf.Timestamp fromTime    = 3;    // window start, broker clock
  google.protobuf.Timestamp toTime      = 4;    // window end,   broker clock
  google.protobuf.Timestamp fromTimeUtc = 5;    // window start, UTC
  google.protobuf.Timestamp toTimeUtc   = 6;    // window end,   UTC
}
```

Set **either** the broker-clock pair (`fromTime`/`toTime`) **or** the UTC pair (`fromTimeUtc`/`toTimeUtc`).

**Reply message:**

```protobuf
QuoteHistoryReply {
  oneof response {
    QuoteHistoryData data  = 1;
    Error            error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                           | Description                                 |
| --------- | ------------------------------ | ------------------------------------------- |
| `req`     | `const pb::QuoteHistoryRequest&` | Symbol, timeframe and window (see fields).  |

```cpp
#include <google/protobuf/util/time_util.h>
using google::protobuf::util::TimeUtil;

pb::QuoteHistoryRequest req;
req.set_symbol("EURUSD");
req.set_timeframe(pb::QH_PERIOD_H1);                              // H1 = 4 here!
*req.mutable_fromtimeutc() = TimeUtil::SecondsToTimestamp(fromUnix);
*req.mutable_totimeutc()   = TimeUtil::SecondsToTimestamp(toUnix);

pb::QuoteHistoryData h = account.quoteHistory(req);
```

> **Accessor note:** the proto field `fromTimeUtc` lowercases to `mutable_fromtimeutc()` / `fromtimeutc()`.

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `QuoteHistoryData`

| Field (proto)       | C++ accessor          | C++ type                        | Description                     |
| ------------------- | --------------------- | ------------------------------- | ------------------------------- |
| `historical_quotes` | `historical_quotes()` | `RepeatedPtrField<HistoryQuote>`| The quote rows. Use `historical_quotes_size()`. |

### `HistoryQuote` (one row)

| Field (proto)  | C++ accessor    | C++ type                      | Description                      |
| -------------- | --------------- | ----------------------------- | -------------------------------- |
| `symbol`       | `symbol()`      | `const std::string&`          | Instrument.                      |
| `index`        | `index()`       | `int32_t`                     | Row index in the series.         |
| `time`         | `time()`        | `google::protobuf::Timestamp` | Row time, broker clock.          |
| `time_utc`     | `time_utc()`    | `google::protobuf::Timestamp` | Row time, UTC.                   |
| `open`         | `open()`        | `double`                      | Open price.                      |
| `high`         | `high()`        | `double`                      | High price.                      |
| `low`          | `low()`         | `double`                      | Low price.                       |
| `close`        | `close()`       | `double`                      | Close price.                     |
| `spread`       | `spread()`      | `int32_t`                     | Spread in points.                |
| `tick_volume`  | `tick_volume()` | `int64_t`                     | Tick (quote) volume.             |
| `real_volume`  | `real_volume()` | `int64_t`                     | Real (exchange) volume, if any.  |

> Unlike [`QuoteHistoryBar`](./Bars.md), `HistoryQuote::time()` is a `Timestamp` (not raw `int64`).

---

## 🧱 Related enum — `ENUM_QUOTE_HISTORY_TIMEFRAME`

```protobuf
enum ENUM_QUOTE_HISTORY_TIMEFRAME {
  QH_PERIOD_M1  = 0;  QH_PERIOD_M5  = 1;  QH_PERIOD_M15 = 2;  QH_PERIOD_M30 = 3;
  QH_PERIOD_H1  = 4;  QH_PERIOD_H4  = 5;  QH_PERIOD_D1  = 6;  QH_PERIOD_W1  = 7;
  QH_PERIOD_MN1 = 8;
}
```

> **These are indices, not minutes.** `QH_PERIOD_H1 = 4`. Do **not** pass `60` here — that's the [`bars`](./Bars.md) enum.

---

## 🧩 Notes & Tips

* **Bound the window.** Supply `fromTime(_utc)` / `toTime(_utc)` so you don't pull more than needed.
* **One time base.** Use the UTC pair *or* the broker-clock pair, not both.
* **Don't confuse the two history methods** — see [Bars](./Bars.md) and the overview table.
* **Select the symbol** if it isn't already in Market Watch.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — last 24h of H1 rows

```cpp
#include <metarpc/mt4_account.hpp>
#include <google/protobuf/util/time_util.h>
#include <ctime>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;
using google::protobuf::util::TimeUtil;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    int64_t now  = (int64_t)std::time(nullptr);
    int64_t from = now - 24 * 3600;

    pb::QuoteHistoryRequest req;
    req.set_symbol("EURUSD");
    req.set_timeframe(pb::QH_PERIOD_H1);
    *req.mutable_fromtimeutc() = TimeUtil::SecondsToTimestamp(from);
    *req.mutable_totimeutc()   = TimeUtil::SecondsToTimestamp(now);

    pb::QuoteHistoryData h = account.quoteHistory(req);
    std::printf("%d rows\n", h.historical_quotes_size());
    for (const auto& q : h.historical_quotes())
        std::printf("  O %.5f H %.5f L %.5f C %.5f  vol %lld\n",
                    q.open(), q.high(), q.low(), q.close(), (long long)q.tick_volume());
    return 0;
}
```

### 2) Highest high / lowest low over the window

```cpp
void rangeHL(const pb::QuoteHistoryData& h) {
    double hi = -1e18, lo = 1e18;
    for (const auto& q : h.historical_quotes()) { hi = std::max(hi, q.high()); lo = std::min(lo, q.low()); }
    std::printf("Range: %.5f .. %.5f\n", lo, hi);
}
```

### 3) Simple moving average of closes

```cpp
double smaClose(const pb::QuoteHistoryData& h, int period) {
    int n = h.historical_quotes_size();
    if (n < period) return 0;
    double sum = 0;
    for (int i = n - period; i < n; ++i) sum += h.historical_quotes(i).close();
    return sum / period;
}
```

### 4) Print rows with UTC time

```cpp
void printRows(const pb::QuoteHistoryData& h) {
    for (const auto& q : h.historical_quotes()) {
        std::time_t t = (std::time_t)q.time_utc().seconds();
        char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::gmtime(&t));
        std::printf("[%s] C %.5f\n", buf, q.close());
    }
}
```

### 5) Error handling

```cpp
bool tryHistory(MT4Account& account, const pb::QuoteHistoryRequest& req, pb::QuoteHistoryData& out) {
    try { out = account.quoteHistory(req); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Window helper (UTC)

```cpp
pb::QuoteHistoryRequest hist(const std::string& sym, pb::ENUM_QUOTE_HISTORY_TIMEFRAME tf,
                             int64_t fromUnix, int64_t toUnix) {
    pb::QuoteHistoryRequest req;
    req.set_symbol(sym);
    req.set_timeframe(tf);
    *req.mutable_fromtimeutc() = TimeUtil::SecondsToTimestamp(fromUnix);
    *req.mutable_totimeutc()   = TimeUtil::SecondsToTimestamp(toUnix);
    return req;
}
```

---

## 📚 See Also

* [Bars](./Bars.md) — OHLC bars via the dedicated `QuoteHistory` service (minute-count timeframe enum, pagination)
* [Quote](./Quote.md) / [QuoteMany](./QuoteMany.md) — the *latest* price
* [Symbols & Quotes — Overview](./Symbols_Quotes.Overview.md) — the two-timeframe-enum table
* **Friendly layer:** `MT4Service::bars(symbol, Timeframe, fromUnix, toUnix)` / `lastBars(symbol, Timeframe, count)` return `std::vector<Bar>` with UTC times (the Service layer standardizes on the bars path).
