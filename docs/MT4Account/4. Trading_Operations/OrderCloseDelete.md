# ❌ Closing a Market Order / Deleting a Pending

> **Request:** **close** a market order (full or partial) **or delete** a pending order, by ticket (`OrderCloseDeleteData`), on **MT4**. One method covers both; the reply's `mode` tells you which happened.

**API Information:**

* **Low-level API:** `MT4Account::orderCloseDelete(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradingHelper`
* **Proto definition:** `OrderCloseDelete` (defined in `mt4-term-api-trading-helper.proto`)

### RPC

* **Service:** `mt4_term_api.TradingHelper`
* **Method:** `OrderCloseDelete(OrderCloseDeleteRequest) → OrderCloseDeleteReply`
* **Low-level client (generated):** `TradingHelper::Stub::OrderCloseDelete(context, request, response)`

---

## 🛑 No retcode — rejection throws

> On acceptance you get `OrderCloseDeleteData{ mode, history_order_comment }`. On rejection (e.g. **`129` invalid price**, **`138` requote**, **`132` market closed**), the call **throws `ApiError`** with the MQL4 code. Wrap it in `try/catch (const ApiError&)`.

---

## 💬 Just the essentials

* **What it is.** One RPC that either **closes** a market order (optionally a *partial* close) or **deletes** a pending order.
* **Why you need it.** Exit a position, cancel a working order, or scale out part of a trade.
* **Sanity check.** `mode()` is `OCD_MARKET_ORDER` (closed) or `OCD_PENDING_ORDER` (deleted).

---

## 🎯 Purpose

