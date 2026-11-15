# 🪙 Getting Tick Value & Size

> **Request:** tick value, tick size and contract size for several symbols (`TickValueWithSizeData`), from **MT4** — the inputs for client-side P&L and position sizing.

**API Information:**

* **Low-level API:** `MT4Account::tickValueWithSize(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.AccountHelper`
* **Proto definition:** `TickValueWithSize` (defined in `mt4-term-api-account-helper.proto`)

### RPC

* **Service:** `mt4_term_api.AccountHelper`
* **Method:** `TickValueWithSize(TickValueWithSizeRequest) → TickValueWithSizeReply`
* **Low-level client (generated):** `AccountHelper::Stub::TickValueWithSize(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning, per symbol, the **tick value**, **tick size** and **contract size** — exactly what you need to turn a price move into money.
* **Why you need it.** MT4 has **no server-side P&L / margin calculator**; you compute P&L and lot sizes yourself from these numbers.
* **Sanity check.** `infos_size()` matches your request; `tradetickvalue()` > 0.

---

## 🎯 Purpose

* Convert a price move to account-currency P&L: `(move / tickSize) × tickValue × lots`.
* Size a position for a target risk: `lots = riskMoney / (stopPoints × tickValue)`.
* Do it for a **basket** of symbols in one call.

> For the *full* spec (digits, point, volume limits, margin, currencies), use [SymbolParamsMany](./SymbolParamsMany.md). `TickValueWithSize` is the lighter, focused call.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Tick value / tick size / contract size for several symbols.
    // Throws ApiError / TransportError on failure.
    pb::TickValueWithSizeData tickValueWithSize(const pb::TickValueWithSizeRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
TickValueWithSizeRequest {
  repeated string symbol_names = 1;   // e.g. ["EURUSD", "XAUUSD"]
}
```

**Reply message:**

```protobuf
TickValueWithSizeReply {
  oneof response {
    TickValueWithSizeData data  = 1;
    Error                 error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                                 | Description                                   |
| --------- | ------------------------------------ | --------------------------------------------- |
| `req`     | `const pb::TickValueWithSizeRequest&`  | Repeated `symbol_names`. Add with `add_symbol_names(...)`. |

```cpp
pb::TickValueWithSizeRequest req;
req.add_symbol_names("EURUSD");
req.add_symbol_names("XAUUSD");
pb::TickValueWithSizeData d = account.tickValueWithSize(req);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `TickValueWithSizeData`

| Field (proto) | C++ accessor | C++ type                                      | Description                       |
| ------------- | ------------ | --------------------------------------------- | --------------------------------- |
| `infos`       | `infos()`    | `RepeatedPtrField<SymbolTickValueWithSizeInfo>`| One entry per symbol. Use `infos_size()`. |

### `SymbolTickValueWithSizeInfo` (one symbol)

| Field (proto)       | C++ accessor          | C++ type             | Description                              |
| ------------------- | --------------------- | -------------------- | ---------------------------------------- |
| `index`             | `index()`             | `int32_t`            | Index of the symbol in the request.      |
| `symbolName`        | `symbolname()`        | `const std::string&` | Symbol name. **Accessor is lowercase.**  |
| `TradeTickValue`    | `tradetickvalue()`    | `double`             | Value of one tick, in account currency.  |
| `TradeTickSize`     | `tradeticksize()`     | `double`             | Minimal price change (tick size).        |
| `TradeContractSize` | `tradecontractsize()` | `double`             | Contract size (e.g. `100000` for FX).    |

> **Match by `symbolname()`**, not by position, if you need per-symbol lookups.

---

## 🧩 Notes & Tips

* **Lowercase accessors** — `symbolName → symbolname()`, `TradeTickValue → tradetickvalue()`, etc.
* **This is the P&L building block** — combine with a price move and lots (see examples).
* **Batch friendly** — ask for several symbols at once.
* **Need more?** Use [SymbolParamsMany](./SymbolParamsMany.md) for digits/point/volume/margin.
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

    pb::TickValueWithSizeRequest req;
    req.add_symbol_names("EURUSD");
    req.add_symbol_names("XAUUSD");

    for (const auto& i : account.tickValueWithSize(req).infos())
        std::printf("%-8s tickVal %.5f  tickSize %.5f  contract %.0f\n",
                    i.symbolname().c_str(), i.tradetickvalue(),
                    i.tradeticksize(), i.tradecontractsize());
    return 0;
}
```

### 2) Price move → money

```cpp
double moveToMoney(const pb::SymbolTickValueWithSizeInfo& i, double priceMove, double lots) {
    if (i.tradeticksize() == 0) return 0;
    return (priceMove / i.tradeticksize()) * i.tradetickvalue() * lots;
}
// e.g. a +0.0010 move on 0.50 lots
```

### 3) Risk-based position sizing

```cpp
// lots so that a `stopPoints`-wide stop risks `riskMoney` of account currency
double lotsForRisk(const pb::SymbolTickValueWithSizeInfo& i, double riskMoney,
                   double stopPoints, double point) {
    double perLotRisk = (stopPoints * point / i.tradeticksize()) * i.tradetickvalue();
    return perLotRisk > 0 ? riskMoney / perLotRisk : 0.0;
}
```

### 4) Index results by symbol

```cpp
#include <map>
std::map<std::string, pb::SymbolTickValueWithSizeInfo> tvMap(MT4Account& account,
        const std::vector<std::string>& syms) {
    pb::TickValueWithSizeRequest req;
    for (auto& s : syms) req.add_symbol_names(s);
    std::map<std::string, pb::SymbolTickValueWithSizeInfo> m;
    for (const auto& i : account.tickValueWithSize(req).infos()) m[i.symbolname()] = i;
    return m;
}
```

### 5) One-symbol convenience

```cpp
bool tickValue(MT4Account& account, const std::string& sym, double& tickVal, double& tickSize) {
    pb::TickValueWithSizeRequest req; req.add_symbol_names(sym);
    auto d = account.tickValueWithSize(req);
    if (d.infos_size() == 0) return false;
    tickVal  = d.infos(0).tradetickvalue();
    tickSize = d.infos(0).tradeticksize();
    return true;
}
```

### 6) Error handling

```cpp
bool tryTickValues(MT4Account& account, const std::vector<std::string>& syms, pb::TickValueWithSizeData& out) {
    try {
        pb::TickValueWithSizeRequest req;
        for (auto& s : syms) req.add_symbol_names(s);
        out = account.tickValueWithSize(req);
        return true;
    } catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch   (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### P&L of an open order from its price move

```cpp
// Pair with openedOrders(): move = (current - open) for a buy, negated for a sell.
double orderPnL(const pb::SymbolTickValueWithSizeInfo& tv,
                double openPrice, double curPrice, double lots, bool isBuy) {
    double move = isBuy ? (curPrice - openPrice) : (openPrice - curPrice);
    return (move / tv.tradeticksize()) * tv.tradetickvalue() * lots;
}
```

---

## 📚 See Also

* [SymbolParamsMany](./SymbolParamsMany.md) — the full spec (digits/point/volume/margin/currencies)
* [Quote](./Quote.md) — the current price to plug into P&L
* [Trading · OrderSend](../4.%20Trading_Operations/OrderSend.md) — sizing before you send
* **Friendly layer:** `MT4Service::tickValueWithSize(symbols)` → `std::vector<TickValueSize>`.
* **Sugar layer:** `MT4Sugar::getMaxLotSize()` and risk helpers use these numbers.
