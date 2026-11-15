# 📜 Fetching Closed-Order History

> **Request:** **closed orders** (`OrdersHistoryData`) from **MT4** within a time window — sorted and paginated. Each row is a `HistoryOrderInfo` (an order plus its `close_time` / `close_price`).

**API Information:**

* **Low-level API:** `MT4Account::ordersHistory(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.AccountHelper`
* **Proto definition:** `OrdersHistory` (defined in `mt4-term-api-account-helper.proto`)

### RPC

* **Service:** `mt4_term_api.AccountHelper`
* **Method:** `OrdersHistory(OrdersHistoryRequest) → OrdersHistoryReply`
* **Low-level client (generated):** `AccountHelper::Stub::OrdersHistory(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning the account's **closed orders** in a time range, with a chosen sort order and pagination.
* **Why you need it.** Trade journals, P/L reports, reconciliation, computing realized profit, feeding your own statistics.
* **Sanity check.** `total_count()` is how many orders match; `orders_info_size()` is how many came back in *this* page.

---

## 🎯 Purpose

Use it to read what already closed:

* Build a statement / trade log for a day, week, month or custom range.
* Sum realized P/L (`profit + swap + commission`) over closed trades.
* Reconcile against your own records by ticket.
* Page through a long history without pulling it all at once.

> **⚠️ Filter out non-trades.** History can include `OO_OP_BALANCE` / `OO_OP_CREDIT` rows (deposits/credits) — skip them for trade statistics.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Returns closed orders within a time window (sorted, paginated).
    // Uses a doubled RPC deadline internally (histories can be large).
    // Throws ApiError / TransportError on failure.
    pb::OrdersHistoryData ordersHistory(const pb::OrdersHistoryRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OrdersHistoryRequest {
  EnumOrderHistorySortType    input_sort_mode = 1;   // sort of the result
  optional google.protobuf.Timestamp input_from     = 2;   // window start, broker clock
  optional google.protobuf.Timestamp input_to       = 3;   // window end,   broker clock
  optional int32 page_number    = 4;                 // 0-based page index
  optional int32 items_per_page = 5;                 // page size
  string page_token = 6;                             // opaque cursor (alt. to page_number)
  int32  page_size  = 7;                             // page size (with page_token)
  optional google.protobuf.Timestamp input_from_utc = 8;   // window start, UTC
  optional google.protobuf.Timestamp input_to_utc   = 9;   // window end,   UTC
}
```

Set **either** the broker-clock window (`input_from` / `input_to`) **or** the UTC window (`input_from_utc` / `input_to_utc`) — not both. Paginate with **either** `page_number` + `items_per_page` **or** `page_token` + `page_size`.

**Reply message:**

```protobuf
OrdersHistoryReply {
  oneof response {
    OrdersHistoryData data  = 1;
    Error             error = 2;
  }
}
```

`MT4Account` unwraps the `oneof`: it returns `data` on success and throws `ApiError` otherwise.

---

## 🔽 Input

| Parameter | Type                            | Description                                             |
| --------- | ------------------------------- | ------------------------------------------------------ |
| `req`     | `const pb::OrdersHistoryRequest&` | The window, sort mode and paging. See fields above.    |

```cpp
#include <google/protobuf/util/time_util.h>
using google::protobuf::util::TimeUtil;

pb::OrdersHistoryRequest req;
req.set_input_sort_mode(pb::HISTORY_SORT_BY_CLOSE_TIME_DESC);     // newest closes first
*req.mutable_input_from_utc() = TimeUtil::SecondsToTimestamp(fromUnix);
*req.mutable_input_to_utc()   = TimeUtil::SecondsToTimestamp(toUnix);
req.set_items_per_page(100);
req.set_page_number(0);

pb::OrdersHistoryData h = account.ordersHistory(req);
```

**There is no `ctx` parameter.** This call uses **twice** the normal `unaryTimeoutMs` because histories can be large; transient failures retry with back-off.

---

## ⬆️ Output — `OrdersHistoryData`

| Field (proto)     | C++ accessor        | C++ type                             | Description                                     |
| ----------------- | ------------------- | ------------------------------------ | ----------------------------------------------- |
| `orders_info`     | `orders_info()`     | `RepeatedPtrField<HistoryOrderInfo>` | Closed orders on this page. Use `orders_info_size()`. |
| `total_count`     | `total_count()`     | `int32_t`                            | Total matching orders across all pages.         |
| `next_page_token` | `next_page_token()` | `const std::string&`                 | Cursor for the next page (empty when last page).|

### `HistoryOrderInfo` (one closed order)

Same fields as [`OpenedOrderInfo`](./OpenedOrders.md) **plus** the close data:

