# 📋 Listing All Open Orders

> **Request:** every **live order** (`OpenedOrdersData`) from **MT4** — market *and* pending, in one list, with full per-order detail.

**API Information:**

* **Low-level API:** `MT4Account::openedOrders(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.AccountHelper`
* **Proto definition:** `OpenedOrders` (defined in `mt4-term-api-account-helper.proto`)

### RPC

* **Service:** `mt4_term_api.AccountHelper`
* **Method:** `OpenedOrders(OpenedOrdersRequest) → OpenedOrdersReply`
* **Low-level client (generated):** `AccountHelper::Stub::OpenedOrders(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning **all live orders** — both market trades (`OO_OP_BUY` / `OO_OP_SELL`) and pending orders (`OO_OP_BUYLIMIT` … `OO_OP_SELLSTOP`) — in a single repeated list.
* **Why you need it.** Draw a positions/orders blotter, sum floating P/L, find orders by symbol or magic number, feed a trailing-stop or risk manager.
* **Sanity check.** `order_infos_size()` is the order count; each entry has a non-zero `ticket()` and a `symbol()`.

---

## 🎯 Purpose

Use it whenever you need the current order book of the account:

* A live blotter / dashboard of everything that is open.
* Compute net floating P/L (sum of `profit()`), exposure per symbol, or lots by side.
* Locate the orders a strategy owns (filter by `magic_number()`), then modify or close them.
* **Remember:** MT4 has no "positions" — this *is* the complete picture of open exposure **and** working orders.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Returns all live orders (market + pending) in one call.
    // Throws ApiError / TransportError on failure.
    pb::OpenedOrdersData openedOrders(const pb::OpenedOrdersRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OpenedOrdersRequest {
  EnumOpenedOrderSortType sort_type = 1;   // order of the returned list
}
```

`sort_type` defaults to `SORT_BY_OPEN_TIME_ASC` (0). Pass the default-constructed request for the natural order.

**Reply message:**

```protobuf
OpenedOrdersReply {
  oneof response {
    OpenedOrdersData data  = 1;
    Error            error = 2;
  }
}
```

`MT4Account` unwraps the `oneof`: it returns `data` on success and throws `ApiError` otherwise.

---

## 🔽 Input

| Parameter | Type                           | Description                                              |
| --------- | ------------------------------ | ------------------------------------------------------- |
| `req`     | `const pb::OpenedOrdersRequest&` | Carries `sort_type`. Pass `{}` for `SORT_BY_OPEN_TIME_ASC`. |

```cpp
pb::OpenedOrdersRequest req;
req.set_sort_type(pb::SORT_BY_OPEN_TIME_DESC);   // newest first
pb::OpenedOrdersData oo = account.openedOrders(req);
```

**There is no `ctx` parameter.** The per-call deadline comes from `ClientOptions::unaryTimeoutMs` (default `30000` ms); transient failures are retried with back-off and a dropped terminal is re-attached automatically.

---

## ⬆️ Output — `OpenedOrdersData`

| Field (proto) | C++ accessor        | C++ type                        | Description                          |
| ------------- | ------------------- | ------------------------------- | ------------------------------------ |
| `order_infos` | `order_infos()`     | `RepeatedPtrField<OpenedOrderInfo>` | All live orders. Use `order_infos_size()` / range-for. |

### `OpenedOrderInfo` (one order)

