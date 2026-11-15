# 📡 Streaming Open-Order Tickets

> **Subscribe** to periodic **ticket snapshots** of the open orders — split into **market** vs **pending** — pushed on a timer you choose (`OnOpenedOrdersTicketsData`), on a background thread, on **MT4**. The lightweight cousin of [OnOpenedOrdersProfit](./OnOpenedOrdersProfit.md).

**API Information:**

* **Low-level API:** `MT4Account::onOpenedOrdersTickets(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.SubscriptionService`
* **Proto definition:** `OnOpenedOrdersTickets` (defined in `mt4-term-api-subscriptions.proto`)

### RPC

* **Service:** `mt4_term_api.SubscriptionService`
* **Method:** `OnOpenedOrdersTickets(OnOpenedOrdersTicketsRequest) → stream OnOpenedOrdersTicketsReply`
* **Low-level client (generated):** `SubscriptionService::Stub::OnOpenedOrdersTickets(context, request)` (server-streaming)

---

## 💬 Just the essentials

* **What it is.** A server stream that pushes just the **ticket numbers** of open orders every `pull_interval_milliseconds` — market tickets and pending tickets in **separate lists**.
* **Why you need it.** The cheapest way to detect that the order set changed, pushed instead of polled.
* **Sanity check.** Your callback fires on the interval with the two ticket lists.

---

## 🎯 Purpose

* Detect opens/closes with minimal bandwidth (integers only).
* Distinguish market exposure from working (pending) orders at a glance.
* Trigger a full [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) pull only when the set changes.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Subscribe to periodic ticket snapshots (market + pending) of the open orders.
    std::unique_ptr<AccountStream> onOpenedOrdersTickets(
        const pb::OnOpenedOrdersTicketsRequest& req,
        std::function<void(const pb::OnOpenedOrdersTicketsData&)> callback);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OnOpenedOrdersTicketsRequest {
  int32 pull_interval_milliseconds = 1;   // how often the server pushes a snapshot
}
```

**Streamed payload:**

```protobuf
OnOpenedOrdersTicketsData {
  repeated int32 position_tickets      = 1;   // market-order tickets
  repeated int32 pending_order_tickets = 2;   // pending-order tickets
  google.protobuf.Timestamp server_time     = 3;
  google.protobuf.Timestamp server_time_utc = 6;
  string terminal_instance_guid_id = 5;
}
```

---

## 🔽 Input

| Parameter  | Type                                                     | Description                                   |
| ---------- | -------------------------------------------------------- | --------------------------------------------- |
| `req`      | `const pb::OnOpenedOrdersTicketsRequest&`                | Set `pull_interval_milliseconds` (e.g. `1000`). |
| `callback` | `std::function<void(const pb::OnOpenedOrdersTicketsData&)>` | Invoked per snapshot, **on a background thread**. |

```cpp
pb::OnOpenedOrdersTicketsRequest req;
req.set_pull_interval_milliseconds(1000);
auto stream = account.onOpenedOrdersTickets(req, [](const pb::OnOpenedOrdersTicketsData& d) { /* ... */ });
```

---

## ⬆️ Payload — `OnOpenedOrdersTicketsData`

| Field (proto)               | C++ accessor                 | C++ type                  | Description                        |
| --------------------------- | ---------------------------- | ------------------------- | ---------------------------------- |
| `position_tickets`          | `position_tickets()`         | `RepeatedField<int32_t>`  | Tickets of **market** orders.      |
| `pending_order_tickets`     | `pending_order_tickets()`    | `RepeatedField<int32_t>`  | Tickets of **pending** orders.     |
| `server_time`               | `server_time()`              | `google::protobuf::Timestamp` | Snapshot time, broker clock.   |
| `server_time_utc`           | `server_time_utc()`          | `google::protobuf::Timestamp` | Snapshot time, UTC.            |
| `terminal_instance_guid_id` | `terminal_instance_guid_id()`| `const std::string&`      | Terminal session id.               |

> **Two lists** — MT4's "positions" are just its market orders; this stream keeps them separate from pending orders. (It still isn't MT5-style netting.)

---

## 🧩 Notes & Tips

* **Server-side timer** — pushes every `pull_interval_milliseconds`.
* **Smallest payload** — integers only; ideal for a frequent change detector.
* **Compare as sets** — ordering is not guaranteed stable across snapshots.
* **Background thread + guards** — synchronize shared state.
* **Keep the handle alive**; watch `running()` / `lastError()`.

---

## 🔗 Usage Examples

### 1) Basic — counts once per second

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

    pb::OnOpenedOrdersTicketsRequest req;
    req.set_pull_interval_milliseconds(1000);
    auto stream = account.onOpenedOrdersTickets(req, [](const pb::OnOpenedOrdersTicketsData& d) {
        std::printf("\rmarket %d  pending %d",
                    d.position_tickets_size(), d.pending_order_tickets_size());
        std::fflush(stdout);
    });

    std::this_thread::sleep_for(std::chrono::seconds(10));
    stream->stop();
    return 0;
}
```