| Field (proto)    | C++ accessor      | C++ type                      | Description                                  |
| ---------------- | ----------------- | ----------------------------- | -------------------------------------------- |
| `ticket`         | `ticket()`        | `int32_t`                     | Order ticket.                                |
| `symbol`         | `symbol()`        | `const std::string&`          | Instrument.                                  |
| `order_type`     | `order_type()`    | `OpenedOrderType`             | Buy/Sell/pending — or `BALANCE`/`CREDIT`.    |
| `lots`           | `lots()`          | `double`                      | Volume in lots.                              |
| `open_price`     | `open_price()`    | `double`                      | Entry price.                                 |
| `close_price`    | `close_price()`   | `double`                      | Exit price.                                  |
| `stop_loss`      | `stop_loss()`     | `double`                      | SL at close (`0` = none).                    |
| `take_profit`    | `take_profit()`   | `double`                      | TP at close (`0` = none).                    |
| `profit`         | `profit()`        | `double`                      | Realized profit of the trade.                |
| `swap`           | `swap()`          | `double`                      | Swap charged.                                |
| `commision` *(sic)* | `commision()`  | `double`                      | Commission. **Single “m”.**                  |
| `magic_number`   | `magic_number()`  | `int32_t`                     | EA/strategy tag.                             |
| `comment`        | `comment()`       | `const std::string&`          | Order comment.                               |
| `open_time`      | `open_time()`     | `google::protobuf::Timestamp` | Open time, broker clock.                     |
| `open_time_utc`  | `open_time_utc()` | `google::protobuf::Timestamp` | Open time, UTC.                              |
| `close_time`     | `close_time()`    | `google::protobuf::Timestamp` | Close time, broker clock.                    |
| `close_time_utc` | `close_time_utc()`| `google::protobuf::Timestamp` | Close time, UTC.                            |
| `account_login`  | `account_login()` | `int64_t`                     | Owning account login.                        |

> **🧮 Realized P/L of a trade:** `profit() + swap() + commision()`.

---

## 🧱 Related enums (from proto)

### `EnumOrderHistorySortType` (request)

```protobuf
enum EnumOrderHistorySortType {
  HISTORY_SORT_BY_OPEN_TIME_ASC        = 0;
  HISTORY_SORT_BY_OPEN_TIME_DESC       = 1;
  HISTORY_SORT_BY_CLOSE_TIME_ASC       = 2;
  HISTORY_SORT_BY_CLOSE_TIME_DESC      = 3;
  HISTORY_SORT_BY_ORDER_TICKET_ID_ASC  = 4;
  HISTORY_SORT_BY_ORDER_TICKET_ID_DESC = 5;
}
```

### `OpenedOrderType` (output)

Same enum as for live orders (`OO_OP_BUY` … `OO_OP_SELLSTOP`, plus `OO_OP_BALANCE` / `OO_OP_CREDIT` which appear **only** in history). See [OpenedOrders → Related enums](./OpenedOrders.md).

---

## 🧩 Notes & Tips

