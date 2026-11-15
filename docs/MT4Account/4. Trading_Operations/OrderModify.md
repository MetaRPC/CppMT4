# ✏️ Modifying an Order (SL / TP / Price / Expiry)

> **Request:** change an order's **stop-loss / take-profit** — and, for a pending order, its **price / expiration** — by ticket (`OrderModifyData`), on **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::orderModify(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradingHelper`
* **Proto definition:** `OrderModify` (defined in `mt4-term-api-trading-helper.proto`)

### RPC

* **Service:** `mt4_term_api.TradingHelper`
* **Method:** `OrderModify(OrderModifyRequest) → OrderModifyReply`
* **Low-level client (generated):** `TradingHelper::Stub::OrderModify(context, request, response)`

---

## 🛑 No retcode — rejection throws

> On acceptance you get `OrderModifyData{ order_was_modified }`. On rejection (e.g. **`130` invalid stops** when SL/TP is too close to price), the call **throws `ApiError`** with the MQL4 code. Wrap it in `try/catch (const ApiError&)`.

---

## 💬 Just the essentials

* **What it is.** One RPC to update an existing order's SL/TP (and a pending order's price/expiry).
* **Why you need it.** Move a stop to break-even, trail it, retarget a TP, or re-price a pending order.
* **Sanity check.** `order_was_modified()` is `true`.

---

## 🎯 Purpose

