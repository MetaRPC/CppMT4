# 🛒 Placing an Order (Market or Pending)

> **Request:** place a **market** order (`OO_OP_BUY`/`OO_OP_SELL`) or a **pending** order (`*LIMIT`/`*STOP`) and get back the new `ticket` (`OrderSendData`), on **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::orderSend(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradingHelper`
* **Proto definition:** `OrderSend` (defined in `mt4-term-api-trading-helper.proto`)

### RPC

* **Service:** `mt4_term_api.TradingHelper`
* **Method:** `OrderSend(OrderSendRequest) → OrderSendReply`
* **Low-level client (generated):** `TradingHelper::Stub::OrderSend(context, request, response)`

---

## 🛑 No retcode — rejection throws

> There is **no success field**. If `orderSend` **returns**, the order was accepted and `OrderSendData` holds its `ticket`. If the broker **rejects** it, the call **throws `ApiError`** with the MQL4 code in `mqlErrorCode` (e.g. `130` invalid stops, `132` market closed, `134` not enough money). **Always wrap this call in `try/catch (const ApiError&)`.**

---

## 💬 Just the essentials

* **What it is.** One RPC that opens a market order or places a pending order.
* **Why you need it.** It's how you enter the market — everything else (modify/close) acts on the ticket it returns.
* **Sanity check.** A returned `OrderSendData` with `ticket() != 0` means the order is live.

---

## 🎯 Purpose

* Open a market BUY/SELL at the current price.
* Place a pending BUYLIMIT/SELLLIMIT/BUYSTOP/SELLSTOP at a chosen price (optionally with an expiry).
* Attach SL/TP, a magic number and a comment at entry time.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Place a market or pending order. Returns OrderSendData (ticket/volume/price/open_time)
    // on acceptance; THROWS ApiError (with MQL4 code) on rejection.
    pb::OrderSendData orderSend(const pb::OrderSendRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OrderSendRequest {
  string symbol = 1;
  OrderSendOperationType operation_type = 2;   // OC_OP_BUY … OC_OP_SELLSTOP
  double volume = 3;                           // lots
  optional double price       = 4;             // required for pending; for market, the reference price
  optional int32  slippage    = 5;             // max price deviation (points)
  optional double stoploss    = 6;             // SL price (0/unset = none)
  optional double takeprofit  = 7;             // TP price (0/unset = none)
  optional string comment     = 8;
  optional int32  magic_number = 9;            // EA/strategy tag
  optional google.protobuf.Timestamp expiration     = 10;  // pending expiry, broker clock
  optional google.protobuf.Timestamp expiration_utc = 11;  // pending expiry, UTC
}
```

**Reply message:**

```protobuf
OrderSendReply {
  oneof response {
    OrderSendData data  = 1;
    Error         error = 2;   // → thrown as ApiError (mqlErrorCode set)
  }
}
```

---

## 🔽 Input

| Parameter | Type                       | Description                                   |
| --------- | -------------------------- | --------------------------------------------- |
| `req`     | `const pb::OrderSendRequest&` | Symbol, operation, volume, optional price/SL/TP/slippage/magic/comment/expiry. |

```cpp
pb::OrderSendRequest req;
req.set_symbol("EURUSD");
req.set_operation_type(pb::OC_OP_BUY);
req.set_volume(0.10);
req.set_slippage(10);               // 10 points
// optional: req.set_stoploss(...); req.set_takeprofit(...); req.set_magic_number(777);
pb::OrderSendData r = account.orderSend(req);
```

**There is no `ctx` parameter.** Trades use `ClientOptions::tradeTimeoutMs` (separate from read RPCs); tune via `account.options()`.

> **⚠️ Not safe to blindly retry.** On a transport timeout the order may already be live. Re-query [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) before resending.

---

## ⬆️ Output — `OrderSendData`

| Field (proto)   | C++ accessor      | C++ type                      | Description                        |
| --------------- | ----------------- | ----------------------------- | ---------------------------------- |
| `ticket`        | `ticket()`        | `int32_t`                     | New order's ticket (your handle).  |
| `volume`        | `volume()`        | `double`                      | Filled volume (lots).              |
| `price`         | `price()`         | `double`                      | Fill price (market) / set price (pending). |
| `open_time`     | `open_time()`     | `google::protobuf::Timestamp` | Open time, broker clock.           |
| `open_time_utc` | `open_time_utc()` | `google::protobuf::Timestamp` | Open time, UTC.                    |

> There is **no status field** — the presence of this payload *is* the success signal.

---

## 🧱 Related enum — `OrderSendOperationType`

```protobuf
enum OrderSendOperationType {
  OC_OP_BUY       = 0;  OC_OP_SELL      = 1;   // market
  OC_OP_BUYLIMIT  = 2;  OC_OP_SELLLIMIT = 3;   // pending limit
  OC_OP_BUYSTOP   = 4;  OC_OP_SELLSTOP  = 5;   // pending stop
}
```

> ⚠️ **`OC_` prefix** for *sending*. When you *read* orders the type enum is `OpenedOrderType` with an **`OO_`** prefix (`OO_OP_BUY` …). Same values, different enum — don't mix them.

**Pending orders need a `price`.** A limit sits *better* than market (BUYLIMIT below, SELLLIMIT above); a stop sits *worse* (BUYSTOP above, SELLSTOP below).

---

## 🧩 Notes & Tips

* **Catch `ApiError`.** Inspect `e.mqlErrorCode` — `130` (stops), `131` (volume), `132` (market closed), `134` (no money), `138` (requote)…
* **Validate before sending.** Round `volume` to `VolumeStep` and keep SL/TP beyond `TradeStopsLevel` — see [SymbolParamsMany](../3.%20Symbols_Quotes/SymbolParamsMany.md). MT4 has no `orderCheck`.
* **Select the symbol** first ([SymbolSelect](../3.%20Symbols_Quotes/SymbolSelect.md)) so there's a live price.
* **Slippage** applies to market orders; widen it in fast markets to avoid `138` requotes.
* **SL/TP are prices, not distances** — compute them from the entry and the symbol `point`.
* **Pending expiry** — set `expiration_utc` (or `expiration`); omit for good-till-cancelled.

---

## 🔗 Usage Examples

### 1) Market BUY (with error handling)

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::OrderSendRequest req;
    req.set_symbol("EURUSD");
    req.set_operation_type(pb::OC_OP_BUY);
    req.set_volume(0.10);
    req.set_slippage(10);

    try {
        pb::OrderSendData r = account.orderSend(req);
        std::printf("Opened #%d  %.2f lots @ %.5f\n", r.ticket(), r.volume(), r.price());
    } catch (const ApiError& e) {
        std::printf("Rejected: %s (MQL4 %d)\n", e.what(), e.mqlErrorCode);
    }
    return 0;
}
```

### 2) Market SELL with SL/TP

```cpp
int sellWithStops(MT4Account& account, const std::string& sym, double lots,
                  double sl, double tp) {
    pb::OrderSendRequest req;
    req.set_symbol(sym);
    req.set_operation_type(pb::OC_OP_SELL);
    req.set_volume(lots);
    req.set_slippage(10);
    req.set_stoploss(sl);          // price
    req.set_takeprofit(tp);        // price
    try { return account.orderSend(req).ticket(); }
    catch (const ApiError& e) { std::printf("SELL failed: %s\n", e.what()); return 0; }
}
```

### 3) SL/TP from pip distances

```cpp
// buy: SL below, TP above, by pip distances
int buyWithPips(MT4Account& account, const std::string& sym, double lots,
                double entry, double point, int digits, double slPips, double tpPips) {
    double pip = (digits == 3 || digits == 5) ? point * 10 : point;
    pb::OrderSendRequest req;
    req.set_symbol(sym);
    req.set_operation_type(pb::OC_OP_BUY);
    req.set_volume(lots);
    req.set_slippage(10);
    req.set_stoploss (entry - slPips * pip);
    req.set_takeprofit(entry + tpPips * pip);
    try { return account.orderSend(req).ticket(); }
    catch (const ApiError& e) { std::printf("BUY failed: %s\n", e.what()); return 0; }
}
```

### 4) Pending BUYLIMIT with expiry

```cpp
#include <google/protobuf/util/time_util.h>
#include <ctime>
using google::protobuf::util::TimeUtil;