### 2) Change detection → pull full detail

```cpp
#include <set>
#include <mutex>
std::set<int32_t> known; std::mutex km;
pb::OnOpenedOrdersTicketsRequest req; req.set_pull_interval_milliseconds(1000);
auto s = account.onOpenedOrdersTickets(req, [&](const pb::OnOpenedOrdersTicketsData& d) {
    std::set<int32_t> now(d.position_tickets().begin(), d.position_tickets().end());
    now.insert(d.pending_order_tickets().begin(), d.pending_order_tickets().end());
    std::lock_guard<std::mutex> lk(km);
    if (now != known) { known = now; std::puts("order set changed — refresh blotter"); }
});
```

### 3) Diff opened / closed between snapshots

```cpp
#include <set>
#include <algorithm>
#include <vector>
std::set<int32_t> prev;
auto s = account.onOpenedOrdersTickets(req, [&](const pb::OnOpenedOrdersTicketsData& d) {
    std::set<int32_t> now(d.position_tickets().begin(), d.position_tickets().end());
    std::vector<int32_t> opened, closed;
    std::set_difference(now.begin(), now.end(), prev.begin(), prev.end(), std::back_inserter(opened));
    std::set_difference(prev.begin(), prev.end(), now.begin(), now.end(), std::back_inserter(closed));
    for (int32_t t : opened) std::printf("＋ #%d\n", t);
    for (int32_t t : closed) std::printf("－ #%d\n", t);
    prev = now;
});
```

### 4) Pending-only watch

```cpp
auto s = account.onOpenedOrdersTickets(req, [](const pb::OnOpenedOrdersTicketsData& d) {
    if (d.pending_order_tickets_size() == 0) return;
    std::printf("%d pending orders waiting\n", d.pending_order_tickets_size());
});
```

### 5) Snapshot time

```cpp
#include <ctime>
auto s = account.onOpenedOrdersTickets(req, [](const pb::OnOpenedOrdersTicketsData& d) {
    std::time_t t = (std::time_t)d.server_time_utc().seconds();
    char buf[20]; std::strftime(buf, sizeof buf, "%H:%M:%S", std::gmtime(&t));
    std::printf("[%s] m=%d p=%d\n", buf, d.position_tickets_size(), d.pending_order_tickets_size());
});
```

### 6) Error / lifetime

```cpp
auto s = account.onOpenedOrdersTickets(req, cb);
std::this_thread::sleep_for(std::chrono::seconds(15));
if (!s->running()) std::printf("tickets stream ended: %s\n", s->lastError().c_str());
s->stop();
```

---

## 🔧 Common Patterns

### "Is anything open?" push flag

```cpp
#include <atomic>
std::atomic<bool> anyOpen{false};
pb::OnOpenedOrdersTicketsRequest req; req.set_pull_interval_milliseconds(1000);
auto s = account.onOpenedOrdersTickets(req, [&](const pb::OnOpenedOrdersTicketsData& d) {
    anyOpen = (d.position_tickets_size() + d.pending_order_tickets_size()) > 0;
});
```

---

## 📚 See Also

* [OnOpenedOrdersProfit](./OnOpenedOrdersProfit.md) — same cadence, full P/L detail
* [OpenedOrdersTickets](../2.%20Orders_History/OpenedOrdersTickets.md) — the polling equivalent
* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — full detail when the set changes
* **Friendly layer:** `MT4Service::streamOpenedTickets(periodMs, cb)` → `ServiceStream`, delivering a friendly `TicketsSnapshot`.
