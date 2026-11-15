# 💱 Getting Many Quotes at Once

> **Request:** the latest bid/ask (+ session high/low) for **several symbols** in one round-trip (`QuoteManyData`) from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::quoteMany(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.MarketInfo`
* **Proto definition:** `QuoteMany` (defined in `mt4-term-api-market-info.proto`)

### RPC

* **Service:** `mt4_term_api.MarketInfo`
* **Method:** `QuoteMany(QuoteManyRequest) → QuoteManyReply`
* **Low-level client (generated):** `MarketInfo::Stub::QuoteMany(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC that returns a list of [`QuoteData`](./Quote.md) — one per requested symbol.
* **Why you need it.** A watchlist / multi-symbol dashboard without N separate `quote()` calls.
* **Sanity check.** `quotes_size()` matches (or closely tracks) the number of symbols you asked for.

---

## 🎯 Purpose

* Refresh a whole watchlist in a single round-trip.
* Snapshot several correlated instruments at (near) the same instant.
* Cut latency vs looping `quote()` per symbol.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Latest quotes for several symbols in one call.
    // Throws ApiError / TransportError on failure.
    pb::QuoteManyData quoteMany(const pb::QuoteManyRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
QuoteManyRequest {
  repeated string symbols = 1;   // e.g. ["EURUSD", "GBPUSD", "XAUUSD"]
}
```

**Reply message:**

```protobuf
QuoteManyReply {
  oneof response {
    QuoteManyData data  = 1;
    Error         error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                        | Description                                   |
| --------- | --------------------------- | --------------------------------------------- |
| `req`     | `const pb::QuoteManyRequest&` | Repeated `symbols`. Add each with `add_symbols(...)`. |

```cpp
pb::QuoteManyRequest req;
for (const char* s : {"EURUSD", "GBPUSD", "XAUUSD"}) req.add_symbols(s);
pb::QuoteManyData d = account.quoteMany(req);
```

**There is no `ctx` parameter.** The deadline comes from `ClientOptions::unaryTimeoutMs`; transient failures retry automatically.

---

## ⬆️ Output — `QuoteManyData`

| Field (proto) | C++ accessor | C++ type                        | Description                           |
| ------------- | ------------ | ------------------------------- | ------------------------------------- |
| `quotes`      | `quotes()`   | `RepeatedPtrField<QuoteData>`   | One quote per symbol. Use `quotes_size()` / range-for. |

Each `QuoteData` has `symbol()`, `bid()`, `ask()`, `high()`, `low()`, `date_time(_utc)` — see [Quote → Output](./Quote.md).

> **Match by `symbol()`**, not by index — don't assume the reply order equals the request order. Build a `map<symbol, QuoteData>` if you need lookups.

---

## 🧩 Notes & Tips

* **Select symbols first.** Symbols not in Market Watch may be missing or stale — call [`symbolSelect`](./SymbolSelect.md) for each once.
* **No `last`/`volume`** — same as single `quote`.
* **Don't rely on order** — key results by `symbol()`.
* **Batch size is your call** — a very large list increases payload; chunk if needed.
* **Reconnection is automatic.**

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

    pb::QuoteManyRequest req;
    for (const char* s : {"EURUSD", "GBPUSD", "USDJPY"}) req.add_symbols(s);
    pb::QuoteManyData d = account.quoteMany(req);

    for (const auto& q : d.quotes())
        std::printf("%-8s %.5f / %.5f\n", q.symbol().c_str(), q.bid(), q.ask());
    return 0;
}
```

### 2) Index by symbol for lookups

```cpp
#include <map>
std::map<std::string, pb::QuoteData> quoteMap(MT4Account& account, const std::vector<std::string>& syms) {
    pb::QuoteManyRequest req;
    for (auto& s : syms) req.add_symbols(s);
    std::map<std::string, pb::QuoteData> m;
    for (const auto& q : account.quoteMany(req).quotes()) m[q.symbol()] = q;
    return m;
}
```

### 3) Refresh a watchlist table

```cpp
void printWatchlist(MT4Account& account, const std::vector<std::string>& syms) {
    pb::QuoteManyRequest req;
    for (auto& s : syms) req.add_symbols(s);
    std::printf("%-8s %12s %12s %10s\n", "SYMBOL", "BID", "ASK", "SPREAD");
    for (const auto& q : account.quoteMany(req).quotes())
        std::printf("%-8s %12.5f %12.5f %10.5f\n",
                    q.symbol().c_str(), q.bid(), q.ask(), q.ask() - q.bid());
}
```

### 4) Widest spread in the basket

```cpp
std::string widestSpread(MT4Account& account, const std::vector<std::string>& syms) {
    pb::QuoteManyRequest req;
    for (auto& s : syms) req.add_symbols(s);
    std::string worst; double maxSpread = -1;
    for (const auto& q : account.quoteMany(req).quotes()) {
        double sp = q.ask() - q.bid();
        if (sp > maxSpread) { maxSpread = sp; worst = q.symbol(); }
    }
    return worst;
}
```

### 5) Detect a missing symbol

```cpp
#include <set>
void reportMissing(MT4Account& account, const std::vector<std::string>& want) {
    pb::QuoteManyRequest req;
    for (auto& s : want) req.add_symbols(s);
    std::set<std::string> got;
    for (const auto& q : account.quoteMany(req).quotes()) got.insert(q.symbol());
    for (auto& s : want)
        if (!got.count(s)) std::printf("⚠️ no quote for %s (select it?)\n", s.c_str());
}
```

### 6) Error handling

```cpp
bool tryQuoteMany(MT4Account& account, const std::vector<std::string>& syms, pb::QuoteManyData& out) {
    try {
        pb::QuoteManyRequest req;
        for (auto& s : syms) req.add_symbols(s);
        out = account.quoteMany(req);
        return true;
    } catch (const ApiError& e) {
        std::printf("Server rejected: %s\n", e.what());
    } catch (const TransportError& e) {
        std::printf("Transport error: %s\n", e.what());
    }
    return false;
}
```

---

## 🔧 Common Patterns

### Chunk a large watchlist

```cpp
#include <vector>
std::map<std::string, pb::QuoteData> quoteChunked(MT4Account& account,
        const std::vector<std::string>& syms, size_t chunk = 50) {
    std::map<std::string, pb::QuoteData> all;
    for (size_t i = 0; i < syms.size(); i += chunk) {
        pb::QuoteManyRequest req;
        for (size_t j = i; j < std::min(i + chunk, syms.size()); ++j) req.add_symbols(syms[j]);
        for (const auto& q : account.quoteMany(req).quotes()) all[q.symbol()] = q;
    }
    return all;
}
```

---

## 📚 See Also

* [Quote](./Quote.md) — a single symbol (shares the `QuoteData` shape)
* [SymbolSelect](./SymbolSelect.md) — enable symbols before quoting
* [Streaming · OnSymbolTick](../5.%20Streaming_Methods/OnSymbolTick.md) — subscribe to several symbols' ticks
* **Friendly layer:** `MT4Service::quoteMany(symbols)` → `std::vector<Quote>`.
* **Sugar layer:** `MT4Sugar::getPriceInfo(symbol)` per symbol (the Sugar layer reads one symbol at a time).
