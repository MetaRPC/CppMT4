# 🔄 Closing an Order by an Opposite One

> **Request:** close an order **against an opposite order** on the **same symbol** (`OrderCloseByData`), on **MT4**. Netting two hedged trades this way avoids paying the spread on a separate close.

**API Information:**

* **Low-level API:** `MT4Account::orderCloseBy(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradingHelper`
* **Proto definition:** `OrderCloseBy` (defined in `mt4-term-api-trading-helper.proto`)

### RPC

* **Service:** `mt4_term_api.TradingHelper`
* **Method:** `OrderCloseBy(OrderCloseByRequest) → OrderCloseByReply`
* **Low-level client (generated):** `TradingHelper::Stub::OrderCloseBy(context, request, response)`

---

## 🛑 No retcode — rejection throws

> On acceptance you get `OrderCloseByData{ close_price, profit, close_time }`. On rejection (tickets not opposite, different symbols, one already closed…), the call **throws `ApiError`** with the MQL4 code. Wrap it in `try/catch (const ApiError&)`.

---

## 💬 Just the essentials

* **What it is.** One RPC that closes one order using an **opposite-direction** order on the **same symbol** as the counter-order.
* **Why you need it.** Net a hedge (one BUY + one SELL) in a single operation — often with less cost than two independent closes.
* **Sanity check.** A returned `OrderCloseByData` with a sane `close_price()` and the realized `profit()`.

---

## 🎯 Purpose

* Unwind a hedged pair (a BUY and a SELL on the same symbol) against each other.
* Realize the netted result in one call instead of closing both legs separately.