* Close a market order fully, or partially via `lots`.
* Delete a pending order that hasn't triggered.
* Let the server decide which operation applies — you just pass the ticket.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Close a market order (full/partial) OR delete a pending order, by ticket.
    // Returns OrderCloseDeleteData{mode, ...}; THROWS ApiError on rejection.
    pb::OrderCloseDeleteData orderCloseDelete(const pb::OrderCloseDeleteRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OrderCloseDeleteRequest {
  int32  order_ticket = 1;
  optional double lots          = 2;   // partial close (market only); omit = full
  optional double closing_price = 3;   // reference close price (market)
  optional int32  slippage      = 4;   // max deviation (points)
}
```

For a **pending** order only `order_ticket` matters; `lots` / `closing_price` / `slippage` apply to **market** closes.

**Reply message:**

```protobuf
OrderCloseDeleteReply {
  oneof response {
    OrderCloseDeleteData data  = 1;
    Error                error = 2;   // → thrown as ApiError
  }
}
```

---

## 🔽 Input

| Parameter | Type                              | Description                                   |
| --------- | --------------------------------- | --------------------------------------------- |
| `req`     | `const pb::OrderCloseDeleteRequest&` | Ticket (+ optional lots/closing_price/slippage for market). |

```cpp
pb::OrderCloseDeleteRequest req;
req.set_order_ticket(ticket);
// full close: leave lots unset. Partial: req.set_lots(0.05);
pb::OrderCloseDeleteData r = account.orderCloseDelete(req);
```

**There is no `ctx` parameter.** Trades use `ClientOptions::tradeTimeoutMs`.

---

## ⬆️ Output — `OrderCloseDeleteData`

| Field (proto)           | C++ accessor              | C++ type                | Description                                 |
| ----------------------- | ------------------------- | ----------------------- | ------------------------------------------- |
| `mode`                  | `mode()`                  | `OrderCloseDeleteMode`  | `OCD_MARKET_ORDER` = closed, `OCD_PENDING_ORDER` = deleted. |
| `history_order_comment` | `history_order_comment()` | `const std::string&` *(optional)* | Broker comment on the resulting history entry. |

---

## 🧱 Related enum — `OrderCloseDeleteMode`

```protobuf
enum OrderCloseDeleteMode {
  OCD_MARKET_ORDER  = 0;   // a market order was closed
  OCD_PENDING_ORDER = 1;   // a pending order was deleted
}
```

---

## 🧩 Notes & Tips

* **One call, two behaviors** — the server picks close vs delete from the order's type; `mode()` reports what it did.
* **Partial close** — set `lots` < the order's volume (market orders only). The remainder stays open under the **same** or a new ticket depending on the broker.
* **Slippage / closing_price** help market closes fill in fast markets; a blown price can throw `138` (requote).
* **Deleting a pending** needs only the ticket.
* **Catch `ApiError`** and read `e.mqlErrorCode`.

---

## 🔗 Usage Examples

### 1) Full close / delete

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

bool closeOrDelete(MT4Account& account, int ticket) {
    pb::OrderCloseDeleteRequest req;
    req.set_order_ticket(ticket);
    try {
        pb::OrderCloseDeleteData r = account.orderCloseDelete(req);
        std::printf("%s #%d\n", r.mode() == pb::OCD_PENDING_ORDER ? "deleted pending" : "closed market", ticket);
        return true;
    } catch (const ApiError& e) {
        std::printf("close/delete failed: %s (%d)\n", e.what(), e.mqlErrorCode);
        return false;
    }
}
```

### 2) Partial close

```cpp
bool closePartial(MT4Account& account, int ticket, double lots) {
    pb::OrderCloseDeleteRequest req;
    req.set_order_ticket(ticket);
    req.set_lots(lots);                 // close this many lots
    req.set_slippage(10);
    try { account.orderCloseDelete(req); return true; }
    catch (const ApiError& e) { std::printf("partial close failed: %s\n", e.what()); return false; }
}
```

### 3) Close all market orders, delete all pendings

```cpp
int closeEverything(MT4Account& account) {
    int n = 0;
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        pb::OrderCloseDeleteRequest req;
        req.set_order_ticket(o.ticket());
        req.set_slippage(10);
        try { account.orderCloseDelete(req); ++n; }
        catch (const ApiError& e) { std::printf("skip #%d: %s\n", o.ticket(), e.what()); }
    }
    return n;
}
```

### 4) Close only losing market orders

```cpp
void cutLosers(MT4Account& account) {
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        bool market = (o.order_type() == pb::OO_OP_BUY || o.order_type() == pb::OO_OP_SELL);
        if (!market || o.profit() >= 0) continue;
        pb::OrderCloseDeleteRequest req; req.set_order_ticket(o.ticket()); req.set_slippage(10);
        try { account.orderCloseDelete(req); } catch (const ApiError&) {}
    }
}
```

### 5) Delete all pending orders only

```cpp
int deletePendings(MT4Account& account) {
    int n = 0;
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        bool pending = o.order_type() >= pb::OO_OP_BUYLIMIT && o.order_type() <= pb::OO_OP_SELLSTOP;
        if (!pending) continue;
        pb::OrderCloseDeleteRequest req; req.set_order_ticket(o.ticket());
        try { if (account.orderCloseDelete(req).mode() == pb::OCD_PENDING_ORDER) ++n; }
        catch (const ApiError&) {}
    }
    return n;
}
```

### 6) Close with the current price as reference

```cpp
bool closeAtMarket(MT4Account& account, const pb::OpenedOrderInfo& o) {
    pb::QuoteRequest q; q.set_symbol(o.symbol());
    pb::QuoteData px = account.quote(q);
    double ref = (o.order_type() == pb::OO_OP_BUY) ? px.bid() : px.ask();  // close buy@bid, sell@ask

    pb::OrderCloseDeleteRequest req;
    req.set_order_ticket(o.ticket());
    req.set_closing_price(ref);
    req.set_slippage(15);
    try { account.orderCloseDelete(req); return true; }
    catch (const ApiError& e) { std::printf("close failed: %s\n", e.what()); return false; }
}
```

---

## 🔧 Common Patterns

### Close all orders on one symbol

```cpp
int closeBySymbol(MT4Account& account, const std::string& sym) {
    int n = 0;
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        if (o.symbol() != sym) continue;
        pb::OrderCloseDeleteRequest req; req.set_order_ticket(o.ticket()); req.set_slippage(10);
        try { account.orderCloseDelete(req); ++n; } catch (const ApiError&) {}
    }
    return n;
}
```

---

## 📚 See Also

* [OrderCloseBy](./OrderCloseBy.md) — close an order against an opposite one (no spread paid twice)
* [OrderSend](./OrderSend.md) / [OrderModify](./OrderModify.md) — open / adjust
* [OrdersHistory](../2.%20Orders_History/OrdersHistory.md) — the closed order shows up here
* **Friendly layer:** `MT4Service::orderClose(ticket, lots = 0, price = 0, slippage = 10)` → `void` (throws on failure).
* **Sugar layer:** `MT4Sugar::closeOrder()` / `closeOrderPartial()` / `closeAllOrders()` / `closeAllBySymbol()` — return `bool`/count, never throw on an ordinary rejection.
