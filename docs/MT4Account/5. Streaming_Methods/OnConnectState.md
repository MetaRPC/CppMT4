# 📡 Streaming Connection State

> **Subscribe** to terminal **connection-state changes** — connected / alive / api-alive, with the server label and any error — pushed to your callback on a background thread (`ConnectStateData`), on **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::onConnectState(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.Connection`
* **Proto definition:** `OnConnectState` (defined in `mt4-term-api-connection.proto`)

### RPC

* **Service:** `mt4_term_api.Connection`
* **Method:** `OnConnectState(OnConnectStateRequest) → stream OnConnectStateReply`
* **Low-level client (generated):** `Connection::Stub::OnConnectState(context, request)` (server-streaming)

> Unlike the other four streams (on `SubscriptionService`), this one lives on the **`Connection`** service.

---

## 💬 Just the essentials

* **What it is.** A server stream that reports when the terminal's connection state changes.
* **Why you need it.** Health monitoring — know the moment the terminal drops or recovers, instead of discovering it when a call fails.
* **Sanity check.** Your callback fires with `is_connected()` reflecting the current state.

---

## 🎯 Purpose

* Show a live "connected / disconnected" indicator.
* Pause trading when the terminal is not alive; resume when it recovers.
* Log connection incidents with the server label and error message.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Subscribe to terminal connection-state changes (connected / disconnected / reconnecting).
    std::unique_ptr<AccountStream> onConnectState(
        const pb::OnConnectStateRequest& req,
        std::function<void(const pb::ConnectStateData&)> callback);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OnConnectStateRequest {
  repeated string terminal_ids = 1;   // terminals to watch (empty = this session's terminal)
}
```

**Streamed payload:**

```protobuf
ConnectStateData {
  string id = 1;
  string state = 2;              // textual state
  bool   is_connected = 3;
  bool   is_alive = 4;
  bool   api_is_alive = 5;
  string server = 6;
  uint64 account = 7;
  string error_message = 8;
  google.protobuf.Timestamp timestamp = 9;
}
```

---

## 🔽 Input

| Parameter  | Type                                             | Description                                      |
| ---------- | ------------------------------------------------ | ------------------------------------------------ |
| `req`      | `const pb::OnConnectStateRequest&`               | `terminal_ids` to watch; leave empty for this session's terminal. |
| `callback` | `std::function<void(const pb::ConnectStateData&)>` | Invoked on each state change, **on a background thread**. |

```cpp
pb::OnConnectStateRequest req;             // empty → this terminal
auto stream = account.onConnectState(req, [](const pb::ConnectStateData& s) { /* ... */ });
```

---

## ⬆️ Payload — `ConnectStateData`

| Field (proto)   | C++ accessor      | C++ type                      | Description                                  |
| --------------- | ----------------- | ----------------------------- | -------------------------------------------- |
| `id`            | `id()`            | `const std::string&`          | Terminal id.                                 |
| `state`         | `state()`         | `const std::string&`          | Textual state label.                         |
| `is_connected`  | `is_connected()`  | `bool`                        | Terminal is connected to the broker.         |
| `is_alive`      | `is_alive()`      | `bool`                        | Terminal process is alive.                   |
| `api_is_alive`  | `api_is_alive()`  | `bool`                        | The gateway/API link is alive.               |
| `server`        | `server()`        | `const std::string&`          | Broker server label.                         |
| `account`       | `account()`       | `uint64_t`                    | Account login.                               |
| `error_message` | `error_message()` | `const std::string&`          | Error text when something is wrong (else empty). |
| `timestamp`     | `timestamp()`     | `google::protobuf::Timestamp` | When the state was reported.                  |

---

## 🧩 Notes & Tips

* **On the `Connection` service** — not `SubscriptionService` like the other streams.
* **Three liveness flags** — `is_connected` (to the broker), `is_alive` (terminal), `api_is_alive` (gateway). All three true = healthy.
* **Background thread + guards** — synchronize shared state.
* **Keep the handle alive**; inspect `lastError()` / `running()`.
* **Complements auto-reconnect** — unary calls re-attach a dropped terminal automatically; this stream lets you *observe* the state and gate trading on it.

---

## 🔗 Usage Examples

### 1) Basic — log state changes

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

    auto stream = account.onConnectState(pb::OnConnectStateRequest{}, [](const pb::ConnectStateData& s) {
        std::printf("[%s] connected=%d alive=%d api=%d  %s\n",
                    s.server().c_str(), s.is_connected(), s.is_alive(),
                    s.api_is_alive(), s.error_message().c_str());
    });

    std::this_thread::sleep_for(std::chrono::seconds(30));
    stream->stop();
    return 0;
}
```

### 2) Health flag for the trading loop

```cpp
#include <atomic>
std::atomic<bool> healthy{true};
auto s = account.onConnectState(pb::OnConnectStateRequest{}, [&](const pb::ConnectStateData& st) {
    healthy = st.is_connected() && st.is_alive() && st.api_is_alive();
});
// elsewhere: if (!healthy) skip sending orders
```

### 3) Alert on disconnect

```cpp
auto s = account.onConnectState(pb::OnConnectStateRequest{}, [](const pb::ConnectStateData& st) {
    if (!st.is_connected())
        std::printf("⚠️ DISCONNECTED from %s: %s\n", st.server().c_str(), st.error_message().c_str());
});
```

### 4) Timestamped incident log

```cpp
#include <ctime>
auto s = account.onConnectState(pb::OnConnectStateRequest{}, [](const pb::ConnectStateData& st) {
    std::time_t t = (std::time_t)st.timestamp().seconds();
    char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
    std::printf("[%s UTC] state=%s connected=%d\n", buf, st.state().c_str(), st.is_connected());
});
```

### 5) Pause/resume a strategy on state

```cpp
#include <atomic>
std::atomic<bool> paused{false};
auto s = account.onConnectState(pb::OnConnectStateRequest{}, [&](const pb::ConnectStateData& st) {
    bool ok = st.is_connected() && st.api_is_alive();
    if (!ok && !paused) { paused = true;  std::puts("⏸ paused (link down)"); }
    if (ok  &&  paused) { paused = false; std::puts("▶ resumed (link up)"); }
});
```

### 6) Error / lifetime

```cpp
auto s = account.onConnectState(pb::OnConnectStateRequest{}, cb);
std::this_thread::sleep_for(std::chrono::minutes(1));
if (!s->running()) std::printf("connect-state stream ended: %s\n", s->lastError().c_str());
s->stop();
```

---

## 🔧 Common Patterns

### Gate every trade on live health

```cpp
#include <atomic>
std::atomic<bool> linkUp{true};
auto guard = account.onConnectState(pb::OnConnectStateRequest{}, [&](const pb::ConnectStateData& st) {
    linkUp = st.is_connected() && st.is_alive() && st.api_is_alive();
});

auto safeBuy = [&](const std::string& sym, double lots) -> int {
    if (!linkUp) { std::puts("link down — not trading"); return 0; }
    pb::OrderSendRequest req;
    req.set_symbol(sym); req.set_operation_type(pb::OC_OP_BUY); req.set_volume(lots); req.set_slippage(10);
    try { return account.orderSend(req).ticket(); } catch (const ApiError&) { return 0; }
};
```

---

## 📚 See Also

* [AccountSummary](../1.%20Account_Information/AccountSummary.md) — a one-shot connectivity sanity check
* [OnTrade](./OnTrade.md) — order-event stream (on `SubscriptionService`)
* **Low-level infra:** `MT4Account::isConnected()` / `reconnect()` — the synchronous connection controls.
* **Friendly layer:** `MT4Service::streamConnectState(cb)` → `ServiceStream`, delivering a friendly `ConnectState`.
