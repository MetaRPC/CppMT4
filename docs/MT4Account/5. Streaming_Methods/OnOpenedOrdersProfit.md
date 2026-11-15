# 📡 Streaming Open-Order Profit

> **Subscribe** to periodic **floating-P/L snapshots** of all open orders — the server pushes on a timer you choose (`OnOpenedOrdersProfitData`), delivered on a background thread, on **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::onOpenedOrdersProfit(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.SubscriptionService`
* **Proto definition:** `OnOpenedOrdersProfit` (defined in `mt4-term-api-subscriptions.proto`)

### RPC

* **Service:** `mt4_term_api.SubscriptionService`
* **Method:** `OnOpenedOrdersProfit(OnOpenedOrdersProfitRequest) → stream OnOpenedOrdersProfitReply`
* **Low-level client (generated):** `SubscriptionService::Stub::OnOpenedOrdersProfit(context, request)` (server-streaming)

---

## 💬 Just the essentials

* **What it is.** A server stream that pushes the current floating P/L of every open order every `timer_period_milliseconds`.
* **Why you need it.** A live P/L panel / equity ticker without polling [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) yourself.
* **Sanity check.** Your callback fires on the interval and the list has your open orders.

---

## 🎯 Purpose

* Drive a real-time P/L dashboard.
* Watch floating profit cross a threshold (e.g. close-all target or drawdown stop).
* Get the accompanying account snapshot (`equity` / `margin` …) at each tick.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Subscribe to periodic floating-P/L snapshots of the open orders.
    std::unique_ptr<AccountStream> onOpenedOrdersProfit(
        const pb::OnOpenedOrdersProfitRequest& req,
        std::function<void(const pb::OnOpenedOrdersProfitData&)> callback);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OnOpenedOrdersProfitRequest {
  int32 timer_period_milliseconds = 1;   // how often the server pushes a snapshot
}
```

**Streamed payload:**

```protobuf
OnOpenedOrdersProfitData {
  MT4_SUB_ENUM_EVENT_GROUP_TYPE type = 1;
  repeated OnOpenedOrdersProfitOrderInfo opened_orders_with_profit_updated = 2;
  OnEventAccountInfo account_info = 3;
  string terminal_instance_guid_id = 4;
}
```

---

## 🔽 Input

| Parameter  | Type                                                   | Description                                 |
| ---------- | ------------------------------------------------------ | ------------------------------------------- |
| `req`      | `const pb::OnOpenedOrdersProfitRequest&`               | Set `timer_period_milliseconds` (e.g. `1000`). |
| `callback` | `std::function<void(const pb::OnOpenedOrdersProfitData&)>` | Invoked per snapshot, **on a background thread**. |

```cpp
pb::OnOpenedOrdersProfitRequest req;
req.set_timer_period_milliseconds(1000);     // once per second
auto stream = account.onOpenedOrdersProfit(req, [](const pb::OnOpenedOrdersProfitData& d) { /* ... */ });
```

---

## ⬆️ Payload — `OnOpenedOrdersProfitData`

| Field (proto)                       | C++ accessor                            | C++ type                                        | Description                     |
| ----------------------------------- | --------------------------------------- | ----------------------------------------------- | ------------------------------- |
| `type`                              | `type()`                                | `MT4_SUB_ENUM_EVENT_GROUP_TYPE`                 | Event group.                    |
| `opened_orders_with_profit_updated` | `opened_orders_with_profit_updated()`   | `RepeatedPtrField<OnOpenedOrdersProfitOrderInfo>` | One entry per open order.     |
| `account_info`                      | `account_info()`                        | `OnEventAccountInfo`                             | Account snapshot (guard `has_account_info()`). |
| `terminal_instance_guid_id`         | `terminal_instance_guid_id()`           | `const std::string&`                            | Terminal session id.            |

### `OnOpenedOrdersProfitOrderInfo` — key fields

| Field (proto)  | C++ accessor     | C++ type                   | Description                |
| -------------- | ---------------- | -------------------------- | -------------------------- |
| `ticket`       | `ticket()`       | `int32_t`                  | Order ticket.              |
| `symbol`       | `symbol()`       | `const std::string&`       | Instrument.                |
| `type`         | `type()`         | `SUB_ORDER_OPERATION_TYPE` | Order type (`SUB_OP_*`).   |
| `lots`         | `lots()`         | `double`                   | Volume.                    |
| `open_price`   | `open_price()`   | `double`                   | Entry price.               |
| `order_profit` | `order_profit()` | `double`                   | Current floating profit.   |
| `swap`         | `swap()`         | `double`                   | Swap.                      |
| `commission`   | `commission()`   | `double`                   | Commission (two m's).      |
| `stop_loss`    | `stop_loss()`    | `double`                   | SL price.                  |
| `take_profit`  | `take_profit()`  | `double`                   | TP price.                  |
| `magic_number` | `magic_number()` | `int32_t`                  | EA/strategy tag.           |
| `open_time`    | `open_time()`    | `google::protobuf::Timestamp` | Open time (`open_time_utc` too). |

(`account_info` is the same `OnEventAccountInfo` as on [OnTrade](./OnTrade.md): `balance`/`credit`/`equity`/`margin`/`free_margin`/`profit`/`margin_level`/`login`.)

---

## 🧩 Notes & Tips

* **Server-side timer** — pushes every `timer_period_milliseconds`, not on each price change. Don't set it absurdly low.
* **Background thread + guards** — synchronize; check `has_account_info()`.
* **Net floating P/L** = `Σ order_profit()` over the list (or read `account_info().profit()`).
* **Keep the handle alive**; watch `running()` / `lastError()`.

---

## 🔗 Usage Examples

### 1) Basic — live total P/L once per second

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

    pb::OnOpenedOrdersProfitRequest req;
    req.set_timer_period_milliseconds(1000);
    auto stream = account.onOpenedOrdersProfit(req, [](const pb::OnOpenedOrdersProfitData& d) {
        double total = 0;
        for (const auto& o : d.opened_orders_with_profit_updated()) total += o.order_profit();
        std::printf("\rFloating P/L: %.2f  (%d orders)",
                    total, d.opened_orders_with_profit_updated_size());
        std::fflush(stdout);
    });

    std::this_thread::sleep_for(std::chrono::seconds(10));
    stream->stop();
    return 0;
}
```

### 2) Close-all profit target

```cpp
#include <atomic>
std::atomic<bool> hitTarget{false};
pb::OnOpenedOrdersProfitRequest req; req.set_timer_period_milliseconds(500);
auto s = account.onOpenedOrdersProfit(req, [&](const pb::OnOpenedOrdersProfitData& d) {
    double total = 0;
    for (const auto& o : d.opened_orders_with_profit_updated()) total += o.order_profit();
    if (total >= 100.0) hitTarget = true;   // +$100 → signal main thread to close all
});
```

### 3) Per-symbol floating P/L

```cpp
#include <map>
#include <mutex>
std::map<std::string,double> bySymbol; std::mutex m;
auto s = account.onOpenedOrdersProfit(req, [&](const pb::OnOpenedOrdersProfitData& d) {
    std::lock_guard<std::mutex> lk(m);
    bySymbol.clear();
    for (const auto& o : d.opened_orders_with_profit_updated())
        bySymbol[o.symbol()] += o.order_profit();
});
```

### 4) Drawdown guard from the account snapshot

```cpp
pb::OnOpenedOrdersProfitRequest req; req.set_timer_period_milliseconds(1000);
auto s = account.onOpenedOrdersProfit(req, [](const pb::OnOpenedOrdersProfitData& d) {
    if (d.has_account_info() && d.account_info().margin_level() > 0 &&
        d.account_info().margin_level() < 150)
        std::puts("⚠️ margin level below 150%");
});
```

### 5) Worst order right now

```cpp
auto s = account.onOpenedOrdersProfit(req, [](const pb::OnOpenedOrdersProfitData& d) {
    const pb::OnOpenedOrdersProfitOrderInfo* worst = nullptr;
    for (const auto& o : d.opened_orders_with_profit_updated())
        if (!worst || o.order_profit() < worst->order_profit()) worst = &o;
    if (worst) std::printf("worst: #%d %s %.2f\n", worst->ticket(), worst->symbol().c_str(), worst->order_profit());
});
```

### 6) Error / lifetime

```cpp
auto s = account.onOpenedOrdersProfit(req, cb);
std::this_thread::sleep_for(std::chrono::seconds(20));
if (!s->running()) std::printf("profit stream ended: %s\n", s->lastError().c_str());
s->stop();
```

---

## 🔧 Common Patterns

### Thread-safe snapshot for the UI thread

```cpp
#include <mutex>
struct PnLSnapshot { double total = 0; int orders = 0; };
PnLSnapshot latest; std::mutex lm;
pb::OnOpenedOrdersProfitRequest req; req.set_timer_period_milliseconds(1000);
auto s = account.onOpenedOrdersProfit(req, [&](const pb::OnOpenedOrdersProfitData& d) {
    double total = 0;
    for (const auto& o : d.opened_orders_with_profit_updated()) total += o.order_profit();
    std::lock_guard<std::mutex> lk(lm);
    latest = { total, d.opened_orders_with_profit_updated_size() };
});
```

---

## 📚 See Also

* [OnOpenedOrdersTickets](./OnOpenedOrdersTickets.md) — lighter: tickets only, no P/L
* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — the polling equivalent (full detail)
* [OnTrade](./OnTrade.md) — event-driven instead of timer-driven
* **Friendly layer:** `MT4Service::streamOrderProfits(periodMs, cb)` → `ServiceStream`, delivering a friendly `OrderProfitEvent`.