* **Always bound the window.** Prefer an explicit `input_from(_utc)` / `input_to(_utc)`; an unbounded history can be huge.
* **Pick one time base.** Use the UTC pair *or* the broker-clock pair, not both.
* **Pick one paging scheme.** `page_number` + `items_per_page`, *or* `page_token` + `page_size`. Loop until `next_page_token()` is empty (or you've read `total_count()` rows).
* **Skip non-trades.** `OO_OP_BALANCE` / `OO_OP_CREDIT` are bookkeeping rows — exclude them from win-rate / profit-factor math.
* **Doubled deadline.** This method intentionally uses `2 × unaryTimeoutMs`.
* **Commission spelling** is `commision()` (one “m”), same as live orders.

---

## 🔗 Usage Examples

### 1) Basic — last 7 days, newest first

```cpp
#include <metarpc/mt4_account.hpp>
#include <google/protobuf/util/time_util.h>
#include <ctime>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;
using google::protobuf::util::TimeUtil;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    int64_t now  = (int64_t)std::time(nullptr);
    int64_t from = now - 7 * 24 * 3600;

    pb::OrdersHistoryRequest req;
    req.set_input_sort_mode(pb::HISTORY_SORT_BY_CLOSE_TIME_DESC);
    *req.mutable_input_from_utc() = TimeUtil::SecondsToTimestamp(from);
    *req.mutable_input_to_utc()   = TimeUtil::SecondsToTimestamp(now);

    pb::OrdersHistoryData h = account.ordersHistory(req);
    std::printf("Matched %d orders (showing %d)\n", h.total_count(), h.orders_info_size());
    for (const auto& o : h.orders_info())
        std::printf("  #%d %-8s %.2f  P/L %.2f\n",
                    o.ticket(), o.symbol().c_str(), o.lots(), o.profit());
    return 0;
}
```

### 2) Realized P/L over the window (trades only)

```cpp
double realizedPnL(MT4Account& account, const pb::OrdersHistoryRequest& req) {
    pb::OrdersHistoryData h = account.ordersHistory(req);
    double net = 0;
    for (const auto& o : h.orders_info()) {
        if (o.order_type() == pb::OO_OP_BALANCE || o.order_type() == pb::OO_OP_CREDIT)
            continue;                                   // skip non-trades
        net += o.profit() + o.swap() + o.commision();
    }
    return net;
}
```

### 3) Page through the whole history

```cpp
#include <vector>
std::vector<pb::HistoryOrderInfo> allHistory(MT4Account& account, int64_t from, int64_t to) {
    std::vector<pb::HistoryOrderInfo> all;
    pb::OrdersHistoryRequest req;
    req.set_input_sort_mode(pb::HISTORY_SORT_BY_CLOSE_TIME_ASC);
    *req.mutable_input_from_utc() = TimeUtil::SecondsToTimestamp(from);
    *req.mutable_input_to_utc()   = TimeUtil::SecondsToTimestamp(to);
    req.set_page_size(200);

    for (;;) {
        pb::OrdersHistoryData h = account.ordersHistory(req);
        for (const auto& o : h.orders_info()) all.push_back(o);
        if (h.next_page_token().empty()) break;
        req.set_page_token(h.next_page_token());         // advance the cursor
    }
    return all;
}
```

### 4) Win rate & profit factor

```cpp
void stats(MT4Account& account, const pb::OrdersHistoryRequest& req) {
    pb::OrdersHistoryData h = account.ordersHistory(req);
    int wins = 0, total = 0;
    double grossWin = 0, grossLoss = 0;
    for (const auto& o : h.orders_info()) {
        if (o.order_type() == pb::OO_OP_BALANCE || o.order_type() == pb::OO_OP_CREDIT) continue;
        double net = o.profit() + o.swap() + o.commision();
        ++total;
        if (net >= 0) { ++wins; grossWin += net; } else { grossLoss += -net; }
    }
    std::printf("Trades: %d  Win rate: %.1f%%  Profit factor: %.2f\n",
                total, total ? 100.0 * wins / total : 0.0,
                grossLoss ? grossWin / grossLoss : 0.0);
}
```

### 5) Group realized P/L by symbol

```cpp
#include <map>
std::map<std::string,double> pnlBySymbol(MT4Account& account, const pb::OrdersHistoryRequest& req) {
    std::map<std::string,double> m;
    for (const auto& o : account.ordersHistory(req).orders_info()) {
        if (o.order_type() == pb::OO_OP_BALANCE || o.order_type() == pb::OO_OP_CREDIT) continue;
        m[o.symbol()] += o.profit() + o.swap() + o.commision();
    }
    return m;
}
```

### 6) Print with close time

```cpp
void printClosed(const pb::HistoryOrderInfo& o) {
    std::time_t t = (std::time_t)o.close_time_utc().seconds();
    char buf[20];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::gmtime(&t));
    std::printf("[%s UTC] #%d %-8s %.2f @ %.5f → %.5f  P/L %.2f\n",
                buf, o.ticket(), o.symbol().c_str(), o.lots(),
                o.open_price(), o.close_price(), o.profit());
}
```

### 7) Error handling

```cpp
bool tryGetHistory(MT4Account& account, const pb::OrdersHistoryRequest& req, pb::OrdersHistoryData& out) {
    try {
        out = account.ordersHistory(req);
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

### "Today" window in UTC

```cpp
pb::OrdersHistoryRequest todayReq() {
    std::time_t now = std::time(nullptr);
    std::tm g = *std::gmtime(&now);
    g.tm_hour = g.tm_min = g.tm_sec = 0;
    int64_t midnight = (int64_t)timegm(&g);

    pb::OrdersHistoryRequest req;
    req.set_input_sort_mode(pb::HISTORY_SORT_BY_CLOSE_TIME_ASC);
    *req.mutable_input_from_utc() = TimeUtil::SecondsToTimestamp(midnight);
    *req.mutable_input_to_utc()   = TimeUtil::SecondsToTimestamp((int64_t)now);
    return req;
}
```

---

## 📚 See Also

* [OpenedOrders](./OpenedOrders.md) — the *live*-order equivalent (shares the field set)
* [Account · TradeStats](../1.%20Account_Information/TradeStats.md) — server-side aggregated statistics (no manual paging)
* [Account · EquityHistory](../1.%20Account_Information/EquityHistory.md) — the equity/balance curve
* **Friendly layer:** `MT4Service::ordersHistory(fromUnix, toUnix)` → `std::vector<HistoryOrder>` with UTC times.
* **Sugar layer:** `MT4Sugar::getHistoryToday()` / `getHistoryYesterday()` / `getHistoryThisWeek()` / `getHistoryThisMonth()` / `getHistoryDateRange()`.
