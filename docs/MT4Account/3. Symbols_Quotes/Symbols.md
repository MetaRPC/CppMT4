# 🗂️ Listing All Symbols

> **Request:** every symbol the broker offers (`SymbolsData` — a list of name + index), from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::symbols(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.MarketInfo`
* **Proto definition:** `Symbols` (defined in `mt4-term-api-market-info.proto`)

### RPC

* **Service:** `mt4_term_api.MarketInfo`
* **Method:** `Symbols(SymbolsRequest) → SymbolsReply`
* **Low-level client (generated):** `MarketInfo::Stub::Symbols(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning the **full symbol inventory** — each entry a name plus its index.
* **Why you need it.** Populate a symbol picker, validate a user-entered symbol, or drive a scan across the whole book.
* **Sanity check.** `symbolnameinfos_size()` is in the hundreds on a typical broker.

---

## 🎯 Purpose

* Discover what is tradable on this account.
* Build a dropdown / autocomplete of symbol names.
* Validate symbols before quoting or trading.

> For the **properties** of a symbol (digits, point, contract size, margins…), follow up with [SymbolParamsMany](./SymbolParamsMany.md).

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // All symbols the broker offers (name + index).
    // Throws ApiError / TransportError on failure.
    pb::SymbolsData symbols(const pb::SymbolsRequest& req = {});
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

`SymbolsRequest {}` *(empty message — just pass the default)*

**Reply message:**

```protobuf
SymbolsReply {
  oneof response {
    SymbolsData data  = 1;
    Error       error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                      | Description                        |
| --------- | ------------------------- | ---------------------------------- |
| `req`     | `const pb::SymbolsRequest&` | Empty request. Optional — defaults to `{}`. |

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `SymbolsData`

| Field (proto)     | C++ accessor        | C++ type                          | Description                      |
| ----------------- | ------------------- | --------------------------------- | -------------------------------- |
| `SymbolNameInfos` | `symbolnameinfos()` | `RepeatedPtrField<SymbolNameInfo>`| All symbols. Use `symbolnameinfos_size()`. |

> **Accessor note:** the proto field `SymbolNameInfos` lowercases to `symbolnameinfos()`.

### `SymbolNameInfo` (one entry)

| Field (proto)   | C++ accessor     | C++ type             | Description               |
| --------------- | ---------------- | -------------------- | ------------------------- |
| `symbol_name`   | `symbol_name()`  | `const std::string&` | Symbol name, e.g. `"EURUSD"`. |
| `symbol_index`  | `symbol_index()` | `int32_t`            | Broker's symbol index.    |

---

## 🧩 Notes & Tips

* **Big list.** Cache it; it changes rarely within a session.
* **Not the same as Market Watch.** This is the *full* inventory; a symbol must still be [selected](./SymbolSelect.md) before it quotes/trades.
* **Names are case-sensitive** as the broker spells them.
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

    pb::SymbolsData s = account.symbols();
    std::printf("Broker offers %d symbols\n", s.symbolnameinfos_size());
    for (int i = 0; i < 10 && i < s.symbolnameinfos_size(); ++i)
        std::printf("  [%d] %s\n", s.symbolnameinfos(i).symbol_index(),
                                   s.symbolnameinfos(i).symbol_name().c_str());
    return 0;
}
```

### 2) Does a symbol exist?

```cpp
#include <set>
bool symbolExists(MT4Account& account, const std::string& name) {
    for (const auto& si : account.symbols().symbolnameinfos())
        if (si.symbol_name() == name) return true;
    return false;
}
```

### 3) Collect names into a vector

```cpp
#include <vector>
std::vector<std::string> allSymbolNames(MT4Account& account) {
    std::vector<std::string> names;
    for (const auto& si : account.symbols().symbolnameinfos())
        names.push_back(si.symbol_name());
    return names;
}
```

### 4) Filter by substring (e.g. all USD pairs)

```cpp
std::vector<std::string> withSubstr(MT4Account& account, const std::string& needle) {
    std::vector<std::string> out;
    for (const auto& si : account.symbols().symbolnameinfos())
        if (si.symbol_name().find(needle) != std::string::npos)
            out.push_back(si.symbol_name());
    return out;
}
// withSubstr(account, "USD");
```

### 5) Error handling

```cpp
bool trySymbols(MT4Account& account, pb::SymbolsData& out) {
    try { out = account.symbols(); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Validate then select

```cpp
bool ensureTradable(MT4Account& account, const std::string& sym) {
    if (!symbolExists(account, sym)) return false;
    pb::SymbolSelectRequest sel; sel.set_symbol(sym); sel.set_select(true);
    return account.symbolSelect(sel).success();
}
```

---

## 📚 See Also

* [SymbolSelect](./SymbolSelect.md) — enable a symbol in Market Watch
* [SymbolParamsMany](./SymbolParamsMany.md) — full properties for the symbols
* [Quote](./Quote.md) / [QuoteMany](./QuoteMany.md) — prices once selected
* **Friendly layer:** `MT4Service::symbolList()` → `std::vector<std::string>`.
* **Sugar layer:** symbol discovery usually goes through `MT4Service::symbolList()`.