int buyLimit(MT4Account& account, const std::string& sym, double lots, double price) {
    pb::OrderSendRequest req;
    req.set_symbol(sym);
    req.set_operation_type(pb::OC_OP_BUYLIMIT);
    req.set_volume(lots);
    req.set_price(price);                                   // required for pending
    *req.mutable_expiration_utc() =
        TimeUtil::SecondsToTimestamp((int64_t)std::time(nullptr) + 3600);  // 1h GTC
    try { return account.orderSend(req).ticket(); }
    catch (const ApiError& e) { std::printf("pending failed: %s\n", e.what()); return 0; }
}
```

### 5) Reacting to specific MQL4 codes

```cpp
int sendWithRetry(MT4Account& account, pb::OrderSendRequest req, int maxTries = 3) {
    for (int i = 0; i < maxTries; ++i) {
        try {
            return account.orderSend(req).ticket();
        } catch (const ApiError& e) {
            if (e.mqlErrorCode == 138 /* requote */ || e.mqlErrorCode == 135 /* price changed */) {
                req.set_slippage(req.slippage() + 5);       // widen and retry
                continue;
            }
            std::printf("Giving up: %s (MQL4 %d)\n", e.what(), e.mqlErrorCode);
            break;
        }
    }
    return 0;
}
```

### 6) Tagging with a magic number & comment

```cpp
int sendTagged(MT4Account& account, const std::string& sym, double lots, int magic) {
    pb::OrderSendRequest req;
    req.set_symbol(sym);
    req.set_operation_type(pb::OC_OP_BUY);
    req.set_volume(lots);
    req.set_slippage(10);
    req.set_magic_number(magic);
    req.set_comment("my-strategy");
    try { return account.orderSend(req).ticket(); }
    catch (const ApiError&) { return 0; }
}
```

### 7) Full error classification

```cpp
int placeSafely(MT4Account& account, const pb::OrderSendRequest& req) {
    try {
        return account.orderSend(req).ticket();
    } catch (const ApiError& e) {
        switch (e.mqlErrorCode) {
            case 130: std::puts("Invalid stops — move SL/TP away from price"); break;
            case 131: std::puts("Invalid volume — round to VolumeStep");       break;
            case 132: std::puts("Market closed");                              break;
            case 134: std::puts("Not enough money");                           break;
            default:  std::printf("Rejected: %s (%d)\n", e.what(), e.mqlErrorCode);
        }
    } catch (const TransportError& e) {
        std::printf("Transport error — order may or may not be live: %s\n", e.what());
    }
    return 0;
}
```

---

## 🔧 Common Patterns

### Validate volume, then send

```cpp
int sendValidated(MT4Account& account, const std::string& sym,
                  pb::OrderSendOperationType op, double lots) {
    pb::SymbolParamsManyRequest pr; pr.set_symbol_name(sym);
    const auto& s = account.symbolParamsMany(pr).symbol_infos(0);
    if (lots < s.volumemin()) lots = s.volumemin();
    if (lots > s.volumemax()) lots = s.volumemax();
    if (s.volumestep() > 0) lots = std::round(lots / s.volumestep()) * s.volumestep();

    pb::OrderSendRequest req;
    req.set_symbol(sym); req.set_operation_type(op); req.set_volume(lots); req.set_slippage(10);
    try { return account.orderSend(req).ticket(); }
    catch (const ApiError& e) { std::printf("send failed: %s\n", e.what()); return 0; }
}
```

---

## 📚 See Also

* [OrderModify](./OrderModify.md) — set/adjust SL/TP after opening
* [OrderCloseDelete](./OrderCloseDelete.md) — close the market order / delete the pending
* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — verify the order went live
* [SymbolParamsMany](../3.%20Symbols_Quotes/SymbolParamsMany.md) / [TickValueWithSize](../3.%20Symbols_Quotes/TickValueWithSize.md) — validate volume & size risk
* **Friendly layer:** `MT4Service::orderSend(OrderRequest)` → `OrderResult` (`ok()` = ticket != 0).
* **Sugar layer:** `MT4Sugar::buyMarket()` / `sellMarket()` / `buyLimit()` / `sellLimit()` / `buyStop()` / `sellStop()` / `buyMarketWithSLTP()` / `buyMarketWithPips()` — return the ticket, `0` on rejection (never throw).
