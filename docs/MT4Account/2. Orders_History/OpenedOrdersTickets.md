# 🎟️ Listing Open Order Tickets

> **Request:** just the **ticket numbers** of all live orders (`OpenedOrdersTicketsData`) from **MT4** — the lightweight counterpart of [OpenedOrders](./OpenedOrders.md).

**API Information:**

* **Low-level API:** `MT4Account::openedOrdersTickets(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.AccountHelper`
* **Proto definition:** `OpenedOrdersTickets` (defined in `mt4-term-api-account-helper.proto`)

### RPC

* **Service:** `mt4_term_api.AccountHelper`
* **Method:** `OpenedOrdersTickets(OpenedOrdersTicketsRequest) → OpenedOrdersTicketsReply`
* **Low-level client (generated):** `AccountHelper::Stub::OpenedOrdersTickets(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning only the **`int32` ticket numbers** of the currently live orders — nothing else.
* **Why you need it.** A cheap, frequent poll to detect *whether the set of open orders changed* without transferring full order detail.
* **Sanity check.** `tickets_size()` is the live order count; each value is a positive ticket id.

---

## 🎯 Purpose

Use it as a change detector / lightweight heartbeat:

* Poll every second to see if any order appeared or disappeared, then call [OpenedOrders](./OpenedOrders.md) **only when the set changes**.
* Keep a local set of known tickets and diff it against the latest snapshot.
* Build a minimal "N orders open" indicator with the smallest possible payload.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Returns just the ticket numbers of live orders (lighter than openedOrders).
    // Throws ApiError / TransportError on failure.
    pb::OpenedOrdersTicketsData openedOrdersTickets(const pb::OpenedOrdersTicketsRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

`OpenedOrdersTicketsRequest {}` *(empty message — just pass the default)*

**Reply message:**

```protobuf
OpenedOrdersTicketsReply {
  oneof response {
    OpenedOrdersTicketsData data  = 1;
    Error                   error = 2;
  }
}
```

`MT4Account` unwraps the `oneof`: it returns `data` on success and throws `ApiError` otherwise.

---

## 🔽 Input

| Parameter | Type                                    | Description                                      |
| --------- | --------------------------------------- | ------------------------------------------------ |
| `req`     | `const pb::OpenedOrdersTicketsRequest&` | Empty request message — pass `{}`.               |

**There is no `ctx` parameter.** The deadline comes from `ClientOptions::unaryTimeoutMs` (default `30000` ms); transient failures retry automatically.

---

## ⬆️ Output — `OpenedOrdersTicketsData`

| Field (proto) | C++ accessor | C++ type                     | Description                                   |
| ------------- | ------------ | ---------------------------- | --------------------------------------------- |
| `tickets`     | `tickets()`  | `RepeatedField<int32_t>`     | Ticket numbers of all live orders (market + pending). Use `tickets_size()` / range-for / `tickets(i)`. |

> Tickets are **`int32`** on MT4. The list covers *all* live orders — there is no per-type filter here; use [OpenedOrders](./OpenedOrders.md) for the type.

---

## 🧩 Notes & Tips

* **Smallest payload.** This returns only integers — ideal for a tight polling loop where `openedOrders()` would be wasteful.
* **Order is not guaranteed stable** across calls; compare as a **set**, not element-by-element, unless you sort first.
* **Empty is normal.** A flat account returns `tickets_size() == 0`.
* **Push alternative.** [OnOpenedOrdersTickets](../5.%20Streaming_Methods/OnOpenedOrdersTickets.md) streams these snapshots so you don't have to poll at all.
* **Reconnection is automatic,** like every read method.

---

## 🔗 Usage Examples

### 1) Basic usage

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::OpenedOrdersTicketsData t = account.openedOrdersTickets(pb::OpenedOrdersTicketsRequest{});
    std::printf("%d open tickets:", t.tickets_size());
    for (int i = 0; i < t.tickets_size(); ++i) std::printf(" %d", t.tickets(i));
    std::puts("");
    return 0;
}
```

### 2) To a `std::vector` / `std::set`

```cpp
#include <set>
std::set<int32_t> openTicketSet(MT4Account& account) {
    auto d = account.openedOrdersTickets(pb::OpenedOrdersTicketsRequest{});
    return std::set<int32_t>(d.tickets().begin(), d.tickets().end());
}
```

### 3) Detect changes (change-driven refresh)

```cpp
#include <set>
bool openOrdersChanged(MT4Account& account, std::set<int32_t>& known) {
    std::set<int32_t> now = openTicketSet(account);
    if (now != known) { known = now; return true; }
    return false;
}

// Usage: pull full detail only when the set actually changes
std::set<int32_t> known;
if (openOrdersChanged(account, known)) {
    pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
    /* rebuild blotter */
}
```

### 4) Diff: which tickets opened / closed

```cpp
#include <set>
#include <vector>
#include <algorithm>

void diffTickets(const std::set<int32_t>& before, const std::set<int32_t>& after) {
    std::vector<int32_t> opened, closed;
    std::set_difference(after.begin(),  after.end(),  before.begin(), before.end(), std::back_inserter(opened));
    std::set_difference(before.begin(), before.end(), after.begin(),  after.end(),  std::back_inserter(closed));
    for (int32_t t : opened) std::printf("＋ opened #%d\n", t);
    for (int32_t t : closed) std::printf("－ closed #%d\n", t);
}
```

### 5) Lightweight "is anything open?" guard

```cpp
bool hasOpenOrders(MT4Account& account) {
    return account.openedOrdersTickets(pb::OpenedOrdersTicketsRequest{}).tickets_size() > 0;
}
```

### 6) Error handling

```cpp
bool tryGetTickets(MT4Account& account, std::vector<int32_t>& out) {
    try {
        auto d = account.openedOrdersTickets(pb::OpenedOrdersTicketsRequest{});
        out.assign(d.tickets().begin(), d.tickets().end());
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

### Cheap poll, expensive pull on change

```cpp
#include <thread>
#include <chrono>
#include <set>

void watchOrders(MT4Account& account) {
    std::set<int32_t> known;
    for (;;) {
        if (openOrdersChanged(account, known)) {
            std::printf("Order set changed → %zu open\n", known.size());
            // account.openedOrders(...) here for full detail
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

---

## 📚 See Also

* [OpenedOrders](./OpenedOrders.md) — full details for the same orders
* [Streaming · OnOpenedOrdersTickets](../5.%20Streaming_Methods/OnOpenedOrdersTickets.md) — push instead of poll
* [OrdersHistory](./OrdersHistory.md) — closed orders
* **Friendly layer:** `MT4Service::openedOrdersTickets()` → `std::vector<int32_t>`.
* **Sugar layer:** `MT4Sugar::getOpenOrders()` when you want the orders rather than just tickets.