* Set or move **SL / TP** on a live market order.
* Re-price or re-time a **pending** order before it triggers.
* Implement break-even and trailing-stop logic.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Modify an order's SL/TP (and price/expiration for a pending order) by ticket.
    // Returns OrderModifyData{order_was_modified}; THROWS ApiError on rejection.
    pb::OrderModifyData orderModify(const pb::OrderModifyRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OrderModifyRequest {
  int32  order_ticket = 1;                     // which order
  optional double new_price       = 2;         // pending orders only
  optional double new_stop_loss   = 3;
  optional double new_take_profit = 4;
  optional google.protobuf.Timestamp new_expiration     = 5;   // pending only, broker clock
  optional google.protobuf.Timestamp new_expiration_utc = 6;   // pending only, UTC
}
```

Only the fields you set are changed. (Because they are `optional`, set the ones you intend to modify.)

**Reply message:**

```protobuf
OrderModifyReply {
  oneof response {
    OrderModifyData data  = 1;   // { bool order_was_modified }
    Error           error = 2;   // → thrown as ApiError
  }
}
```

---

## 🔽 Input

| Parameter | Type                         | Description                                 |
| --------- | ---------------------------- | ------------------------------------------- |
| `req`     | `const pb::OrderModifyRequest&` | Ticket + the field(s) to change.            |

```cpp
pb::OrderModifyRequest req;
req.set_order_ticket(ticket);
req.set_new_stop_loss(1.09500);
req.set_new_take_profit(1.10500);
bool ok = account.orderModify(req).order_was_modified();
```

**There is no `ctx` parameter.** Trades use `ClientOptions::tradeTimeoutMs`.

---

## ⬆️ Output — `OrderModifyData`

| Field (proto)        | C++ accessor           | C++ type | Description                           |
| -------------------- | ---------------------- | -------- | ------------------------------------- |
| `order_was_modified` | `order_was_modified()` | `bool`   | `true` if the change was applied.     |

---

## 🧩 Notes & Tips

* **SL/TP are prices, not distances.** Compute them from the symbol's `point` / `digits`.
* **Respect the stops level.** SL/TP closer than `TradeStopsLevel` to the market → `ApiError` `130`. See [SymbolParamsMany](../3.%20Symbols_Quotes/SymbolParamsMany.md).
* **`new_price` / `new_expiration` are for pending orders.** Changing the price of a *market* order is not meaningful.
* **No-op modifies may be rejected** — MT4 can return an error if nothing actually changed.
* **Sides matter.** For a buy, SL must be below and TP above the market; reversed for a sell.
* **Catch `ApiError`** and read `e.mqlErrorCode`.

---

## 🔗 Usage Examples

### 1) Set SL and TP

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

bool setStops(MT4Account& account, int ticket, double sl, double tp) {
    pb::OrderModifyRequest req;
    req.set_order_ticket(ticket);
    req.set_new_stop_loss(sl);
    req.set_new_take_profit(tp);
    try { return account.orderModify(req).order_was_modified(); }
    catch (const ApiError& e) { std::printf("modify failed: %s (%d)\n", e.what(), e.mqlErrorCode); return false; }
}
```

### 2) Move stop to break-even

```cpp
// pull SL to the entry price once the trade is in profit
bool toBreakEven(MT4Account& account, const pb::OpenedOrderInfo& o) {
    pb::OrderModifyRequest req;
    req.set_order_ticket(o.ticket());
    req.set_new_stop_loss(o.open_price());      // SL = entry
    req.set_new_take_profit(o.take_profit());   // keep existing TP
    try { return account.orderModify(req).order_was_modified(); }
    catch (const ApiError&) { return false; }
}
```

### 3) Trailing stop by pips

```cpp
bool trail(MT4Account& account, const pb::OpenedOrderInfo& o,
           double curPrice, double point, int digits, double trailPips) {
    double pip = (digits == 3 || digits == 5) ? point * 10 : point;
    bool buy = (o.order_type() == pb::OO_OP_BUY);
    double newSL = buy ? curPrice - trailPips * pip : curPrice + trailPips * pip;

    // only move the stop in the favorable direction
    if (buy  && !(newSL > o.stop_loss())) return false;
    if (!buy && !(newSL < o.stop_loss() || o.stop_loss() == 0)) return false;

    pb::OrderModifyRequest req;
    req.set_order_ticket(o.ticket());
    req.set_new_stop_loss(newSL);
    req.set_new_take_profit(o.take_profit());
    try { return account.orderModify(req).order_was_modified(); }
    catch (const ApiError&) { return false; }
}
```

### 4) Re-price a pending order

```cpp
bool reprice(MT4Account& account, int ticket, double newPrice) {
    pb::OrderModifyRequest req;
    req.set_order_ticket(ticket);
    req.set_new_price(newPrice);                 // pending only
    try { return account.orderModify(req).order_was_modified(); }
    catch (const ApiError& e) { std::printf("reprice failed: %s\n", e.what()); return false; }
}
```

### 5) Extend a pending order's expiry

```cpp
#include <google/protobuf/util/time_util.h>
#include <ctime>
using google::protobuf::util::TimeUtil;

bool extendExpiry(MT4Account& account, int ticket, int extraSeconds) {
    pb::OrderModifyRequest req;
    req.set_order_ticket(ticket);
    *req.mutable_new_expiration_utc() =
        TimeUtil::SecondsToTimestamp((int64_t)std::time(nullptr) + extraSeconds);
    try { return account.orderModify(req).order_was_modified(); }
    catch (const ApiError&) { return false; }
}
```

### 6) Guard against "invalid stops"

```cpp
bool setStopsSafe(MT4Account& account, const pb::SymbolParamsManyInfo& spec,
                  int ticket, bool buy, double market, double sl, double tp) {
    double minDist = spec.tradestopslevel() * spec.point();   // minimum distance
    if (buy) {
        if (market - sl < minDist) sl = market - minDist;
        if (tp - market < minDist) tp = market + minDist;
    } else {
        if (sl - market < minDist) sl = market + minDist;
        if (market - tp < minDist) tp = market - minDist;
    }
    pb::OrderModifyRequest req;
    req.set_order_ticket(ticket); req.set_new_stop_loss(sl); req.set_new_take_profit(tp);
    try { return account.orderModify(req).order_was_modified(); }
    catch (const ApiError& e) { std::printf("still rejected: %s\n", e.what()); return false; }
}
```

---

## 🔧 Common Patterns

### Modify every open order on a symbol

```cpp
void setTpAll(MT4Account& account, const std::string& sym, double tp) {
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        if (o.symbol() != sym) continue;
        pb::OrderModifyRequest req;
        req.set_order_ticket(o.ticket());
        req.set_new_stop_loss(o.stop_loss());    // keep SL
        req.set_new_take_profit(tp);
        try { account.orderModify(req); } catch (const ApiError&) { /* skip */ }
    }
}
```

---

## 📚 See Also

* [OrderSend](./OrderSend.md) — attach SL/TP at entry instead
* [OrderCloseDelete](./OrderCloseDelete.md) — close/delete when done
* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — read current SL/TP before modifying
* **Friendly layer:** `MT4Service::orderModify(ticket, stopLoss, takeProfit, ...)` → `bool`.
* **Sugar layer:** `MT4Sugar::modifyOrderSL()` / `modifyOrderTP()` / `modifyOrderSLTP()` — return `bool`, never throw on an ordinary rejection.
