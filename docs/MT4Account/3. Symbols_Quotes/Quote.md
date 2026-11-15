# 💱 Getting a Live Quote

> **Request:** the latest **bid/ask** (+ session high/low) for **one** symbol (`QuoteData`) from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::quote(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.MarketInfo`
* **Proto definition:** `Quote` (defined in `mt4-term-api-market-info.proto`)

### RPC

* **Service:** `mt4_term_api.MarketInfo`
* **Method:** `Quote(QuoteRequest) → QuoteReply`
* **Low-level client (generated):** `MarketInfo::Stub::Quote(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning the current **bid**, **ask**, session **high** / **low** and a timestamp for a single symbol.
* **Why you need it.** Price tickers, spread checks, mid-price calc, a pre-trade price read.
* **Sanity check.** `bid()` and `ask()` are positive and `ask() >= bid()`.

> **🆚 No `last` / `volume`.** MT4 quotes carry **only** bid/ask + session high/low. There is no last-trade price or tick volume on a quote (per-symbol `volume` lives on [SymbolParamsMany](./SymbolParamsMany.md)).

---

## 🎯 Purpose

* Show a live price for one instrument.
* Compute spread (`ask − bid`) and mid (`(ask + bid) / 2`).
* Read a reference price just before sending an order.

> **Select first.** If the symbol is not in Market Watch, the quote can be empty/stale — call [`symbolSelect(sym, true)`](./SymbolSelect.md) once.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Latest quote (bid/ask + session high/low) for one symbol.
    // Throws ApiError / TransportError on failure.
    pb::QuoteData quote(const pb::QuoteRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
QuoteRequest {
  string symbol = 1;   // e.g. "EURUSD"
}
```

**Reply message:**

```protobuf
QuoteReply {
  oneof response {
    QuoteData data  = 1;
    Error     error = 2;
  }
}
```

`MT4Account` returns `data` on success and throws `ApiError` otherwise (e.g. unknown symbol).

---

## 🔽 Input

| Parameter | Type                    | Description                         |
| --------- | ----------------------- | ----------------------------------- |
| `req`     | `const pb::QuoteRequest&` | Carries `symbol`. Set it before calling. |

```cpp
pb::QuoteRequest req;
req.set_symbol("EURUSD");
pb::QuoteData d = account.quote(req);
```

**There is no `ctx` parameter.** The deadline comes from `ClientOptions::unaryTimeoutMs`; transient failures retry automatically.

---

## ⬆️ Output — `QuoteData`

| Field (proto)   | C++ accessor      | C++ type                      | Description                              |
| --------------- | ----------------- | ----------------------------- | ---------------------------------------- |
| `symbol`        | `symbol()`        | `const std::string&`          | The symbol echoed back.                  |
| `bid`           | `bid()`           | `double`                      | Current bid price.                       |
| `ask`           | `ask()`           | `double`                      | Current ask price.                       |
| `high`          | `high()`          | `double`                      | Session high.                            |
| `low`           | `low()`           | `double`                      | Session low.                             |
| `date_time`     | `date_time()`     | `google::protobuf::Timestamp` | Quote time, broker clock.                |
| `date_time_utc` | `date_time_utc()` | `google::protobuf::Timestamp` | Quote time, UTC.                         |

> **Spread in points** needs the symbol's `point()` (from [SymbolParamsMany](./SymbolParamsMany.md)): `(ask − bid) / point`.

---

## 🧩 Notes & Tips

* **Select the symbol first** for a reliable quote (see [SymbolSelect](./SymbolSelect.md)).
* **Unknown symbol throws** `ApiError`; catch it if the symbol name is user-supplied.
* **No `last`/`volume`** on MT4 quotes — use bars for volume.
* **Streaming alternative:** [OnSymbolTick](../5.%20Streaming_Methods/OnSymbolTick.md) pushes ticks so you don't poll.
* **Reconnection is automatic,** like every read.

---

## 🔗 Usage Examples

### 1) Basic usage

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::QuoteRequest req; req.set_symbol("EURUSD");
    pb::QuoteData d = account.quote(req);
    std::printf("%s  bid %.5f  ask %.5f  (hi %.5f / lo %.5f)\n",
                d.symbol().c_str(), d.bid(), d.ask(), d.high(), d.low());
    return 0;
}
```

### 2) Spread and mid-price

```cpp
void spreadMid(MT4Account& account, const std::string& sym, double point) {
    pb::QuoteRequest req; req.set_symbol(sym);
    pb::QuoteData d = account.quote(req);
    double spread = (d.ask() - d.bid()) / point;    // in points
    double mid    = (d.ask() + d.bid()) / 2.0;
    std::printf("%s  spread %.1f pts  mid %.5f\n", sym.c_str(), spread, mid);
}
```

### 3) Select-then-quote helper

```cpp
pb::QuoteData quoteSelected(MT4Account& account, const std::string& sym) {
    pb::SymbolSelectRequest sel; sel.set_symbol(sym); sel.set_select(true);
    account.symbolSelect(sel);                      // ensure it's in Market Watch
    pb::QuoteRequest q; q.set_symbol(sym);
    return account.quote(q);
}
```

### 4) Pre-trade price read

```cpp
bool priceOK(MT4Account& account, const std::string& sym, double maxSpreadPts, double point) {
    pb::QuoteRequest req; req.set_symbol(sym);
    pb::QuoteData d = account.quote(req);
    return (d.ask() - d.bid()) / point <= maxSpreadPts;   // don't trade on a blown-out spread
}
```

### 5) Quote time in UTC

```cpp
#include <ctime>
void quoteTime(const pb::QuoteData& d) {
    std::time_t t = (std::time_t)d.date_time_utc().seconds();
    char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
    std::printf("%s @ %s UTC\n", d.symbol().c_str(), buf);
}
```

### 6) Error handling (user-supplied symbol)

```cpp
bool tryQuote(MT4Account& account, const std::string& sym, pb::QuoteData& out) {
    try {
        pb::QuoteRequest req; req.set_symbol(sym);
        out = account.quote(req);
        return true;
    } catch (const ApiError& e) {
        std::printf("Quote failed for %s: %s\n", sym.c_str(), e.what());
    } catch (const TransportError& e) {
        std::printf("Transport error: %s\n", e.what());
    }
    return false;
}
```

---

## 🔧 Common Patterns

### Poll a single symbol

```cpp
#include <thread>
#include <chrono>
void tick(MT4Account& account, const std::string& sym, std::chrono::milliseconds every) {
    for (;;) {
        pb::QuoteRequest req; req.set_symbol(sym);
        pb::QuoteData d = account.quote(req);
        std::printf("\r%s %.5f / %.5f", sym.c_str(), d.bid(), d.ask());
        std::fflush(stdout);
        std::this_thread::sleep_for(every);
    }
}
```

---

## 📚 See Also

* [QuoteMany](./QuoteMany.md) — many symbols in one call
* [SymbolSelect](./SymbolSelect.md) — enable a symbol before quoting
* [SymbolParamsMany](./SymbolParamsMany.md) — `point()` / `digits()` to format and measure spread
* [Streaming · OnSymbolTick](../5.%20Streaming_Methods/OnSymbolTick.md) — pushed ticks
* **Friendly layer:** `MT4Service::quote(symbol)` → `Quote{bid, ask, high, low, timeUtc}`.
* **Sugar layer:** `MT4Sugar::getBid()` / `getAsk()` / `getSpread()` / `getPriceInfo()`.
