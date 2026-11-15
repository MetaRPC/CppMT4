# ➕ Selecting a Symbol (Market Watch)

> **Request:** add or remove a symbol from **Market Watch** (`SymbolSelectData` with a `success` flag), on **MT4**. A symbol must be selected before it reliably quotes or trades.

**API Information:**

* **Low-level API:** `MT4Account::symbolSelect(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.MarketInfo`
* **Proto definition:** `SymbolSelect` (defined in `mt4-term-api-market-info.proto`)

### RPC

* **Service:** `mt4_term_api.MarketInfo`
* **Method:** `SymbolSelect(SymbolSelectRequest) → SymbolSelectReply`
* **Low-level client (generated):** `MarketInfo::Stub::SymbolSelect(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC that toggles a symbol in **Market Watch** — `select = true` adds it, `false` removes it.
* **Why you need it.** Many quote/stream/trade calls only work for symbols the terminal is actively watching. Select first.
* **Sanity check.** `success()` is `true`.

---

## 🎯 Purpose

* Enable a symbol so [`quote`](./Quote.md) / [`quoteMany`](./QuoteMany.md) / [`onSymbolTick`](../5.%20Streaming_Methods/OnSymbolTick.md) / trading work on it.
* Tidy up Market Watch by removing symbols you no longer need.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Add (select=true) or remove (select=false) a symbol from Market Watch.
    // Throws ApiError / TransportError on failure.
    pb::SymbolSelectData symbolSelect(const pb::SymbolSelectRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
SymbolSelectRequest {
  string symbol = 1;
  bool   select = 2;   // true = add to Market Watch, false = remove
}
```

**Reply message:**

```protobuf
SymbolSelectReply {
  oneof response {
    SymbolSelectData data  = 1;
    Error            error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                          | Description                                 |
| --------- | ----------------------------- | ------------------------------------------- |
| `req`     | `const pb::SymbolSelectRequest&` | `symbol` + `select` (true to add, false to remove). |

```cpp
pb::SymbolSelectRequest req;
req.set_symbol("EURUSD");
req.set_select(true);
bool ok = account.symbolSelect(req).success();
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `SymbolSelectData`

| Field (proto) | C++ accessor | C++ type | Description                          |
| ------------- | ------------ | -------- | ------------------------------------ |
| `success`     | `success()`  | `bool`   | `true` if the operation succeeded.   |

> A `false` means the broker declined (e.g. unknown symbol); an outright error throws `ApiError`.

---

## 🧩 Notes & Tips

* **Select before quoting/trading** — this is the most common fix for "empty quote" or a rejected trade on an otherwise valid symbol.
* **Idempotent.** Selecting an already-selected symbol just returns `success = true`.
* **Give it a moment.** After selecting a fresh symbol, the first quote may lag a tick while the terminal subscribes.
* **Removing** a symbol you have open orders/positions on is generally a bad idea — keep it watched.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — add to Market Watch

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::SymbolSelectRequest req;
    req.set_symbol("XAUUSD");
    req.set_select(true);
    std::printf("select XAUUSD → %s\n", account.symbolSelect(req).success() ? "ok" : "declined");
    return 0;
}
```

### 2) Select helper

```cpp
bool select(MT4Account& account, const std::string& sym, bool on = true) {
    pb::SymbolSelectRequest req;
    req.set_symbol(sym);
    req.set_select(on);
    return account.symbolSelect(req).success();
}
```

### 3) Select then quote

```cpp
pb::QuoteData quoteAfterSelect(MT4Account& account, const std::string& sym) {
    select(account, sym, true);
    pb::QuoteRequest q; q.set_symbol(sym);
    return account.quote(q);
}
```

### 4) Select a whole watchlist

```cpp
void selectAll(MT4Account& account, const std::vector<std::string>& syms) {
    for (auto& s : syms)
        if (!select(account, s, true))
            std::printf("⚠️ could not select %s\n", s.c_str());
}
```

### 5) Remove a symbol

```cpp
bool unwatch(MT4Account& account, const std::string& sym) {
    return select(account, sym, false);
}
```

### 6) Error handling

```cpp
bool trySelect(MT4Account& account, const std::string& sym) {
    try {
        return select(account, sym, true);
    } catch (const ApiError& e) {
        std::printf("Select failed for %s: %s\n", sym.c_str(), e.what());
    } catch (const TransportError& e) {
        std::printf("Transport error: %s\n", e.what());
    }
    return false;
}
```

---

## 🔧 Common Patterns

### Ensure-selected before any price/trade op

```cpp
bool ensureSelected(MT4Account& account, const std::string& sym) {
    return select(account, sym, true);   // safe to call every time; idempotent
}
```

---

## 📚 See Also

* [Quote](./Quote.md) / [QuoteMany](./QuoteMany.md) — need the symbol selected
* [Symbols](./Symbols.md) — discover valid symbol names first
* [Streaming · OnSymbolTick](../5.%20Streaming_Methods/OnSymbolTick.md) — also needs a selected symbol
* **Friendly layer:** `MT4Service::symbolSelect(symbol, enable)` → `bool`.
* **Sugar layer:** the price/trade helpers select symbols for you where needed.
