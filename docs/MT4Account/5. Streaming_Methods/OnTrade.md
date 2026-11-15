# 📡 Streaming Trade Events

> **Subscribe** to **trade events** — orders opening, changing, closing, or landing in history — each payload (`OnTradeData`) carrying the changed orders plus a fresh account snapshot, delivered on a background thread, on **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::onTrade(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.SubscriptionService`
* **Proto definition:** `OnTrade` (defined in `mt4-term-api-subscriptions.proto`)

### RPC

* **Service:** `mt4_term_api.SubscriptionService`
* **Method:** `OnTrade(OnTradeRequest) → stream OnTradeReply`
* **Low-level client (generated):** `SubscriptionService::Stub::OnTrade(context, request)` (server-streaming)

---

## 💬 Just the essentials

* **What it is.** A server stream that fires whenever the order set changes — new orders, updates, removals, and new history entries.
* **Why you need it.** React immediately to fills, closes and modifications; keep a local order book in sync without polling.
* **Sanity check.** Your callback fires with `d.has_event_data()` true on a change.

---

## 🎯 Purpose

* Keep a live view of orders in sync (new / updated / removed / new-history).
* Trigger logic on a fill or a close.
* Capture the account snapshot (`balance` / `equity` / `margin` / `profit`) that accompanies each event.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Subscribe to trade/order events. Returns a stream handle; keep it alive.
    std::unique_ptr<AccountStream> onTrade(
        const pb::OnTradeRequest& req,
        std::function<void(const pb::OnTradeData&)> callback);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

`OnTradeRequest {}` *(empty — subscribe to all trade events for the account)*

**Streamed payload:**

```protobuf
OnTradeData {
  MT4_SUB_ENUM_EVENT_GROUP_TYPE type = 1;   // OrderProfit / OrderUpdate
  OnTadeEventData   event_data   = 2;        // the changed orders (note the proto typo "OnTade")
  OnEventAccountInfo account_info = 3;        // account snapshot at the event
  string terminal_instance_guid_id = 4;
}
```

---

## 🔽 Input

| Parameter  | Type                                          | Description                                 |
| ---------- | --------------------------------------------- | ------------------------------------------- |
| `req`      | `const pb::OnTradeRequest&`                   | Empty request — pass `{}`.                  |
| `callback` | `std::function<void(const pb::OnTradeData&)>` | Invoked per event, **on a background thread**. |

```cpp
auto stream = account.onTrade(pb::OnTradeRequest{}, [](const pb::OnTradeData& d) { /* ... */ });
```

---

## ⬆️ Payload — `OnTradeData`

| Field (proto)               | C++ accessor                 | C++ type                        | Description                           |
| --------------------------- | ---------------------------- | ------------------------------- | ------------------------------------- |
| `type`                      | `type()`                     | `MT4_SUB_ENUM_EVENT_GROUP_TYPE` | `OrderProfit` (0) / `OrderUpdate` (1).|
| `event_data`                | `event_data()`              | `OnTadeEventData` *(sic)*       | The changed orders (guard `has_event_data()`). |
| `account_info`              | `account_info()`             | `OnEventAccountInfo`            | Account snapshot (guard `has_account_info()`). |
| `terminal_instance_guid_id` | `terminal_instance_guid_id()`| `const std::string&`            | Terminal session id.                  |

### `event_data` — `OnTadeEventData` *(proto typo: one “d”)*

| Field (proto)        | C++ accessor           | C++ type                                 | Description                      |
| -------------------- | ---------------------- | ---------------------------------------- | -------------------------------- |
| `new_orders`         | `new_orders()`         | `RepeatedPtrField<OnTradeOrderInfo>`     | Orders that just appeared.        |
| `updated_orders`     | `updated_orders()`     | `RepeatedPtrField<OnTradeUpdatedOrderInfo>` | Orders that changed (prev/current).|
| `removed_orders`     | `removed_orders()`     | `RepeatedPtrField<OnTradeOrderInfo>`     | Orders that were removed/closed.  |
| `new_history_orders` | `new_history_orders()` | `RepeatedPtrField<OnTradeOrderInfo>`     | Orders that landed in history.     |

### `OnTradeOrderInfo` — key fields

| Field (proto)  | C++ accessor     | C++ type                   | Description                          |
| -------------- | ---------------- | -------------------------- | ------------------------------------ |
| `ticket`       | `ticket()`       | `int32_t`                  | Order ticket.                        |
| `symbol`       | `symbol()`       | `const std::string&`       | Instrument.                          |
| `type`         | `type()`         | `SUB_ORDER_OPERATION_TYPE` | `SUB_OP_BUY` … `SUB_OP_SELLSTOP`.    |
| `lots`         | `lots()`         | `double`                   | Volume.                              |
| `open_price`   | `open_price()`   | `double`                   | Entry price.                         |
| `close_price`  | `close_price()`  | `double`                   | Exit price (for history/closed).     |
| `order_profit` | `order_profit()` | `double`                   | Order profit.                        |
| `stop_loss`    | `stop_loss()`    | `double`                   | SL price.                            |
| `take_profit`  | `take_profit()`  | `double`                   | TP price.                            |
| `swap`         | `swap()`         | `double`                   | Swap.                                |
| `commission`   | `commission()`   | `double`                   | Commission. **Correct spelling (two m's)** — unlike the read-side `commision()`. |
| `magic_number` | `magic_number()` | `int32_t`                  | EA/strategy tag.                     |
| `is_history`   | `is_history()`   | `bool`                     | `true` for a history entry.          |
| `open_time`    | `open_time()`    | `google::protobuf::Timestamp` | Open time (`open_time_utc` also).  |
| `close_time`   | `close_time()`   | `google::protobuf::Timestamp` | Close time (`close_time_utc` also).|

`OnTradeUpdatedOrderInfo` has `previous()` and `current()`, both `OnTradeOrderInfo`.

### `account_info` — `OnEventAccountInfo`

| Accessor        | Type     | Description            |
| --------------- | -------- | --------------------- |
| `balance()`     | `double` | Account balance.      |
| `credit()`      | `double` | Credit.               |
| `equity()`      | `double` | Equity.               |
| `margin()`      | `double` | Used margin.          |
| `free_margin()` | `double` | Free margin.          |
| `profit()`      | `double` | Floating profit.      |
| `margin_level()`| `double` | Margin level (%).     |
| `login()`       | `int64_t`| Account login.        |

---

## 🧱 Related enums

```protobuf
enum MT4_SUB_ENUM_EVENT_GROUP_TYPE { OrderProfit = 0; OrderUpdate = 1; }

enum SUB_ORDER_OPERATION_TYPE {
  SUB_OP_BUY = 0; SUB_OP_SELL = 1;
  SUB_OP_BUYLIMIT = 2; SUB_OP_SELLLIMIT = 3;
  SUB_OP_BUYSTOP = 4; SUB_OP_SELLSTOP = 5;
}
```

> `SUB_OP_*` is the **streaming** order-type enum — distinct from `OO_OP_*` (reading) and `OC_OP_*` (sending).

---

## 🧩 Notes & Tips

* **Background thread + guards** — synchronize shared state; check `has_event_data()` / `has_account_info()`.
* **Four buckets** — new / updated / removed / new-history; each is a repeated list (may be empty).
* **Correct commission spelling here** (`commission()`), opposite of the unary `OpenedOrderInfo::commision()`.
* **Keep the handle alive**; inspect `lastError()` / `running()`.
* **No request filter** — you receive all account trade events; filter in your callback (by symbol / magic).

---

## 🔗 Usage Examples

### 1) Basic — log order events

```cpp
#include <metarpc/mt4_account.hpp>
#include <thread>
#include <chrono>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    auto stream = account.onTrade(pb::OnTradeRequest{}, [](const pb::OnTradeData& d) {
        if (!d.has_event_data()) return;
        const auto& e = d.event_data();
        for (const auto& o : e.new_orders())
            std::printf("＋ NEW #%d %s %.2f @ %.5f\n", o.ticket(), o.symbol().c_str(), o.lots(), o.open_price());
        for (const auto& o : e.removed_orders())
            std::printf("－ CLOSED #%d profit %.2f\n", o.ticket(), o.order_profit());
    });

    std::this_thread::sleep_for(std::chrono::seconds(30));
    stream->stop();
    return 0;
}
```

### 2) Track modifications (previous → current)

```cpp
auto s = account.onTrade(pb::OnTradeRequest{}, [](const pb::OnTradeData& d) {
    if (!d.has_event_data()) return;
    for (const auto& u : d.event_data().updated_orders()) {
        const auto& p = u.previous();
        const auto& c = u.current();
        if (p.stop_loss() != c.stop_loss())
            std::printf("#%d SL %.5f → %.5f\n", c.ticket(), p.stop_loss(), c.stop_loss());
    }
});
```

### 3) React to a fill on a specific magic number

```cpp
int myMagic = 777;
auto s = account.onTrade(pb::OnTradeRequest{}, [myMagic](const pb::OnTradeData& d) {
    if (!d.has_event_data()) return;
    for (const auto& o : d.event_data().new_orders())
        if (o.magic_number() == myMagic)
            std::printf("my order filled: #%d\n", o.ticket());
});
```

### 4) Live equity from the event snapshot

```cpp
#include <atomic>
std::atomic<double> equity{0};
auto s = account.onTrade(pb::OnTradeRequest{}, [&](const pb::OnTradeData& d) {
    if (d.has_account_info()) equity = d.account_info().equity();
});
```

### 5) Collect closed-order P/L

```cpp
#include <mutex>
double realized = 0; std::mutex rm;
auto s = account.onTrade(pb::OnTradeRequest{}, [&](const pb::OnTradeData& d) {
    if (!d.has_event_data()) return;
    for (const auto& o : d.event_data().new_history_orders()) {
        std::lock_guard<std::mutex> lk(rm);
        realized += o.order_profit() + o.swap() + o.commission();
    }
});
```

### 6) Error / lifetime handling

```cpp
auto s = account.onTrade(pb::OnTradeRequest{}, cb);
std::this_thread::sleep_for(std::chrono::minutes(1));
if (!s->running()) std::printf("trade stream ended: %s\n", s->lastError().c_str());
s->stop();
```

---

## 🔧 Common Patterns

### Keep a local order book in sync

```cpp
#include <map>
#include <mutex>
std::map<int, pb::OnTradeOrderInfo> book; std::mutex bm;
auto s = account.onTrade(pb::OnTradeRequest{}, [&](const pb::OnTradeData& d) {
    if (!d.has_event_data()) return;
    std::lock_guard<std::mutex> lk(bm);
    for (const auto& o : d.event_data().new_orders())     book[o.ticket()] = o;
    for (const auto& u : d.event_data().updated_orders()) book[u.current().ticket()] = u.current();
    for (const auto& o : d.event_data().removed_orders()) book.erase(o.ticket());
});
```

---

## 📚 See Also

* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — the polling equivalent
* [OnOpenedOrdersProfit](./OnOpenedOrdersProfit.md) — periodic P/L snapshots
* [OrdersHistory](../2.%20Orders_History/OrdersHistory.md) — where `new_history_orders` end up
* **Friendly layer:** `MT4Service::streamTradeUpdates(cb)` → `ServiceStream`, delivering a friendly `TradeEvent`.