> **Requirements:** both tickets must be **open market orders** on the **same symbol** and in **opposite directions**.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Close one order against an opposite order on the same symbol ("close by").
    // Returns OrderCloseByData{close_price, profit, close_time}; THROWS ApiError on rejection.
    pb::OrderCloseByData orderCloseBy(const pb::OrderCloseByRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OrderCloseByRequest {
  int32 ticket_to_close            = 1;   // the order being closed
  int32 opposite_ticket_closing_by = 2;   // the opposite order used to close it
}
```

**Reply message:**

```protobuf
OrderCloseByReply {
  oneof response {
    OrderCloseByData data  = 1;
    Error            error = 2;   // → thrown as ApiError
  }
}
```

---

## 🔽 Input

| Parameter | Type                          | Description                                  |
| --------- | ----------------------------- | -------------------------------------------- |
| `req`     | `const pb::OrderCloseByRequest&` | The two tickets: the one to close + the opposite one. |

```cpp
pb::OrderCloseByRequest req;
req.set_ticket_to_close(buyTicket);
req.set_opposite_ticket_closing_by(sellTicket);
pb::OrderCloseByData r = account.orderCloseBy(req);
```

**There is no `ctx` parameter.** Trades use `ClientOptions::tradeTimeoutMs`.

---

## ⬆️ Output — `OrderCloseByData`

| Field (proto)    | C++ accessor      | C++ type                      | Description                      |
| ---------------- | ----------------- | ----------------------------- | -------------------------------- |
| `close_price`    | `close_price()`   | `double`                      | Price at which the close-by executed. |
| `profit`         | `profit()`        | `double`                      | Realized profit of the operation. |
| `close_time`     | `close_time()`    | `google::protobuf::Timestamp` | Close time, broker clock.        |
| `close_time_utc` | `close_time_utc()`| `google::protobuf::Timestamp` | Close time, UTC.                 |

---

## 🧩 Notes & Tips

* **Opposite + same symbol + market orders** — otherwise the broker rejects with an `ApiError`.
* **Volumes may differ** — if the two legs aren't equal, the larger order's remainder stays open (broker-dependent).
* **Not for pendings** — close-by applies to live market orders only; delete a pending with [OrderCloseDelete](./OrderCloseDelete.md).
* **Catch `ApiError`** and read `e.mqlErrorCode`.

---

## 🔗 Usage Examples

### 1) Basic close-by

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

bool closeBy(MT4Account& account, int ticket, int oppositeTicket) {
    pb::OrderCloseByRequest req;
    req.set_ticket_to_close(ticket);
    req.set_opposite_ticket_closing_by(oppositeTicket);
    try {
        pb::OrderCloseByData r = account.orderCloseBy(req);
        std::printf("closed-by @ %.5f  profit %.2f\n", r.close_price(), r.profit());
        return true;
    } catch (const ApiError& e) {
        std::printf("close-by failed: %s (%d)\n", e.what(), e.mqlErrorCode);
        return false;
    }
}
```

### 2) Find a hedge pair and net it

```cpp
// Close the first BUY against the first SELL on the same symbol.
bool netHedge(MT4Account& account, const std::string& sym) {
    int buy = 0, sell = 0;
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        if (o.symbol() != sym) continue;
        if (o.order_type() == pb::OO_OP_BUY  && !buy)  buy  = o.ticket();
        if (o.order_type() == pb::OO_OP_SELL && !sell) sell = o.ticket();
    }
    if (!buy || !sell) { std::puts("no opposite pair"); return false; }

    pb::OrderCloseByRequest req;
    req.set_ticket_to_close(buy);
    req.set_opposite_ticket_closing_by(sell);
    try { account.orderCloseBy(req); return true; }
    catch (const ApiError& e) { std::printf("net failed: %s\n", e.what()); return false; }
}
```

### 3) Net all opposite pairs on a symbol

```cpp
#include <vector>
int netAll(MT4Account& account, const std::string& sym) {
    std::vector<int> buys, sells;
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        if (o.symbol() != sym) continue;
        if (o.order_type() == pb::OO_OP_BUY)  buys.push_back(o.ticket());
        if (o.order_type() == pb::OO_OP_SELL) sells.push_back(o.ticket());
    }
    int done = 0;
    for (size_t i = 0; i < buys.size() && i < sells.size(); ++i) {
        pb::OrderCloseByRequest req;
        req.set_ticket_to_close(buys[i]);
        req.set_opposite_ticket_closing_by(sells[i]);
        try { account.orderCloseBy(req); ++done; } catch (const ApiError&) {}
    }
    return done;
}
```

### 4) Record the netted profit

```cpp
double closeByProfit(MT4Account& account, int t, int opp) {
    pb::OrderCloseByRequest req;
    req.set_ticket_to_close(t);
    req.set_opposite_ticket_closing_by(opp);
    try { return account.orderCloseBy(req).profit(); }
    catch (const ApiError&) { return 0.0; }
}
```

### 5) Error classification

```cpp
bool closeBySafe(MT4Account& account, int t, int opp) {
    try {
        account.orderCloseBy({});   // (illustrative) normally fill the request
        return true;
    } catch (const ApiError& e) {
        std::printf("close-by rejected: %s (MQL4 %d)\n", e.what(), e.mqlErrorCode);
    } catch (const TransportError& e) {
        std::printf("transport error: %s\n", e.what());
    }
    return false;
}
```

---

## 🔧 Common Patterns

### Prefer close-by when a hedge exists, else plain close

```cpp
void unwind(MT4Account& account, const pb::OpenedOrderInfo& o) {
    // look for an opposite order on the same symbol
    for (const auto& x : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        bool opposite = (o.order_type() == pb::OO_OP_BUY  && x.order_type() == pb::OO_OP_SELL) ||
                        (o.order_type() == pb::OO_OP_SELL && x.order_type() == pb::OO_OP_BUY);
        if (x.symbol() == o.symbol() && opposite) {
            pb::OrderCloseByRequest req;
            req.set_ticket_to_close(o.ticket());
            req.set_opposite_ticket_closing_by(x.ticket());
            try { account.orderCloseBy(req); return; } catch (const ApiError&) { break; }
        }
    }
    // fall back to a normal close
    pb::OrderCloseDeleteRequest req; req.set_order_ticket(o.ticket()); req.set_slippage(10);
    try { account.orderCloseDelete(req); } catch (const ApiError&) {}
}
```

---

## 📚 See Also

* [OrderCloseDelete](./OrderCloseDelete.md) — the plain close/delete
* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — find the opposite ticket
* [OrdersHistory](../2.%20Orders_History/OrdersHistory.md) — the netted result lands here
* **Friendly layer:** `MT4Service::orderCloseBy(ticket, oppositeTicket)` → `CloseByResult{ closePrice, profit, closeTimeUtc }`.
* **Sugar layer:** hedging strategies call the Service `orderCloseBy` directly.
