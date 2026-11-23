# 🧾 Getting Full Symbol Parameters

> **Request:** the full specification (67 fields) of one, several, or all symbols (`SymbolParamsManyData` — a list of `SymbolParamsManyInfo`), from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::symbolParamsMany(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.AccountHelper`
* **Proto definition:** `SymbolParamsMany` (defined in `mt4-term-api-account-helper.proto`)

### RPC

* **Service:** `mt4_term_api.AccountHelper`
* **Method:** `SymbolParamsMany(SymbolParamsManyRequest) → SymbolParamsManyReply`
* **Low-level client (generated):** `AccountHelper::Stub::SymbolParamsMany(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning the **complete contract spec** per symbol: digits, point, tick value/size, contract size, volume limits, margin, currencies, trade mode and more (67 fields).
* **Why you need it.** Format prices correctly, size positions, compute P&L and margin **client-side** (MT4 has no server calculators), and know a symbol's trading rules.
* **Sanity check.** `symbol_infos_size()` ≥ 1; `symbolname()` and `digits()` are set.

---

## 🎯 Purpose

* Get `digits()` / `point()` to format and measure prices in points/pips.
* Get `tradetickvalue()` / `tradeticksize()` / `tradecontractsize()` for P&L and sizing.
* Get `volumemin()` / `volumemax()` / `volumestep()` to validate order volume.
* Get `margininitial()`, `trademode()`, currencies — the full trading rulebook.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Full parameters for one symbol (set symbol_name) or ALL symbols (leave it unset).
    // Throws ApiError / TransportError on failure.
    pb::SymbolParamsManyData symbolParamsMany(const pb::SymbolParamsManyRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
SymbolParamsManyRequest {
  optional string symbol_name = 1;   // one symbol; omit to get them ALL
}
```

**Reply message:**

```protobuf
SymbolParamsManyReply {
  oneof response {
    SymbolParamsManyData data  = 1;
    Error                error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                                | Description                                            |
| --------- | ----------------------------------- | ------------------------------------------------------ |
| `req`     | `const pb::SymbolParamsManyRequest&`  | Set `symbol_name` for one symbol; leave unset for all. |

```cpp
pb::SymbolParamsManyRequest req;
req.set_symbol_name("EURUSD");        // or leave unset → every symbol
pb::SymbolParamsManyData d = account.symbolParamsMany(req);
const pb::SymbolParamsManyInfo& s = d.symbol_infos(0);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `SymbolParamsManyData`

| Field (proto)  | C++ accessor     | C++ type                                | Description                  |
| -------------- | ---------------- | --------------------------------------- | ---------------------------- |
| `symbol_infos` | `symbol_infos()` | `RepeatedPtrField<SymbolParamsManyInfo>`| One entry per symbol. Use `symbol_infos_size()`. |

### `SymbolParamsManyInfo` — key fields

> 67 fields in total. The proto names are **CamelCase** but the C++ accessors are **all-lowercase**. The most-used ones:

| Field (proto)       | C++ accessor          | C++ type                      | Description                                   |
| ------------------- | --------------------- | ----------------------------- | --------------------------------------------- |
| `SymbolName`        | `symbolname()`        | `const std::string&`          | Symbol name.                                  |
| `Digits`            | `digits()`            | `int32_t`                     | Price decimal places.                         |
| `Point`             | `point()`             | `double`                      | Point size (e.g. `0.00001`).                  |
| `Bid`               | `bid()`               | `double`                      | Current bid (snapshot).                       |
| `TheLast`           | `thelast()`           | `double`                      | Last price (snapshot).                        |
| `TradeTickValue`    | `tradetickvalue()`    | `double`                      | Value of one tick in account currency.        |
| `TradeTickSize`     | `tradeticksize()`     | `double`                      | Minimal price change (tick size).             |
| `TradeContractSize` | `tradecontractsize()` | `double`                      | Contract size (e.g. `100000` for FX).         |
| `VolumeMin`         | `volumemin()`         | `double`                      | Minimum order volume (lots).                  |
| `VolumeMax`         | `volumemax()`         | `double`                      | Maximum order volume (lots).                  |
| `VolumeStep`        | `volumestep()`        | `double`                      | Volume increment (lots).                      |
| `MarginInitial`     | `margininitial()`     | `double`                      | Initial margin per lot (if provided).         |
| `CurrencyBase`      | `currencybase()`      | `const std::string&`          | Base currency.                                |
| `CurrencyProfit`    | `currencyprofit()`    | `const std::string&`          | Profit currency.                              |
| `TradeCalcMode`     | `tradecalcmode()`     | `SP_ENUM_TRADE_CALC_MODE`     | Margin calc mode (Forex/Futures/CFD).         |
| `TradeMode`         | `trademode()`         | `SP_ENUM_SYMBOL_TRADE_MODE`   | Disabled / LongOnly / ShortOnly / CloseOnly / Full. |
| `SpreadFloat`       | `spreadfloat()`       | `bool`                        | `true` for a floating spread.                 |
| `is_selected`       | `is_selected()`       | `bool`                        | In Market Watch?                              |
| `Time`              | `time()`              | `int64_t`                     | Snapshot time, broker clock (**raw int64**).  |
| `TimeUtc`           | `timeutc()`           | `int64_t`                     | Snapshot time, UTC (**raw int64**).           |

> **Other fields** (~50 more) cover session stats (`SessionOpen/Close/Volume…`), swaps (`SwapLong/Short`, `SwapMode`), full margin set (`MarginMaintenance/Long/Short/Stop…`), stops/freeze levels (`TradeStopsLevel`, `TradeFreezeLevel`), bid/ask highs & lows, `Isin`, `Path`, `SymDescription`, etc. `StartTime` / `ExpirationTime` are `Timestamp`.

---

## 🧱 Related enums (from proto)

```protobuf
enum SP_ENUM_TRADE_CALC_MODE {
  SYMBOL_TRADE_MARGINE_CALC_MODE_FOREX   = 0;
  SYMBOL_TRADE_MARGINE_CALC_MODE_FUTURES = 1;
  SYMBOL_TRADE_MARGINE_CALC_MODE_CFD     = 2;
}

enum SP_ENUM_SYMBOL_TRADE_MODE {
  SYMBOL_TRADE_MODE_DISABLED  = 0;  SYMBOL_TRADE_MODE_LONGONLY  = 1;
  SYMBOL_TRADE_MODE_SHORTONLY = 2;  SYMBOL_TRADE_MODE_CLOSEONLY = 3;
  SYMBOL_TRADE_MODE_FULL      = 4;
}

enum SP_ENUM_SYMBOL_TRADE_EXECUTION {  // TradeExeMode
  SYMBOL_TRADE_EXECUTION_REQUEST  = 0;  SYMBOL_TRADE_EXECUTION_INSTANT  = 1;
  SYMBOL_TRADE_EXECUTION_MARKET   = 2;  SYMBOL_TRADE_EXECUTION_EXCHANGE = 3;
}
```

---

## 🧩 Notes & Tips

* **All-lowercase accessors** — the CamelCase proto names (`TradeTickValue`) become `tradetickvalue()`. Mind the `Time` / `TimeUtc` are **`int64`**, while `StartTime` / `ExpirationTime` are `Timestamp`.
* **Client-side P&L / sizing** — MT4 has no `orderCalcProfit` / `orderCalcMargin`; combine these fields with [TickValueWithSize](./TickValueWithSize.md). The `MT4Service` / `MT4Sugar` layers already do this.
* **Pip vs point** — a pip is 10 points for 3/5-digit symbols; use `digits()` to decide.
* **One vs all** — fetching all symbols is heavier; set `symbol_name` when you only need one.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — one symbol's spec

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::SymbolParamsManyRequest req;
    req.set_symbol_name("EURUSD");
    const auto& s = account.symbolParamsMany(req).symbol_infos(0);

    std::printf("%s  digits=%d  point=%.5f  contract=%.0f\n",
                s.symbolname().c_str(), s.digits(), s.point(), s.tradecontractsize());
    std::printf("  volume: min %.2f / max %.2f / step %.2f\n",
                s.volumemin(), s.volumemax(), s.volumestep());
    std::printf("  tick: value %.5f / size %.5f   margin/lot %.2f\n",
                s.tradetickvalue(), s.tradeticksize(), s.margininitial());
    return 0;
}
```

### 2) Pip size from digits

```cpp
double pipSize(const pb::SymbolParamsManyInfo& s) {
    // 3/5-digit symbols: 1 pip = 10 points
    return (s.digits() == 3 || s.digits() == 5) ? s.point() * 10.0 : s.point();
}
```

### 3) Validate / round an order volume

```cpp
double clampVolume(const pb::SymbolParamsManyInfo& s, double lots) {
    if (lots < s.volumemin()) lots = s.volumemin();
    if (lots > s.volumemax()) lots = s.volumemax();
    double step = s.volumestep();
    if (step > 0) lots = std::round(lots / step) * step;
    return lots;
}
```

### 4) Estimate P&L for a price move

```cpp
// profit ≈ (priceMove / tickSize) * tickValue * lots
double estProfit(const pb::SymbolParamsManyInfo& s, double priceMove, double lots) {
    if (s.tradeticksize() == 0) return 0;
    return (priceMove / s.tradeticksize()) * s.tradetickvalue() * lots;
}
```

### 5) Is the symbol tradable right now?

```cpp
bool tradable(const pb::SymbolParamsManyInfo& s) {
    return s.trademode() == pb::SYMBOL_TRADE_MODE_FULL;
}
```

### 6) Scan all symbols (unset name)

```cpp
void listContracts(MT4Account& account) {
    pb::SymbolParamsManyRequest req;            // no symbol_name → all symbols
    pb::SymbolParamsManyData d = account.symbolParamsMany(req);
    std::printf("%d symbols\n", d.symbol_infos_size());
    for (const auto& s : d.symbol_infos())
        std::printf("  %-10s d=%d contract=%.0f base=%s\n",
                    s.symbolname().c_str(), s.digits(),
                    s.tradecontractsize(), s.currencybase().c_str());
}
```

### 7) Error handling

```cpp
bool tryParams(MT4Account& account, const std::string& sym, pb::SymbolParamsManyInfo& out) {
    try {
        pb::SymbolParamsManyRequest req; req.set_symbol_name(sym);
        auto d = account.symbolParamsMany(req);
        if (d.symbol_infos_size() == 0) return false;
        out = d.symbol_infos(0);
        return true;
    } catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch   (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Cache specs by symbol

```cpp
#include <map>
std::map<std::string, pb::SymbolParamsManyInfo> specCache(MT4Account& account) {
    std::map<std::string, pb::SymbolParamsManyInfo> m;
    for (const auto& s : account.symbolParamsMany(pb::SymbolParamsManyRequest{}).symbol_infos())
        m[s.symbolname()] = s;
    return m;   // reuse for formatting / sizing without re-fetching
}
```

---

## 📚 See Also

* [TickValueWithSize](./TickValueWithSize.md) — a lighter call for just tick value / size / contract size
* [Symbols](./Symbols.md) — the list of symbol names
* [Trading · OrderSend](../4.%20Trading_Operations/OrderSend.md) — sizing/validation uses these fields
* **Friendly layer:** `MT4Service::symbolParams(symbol)` → `SymbolInfo` (the useful subset); `symbolParamsMany(nameFilter)` → `std::vector<SymbolInfo>`.
* **Sugar layer:** `MT4Sugar::getMaxLotSize()` and risk helpers build on these fields.