| Field (proto)         | C++ accessor            | C++ type                      | Description                                         |
| --------------------- | ----------------------- | ----------------------------- | --------------------------------------------------- |
| `ticket`              | `ticket()`              | `int32_t`                     | Order ticket (unique id). **int32 on MT4.**         |
| `symbol`              | `symbol()`              | `const std::string&`          | Instrument, e.g. `"EURUSD"`.                        |
| `order_type`          | `order_type()`          | `OpenedOrderType`             | Buy/Sell (market) or *Limit/*Stop (pending) — see enum. |
| `lots`                | `lots()`                | `double`                      | Volume in lots.                                     |
| `open_price`          | `open_price()`          | `double`                      | Entry price (for pending: the trigger price).       |
| `stop_loss`           | `stop_loss()`           | `double`                      | SL price (`0` = none).                              |
| `take_profit`         | `take_profit()`         | `double`                      | TP price (`0` = none).                              |
| `profit`              | `profit()`              | `double`                      | Current floating P/L of this order (broker-calculated). |
| `swap`                | `swap()`                | `double`                      | Accrued swap.                                       |
| `commision` *(sic)*   | `commision()`           | `double`                      | Commission charged. **Note the single “m”.**        |
| `magic_number`        | `magic_number()`        | `int32_t`                     | EA/strategy tag set at send time.                   |
| `comment`             | `comment()`             | `const std::string&`          | Order comment.                                      |
| `open_time`           | `open_time()`           | `google::protobuf::Timestamp` | Open time, broker clock.                            |
| `open_time_utc`       | `open_time_utc()`       | `google::protobuf::Timestamp` | Open time, UTC.                                     |
| `expiration_time`     | `expiration_time()`     | `google::protobuf::Timestamp` | Pending-order expiry (broker clock); unset for market. |
| `expiration_time_utc` | `expiration_time_utc()` | `google::protobuf::Timestamp` | Expiry, UTC.                                        |
| `account_login`       | `account_login()`       | `int64_t`                     | Owning account login.                               |
| `position_index`      | `position_index()`      | `int32_t`                     | Internal ordering index.                            |
| `sort_index`          | `sort_index()`          | `int32_t`                     | Index under the requested `sort_type`.              |

> **🧮 Net floating P/L:** `Σ order.profit()` over the list equals `equity − balance` from [AccountSummary](../1.%20Account_Information/AccountSummary.md).

---

## 🧱 Related enums (from proto)

### `OpenedOrderType`

```protobuf
enum OpenedOrderType {
  OO_OP_BUY = 0; OO_OP_SELL = 1;            // market
  OO_OP_BUYLIMIT = 2; OO_OP_SELLLIMIT = 3;  // pending limit
  OO_OP_BUYSTOP = 4; OO_OP_SELLSTOP = 5;    // pending stop
  OO_OP_BALANCE = 6; OO_OP_CREDIT = 7;      // non-trade (mostly in history)
}
```

```cpp
inline bool isPending(pb::OpenedOrderType t) {
    return t >= pb::OO_OP_BUYLIMIT && t <= pb::OO_OP_SELLSTOP;
}
inline bool isBuy(pb::OpenedOrderType t) {
    return t == pb::OO_OP_BUY || t == pb::OO_OP_BUYLIMIT || t == pb::OO_OP_BUYSTOP;
}
```

### `EnumOpenedOrderSortType` (request)

```protobuf
enum EnumOpenedOrderSortType {
  SORT_BY_OPEN_TIME_ASC = 0; SORT_BY_OPEN_TIME_DESC = 1;
  SORT_BY_ORDER_TICKET_ID_ASC = 2; SORT_BY_ORDER_TICKET_ID_DESC = 3;
}
```

---

## 🧩 Notes & Tips

* **One list, two kinds.** Market and pending orders share the list; always branch on `order_type()` before treating a row as an open trade.
* **Commission spelling.** It is `commision()` on the wire — a protobuf field typo that cannot be changed without breaking the contract.
* **Empty is normal.** A flat account returns `order_infos_size() == 0`, not an error.
* **Reconnection is automatic.** Like all reads, this runs through the retry/reconnect wrapper.
* **Timestamps.** `open_time()` etc. are `google.protobuf.Timestamp`; read `.seconds()` or use `MT4Service::openedOrders()` for ready UTC fields.

---

## 🔗 Usage Examples

### 1) Basic listing

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    std::printf("Open orders: %d\n", oo.order_infos_size());
    for (const auto& o : oo.order_infos())
        std::printf("  #%d %-8s %.2f @ %.5f  P/L %.2f\n",
                    o.ticket(), o.symbol().c_str(), o.lots(),
                    o.open_price(), o.profit());
    return 0;
}
```

### 2) Split market vs pending

```cpp
void splitOrders(MT4Account& account) {
    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    int market = 0, pending = 0;
    for (const auto& o : oo.order_infos())
        (isPending(o.order_type()) ? pending : market)++;
    std::printf("Market: %d   Pending: %d\n", market, pending);
}
```

### 3) Net floating P/L and exposure per symbol

```cpp
#include <map>

void exposure(MT4Account& account) {
    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    double floating = 0;
    std::map<std::string, double> lotsBySymbol;
    for (const auto& o : oo.order_infos()) {
        if (isPending(o.order_type())) continue;     // only market exposure
        floating += o.profit() + o.swap() + o.commision();
        lotsBySymbol[o.symbol()] += isBuy(o.order_type()) ? o.lots() : -o.lots();
    }
    std::printf("Net floating (incl. swap/comm): %.2f\n", floating);
    for (auto& [sym, net] : lotsBySymbol)
        std::printf("  %-8s net %.2f lots\n", sym.c_str(), net);
}
```

### 4) Find orders owned by a strategy (magic number)

```cpp
std::vector<int32_t> ticketsForMagic(MT4Account& account, int32_t magic) {
    std::vector<int32_t> out;
    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    for (const auto& o : oo.order_infos())
        if (o.magic_number() == magic) out.push_back(o.ticket());
    return out;
}
```

### 5) Newest first, with time

```cpp
#include <ctime>

void recentFirst(MT4Account& account) {
    pb::OpenedOrdersRequest req;
    req.set_sort_type(pb::SORT_BY_OPEN_TIME_DESC);
    pb::OpenedOrdersData oo = account.openedOrders(req);

    for (const auto& o : oo.order_infos()) {
        std::time_t t = (std::time_t)o.open_time_utc().seconds();
        char buf[20];
        std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::gmtime(&t));
        std::printf("[%s UTC] #%d %s %.2f\n",
                    buf, o.ticket(), o.symbol().c_str(), o.lots());
    }
}
```

### 6) Orders at risk (no stop-loss)

```cpp
void unprotected(MT4Account& account) {
    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    for (const auto& o : oo.order_infos())
        if (!isPending(o.order_type()) && o.stop_loss() == 0.0)
            std::printf("⚠️ #%d %s has NO stop-loss\n",
                        o.ticket(), o.symbol().c_str());
}
```

### 7) Error handling

```cpp
bool tryGetOrders(MT4Account& account, pb::OpenedOrdersData& out) {
    try {
        out = account.openedOrders(pb::OpenedOrdersRequest{});
        return true;
    } catch (const ApiError& e) {
        std::printf("Server rejected: %s\n", e.what());
    } catch (const TransportError& e) {
        std::printf("Transport failed after retries: %s\n", e.what());
    }
    return false;
}
```

---

## 🔧 Common Patterns

### Count orders by symbol

```cpp
#include <map>
std::map<std::string,int> countBySymbol(MT4Account& account) {
    std::map<std::string,int> m;
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos())
        m[o.symbol()]++;
    return m;
}
```

### Poll tickets cheaply, pull details only on change

```cpp
// Prefer openedOrdersTickets() to detect changes, then openedOrders() for detail.
static std::vector<int32_t> prev;
auto t = account.openedOrdersTickets(pb::OpenedOrdersTicketsRequest{});
std::vector<int32_t> now(t.tickets().begin(), t.tickets().end());
if (now != prev) {
    prev = now;
    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    /* refresh the blotter … */
}
```

---

## 📚 See Also

* [OpenedOrdersTickets](./OpenedOrdersTickets.md) — the same set of orders, tickets only (lightweight polling)
* [OrdersHistory](./OrdersHistory.md) — the closed-order equivalent
* [Trading · OrderModify](../4.%20Trading_Operations/OrderModify.md) / [OrderCloseDelete](../4.%20Trading_Operations/OrderCloseDelete.md) — act on a listed order
* [Streaming · OnOpenedOrdersProfit](../5.%20Streaming_Methods/OnOpenedOrdersProfit.md) — pushed floating-P/L snapshots
* **Friendly layer:** `MT4Service::openedOrders()` → `std::vector<Order>` (with `isPending()` / `isBuy()`); `openedOrders(magic)` filters by magic.
* **Sugar layer:** `MT4Sugar::getOpenOrders()` / `getOrdersBySymbol()` / `getOrderByTicket()`.
