# 📡 Streaming Live Ticks

> **Subscribe** to real-time **ticks** for one or more symbols. Each payload (`OnSymbolTickData`) carries bid/ask **plus `last` and `volume`** — delivered to your callback on a background thread, on **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::onSymbolTick(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.SubscriptionService`
* **Proto definition:** `OnSymbolTick` (defined in `mt4-term-api-subscriptions.proto`)

### RPC

* **Service:** `mt4_term_api.SubscriptionService`
* **Method:** `OnSymbolTick(OnSymbolTickRequest) → stream OnSymbolTickReply`
* **Low-level client (generated):** `SubscriptionService::Stub::OnSymbolTick(context, request)` (server-streaming)

---

## 💬 Just the essentials

* **What it is.** A server stream that pushes a tick whenever a subscribed symbol's price updates.
* **Why you need it.** Live tickers, spread monitors, tick-driven strategies — without polling `quote`.
* **Sanity check.** Your callback fires and `d.has_symbol_tick()` is `true`.

> **🆚 vs `quote`:** the stream's `OnSymbolMqlTickInfo` includes `last()` and `volume()` and millisecond times — the unary [Quote](../3.%20Symbols_Quotes/Quote.md) does not.

---

## 🎯 Purpose

* Drive a live price display or spread guard.
* Trigger logic on every tick (scalping, trailing).
* Replace a tight `quote` polling loop with push delivery.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Subscribe to live ticks for the given symbols. Returns a stream handle;
    // keep it alive to keep receiving, stop() / destroy to cancel.
    std::unique_ptr<AccountStream> onSymbolTick(
        const pb::OnSymbolTickRequest& req,
        std::function<void(const pb::OnSymbolTickData&)> callback);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
OnSymbolTickRequest {
  repeated string symbol_names = 1;   // symbols to subscribe to
}
```

**Streamed payload:**

```protobuf
OnSymbolTickData {
  OnSymbolMqlTickInfo symbol_tick = 1;
  string terminal_instance_guid_id = 2;
}
```

---

## 🔽 Input

| Parameter  | Type                                              | Description                                      |
| ---------- | ------------------------------------------------- | ------------------------------------------------ |
| `req`      | `const pb::OnSymbolTickRequest&`                  | `symbol_names` to subscribe to (`add_symbol_names`). |
| `callback` | `std::function<void(const pb::OnSymbolTickData&)>` | Invoked per tick, **on a background thread**.    |

```cpp
pb::OnSymbolTickRequest req;
req.add_symbol_names("EURUSD");
req.add_symbol_names("GBPUSD");
auto stream = account.onSymbolTick(req, [](const pb::OnSymbolTickData& d) { /* ... */ });
```

> **Select the symbols first** ([SymbolSelect](../3.%20Symbols_Quotes/SymbolSelect.md)) so the terminal is subscribed to them.

---

## ⬆️ Payload — `OnSymbolTickData` → `OnSymbolMqlTickInfo`

| Field (proto)              | C++ accessor                 | C++ type                      | Description                   |
| -------------------------- | ---------------------------- | ----------------------------- | ----------------------------- |
| `symbol_tick`              | `symbol_tick()`              | `OnSymbolMqlTickInfo`         | The tick (guard with `has_symbol_tick()`). |
| `terminal_instance_guid_id`| `terminal_instance_guid_id()`| `const std::string&`          | Terminal session id.          |

### `OnSymbolMqlTickInfo`

| Field (proto)  | C++ accessor    | C++ type                      | Description                      |
| -------------- | --------------- | ----------------------------- | -------------------------------- |
| `symbol`       | `symbol()`      | `const std::string&`          | Symbol.                          |
| `bid`          | `bid()`         | `double`                      | Bid price.                       |
| `ask`          | `ask()`         | `double`                      | Ask price.                       |
| `last`         | `last()`        | `double`                      | Last trade price.                |
| `volume`       | `volume()`      | `uint64_t`                    | Tick volume.                     |
| `time`         | `time()`        | `google::protobuf::Timestamp` | Tick time, broker clock.         |
| `time_utc`     | `time_utc()`    | `google::protobuf::Timestamp` | Tick time, UTC.                  |
| `time_msc`     | `time_msc()`    | `int64_t`                     | Tick time, broker (milliseconds).|
| `time_msc_utc` | `time_msc_utc()`| `int64_t`                     | Tick time, UTC (milliseconds).   |

---

## 🧩 Notes & Tips

* **Background thread** — synchronize any shared state; keep the callback fast.
* **Keep the handle alive** — store the `unique_ptr`; losing it cancels the stream.
* **Guard sub-fields** — check `has_symbol_tick()` before reading.
* **Several symbols, one stream** — add them all to `symbol_names`.
* **`lastError()`** tells you why a stream ended unexpectedly.

---

## 🔗 Usage Examples

### 1) Basic — print ticks for 5 seconds

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

    pb::OnSymbolTickRequest req;
    req.add_symbol_names("EURUSD");
    auto stream = account.onSymbolTick(req, [](const pb::OnSymbolTickData& d) {
        if (!d.has_symbol_tick()) return;
        const auto& t = d.symbol_tick();
        std::printf("%s  %.5f / %.5f  last %.5f  vol %llu\n",
                    t.symbol().c_str(), t.bid(), t.ask(), t.last(),
                    (unsigned long long)t.volume());
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    stream->stop();
    std::printf("received %llu ticks\n", (unsigned long long)stream->count());
    return 0;
}
```

### 2) Thread-safe latest-price cache

```cpp
#include <map>
#include <mutex>
struct PriceCache {
    std::mutex m;
    std::map<std::string, std::pair<double,double>> bidAsk;   // symbol → {bid, ask}
    void update(const pb::OnSymbolMqlTickInfo& t) {
        std::lock_guard<std::mutex> lk(m);
        bidAsk[t.symbol()] = { t.bid(), t.ask() };
    }
};

PriceCache cache;
pb::OnSymbolTickRequest req; req.add_symbol_names("EURUSD");
auto s = account.onSymbolTick(req, [&](const pb::OnSymbolTickData& d) {
    if (d.has_symbol_tick()) cache.update(d.symbol_tick());
});
```

### 3) Spread-blowout alert

```cpp
auto s = account.onSymbolTick(req, [point](const pb::OnSymbolTickData& d) {
    if (!d.has_symbol_tick()) return;
    const auto& t = d.symbol_tick();
    double spreadPts = (t.ask() - t.bid()) / point;
    if (spreadPts > 30) std::printf("⚠️ %s spread %.0f pts\n", t.symbol().c_str(), spreadPts);
});
```

### 4) Count ticks per symbol

```cpp
#include <map>
#include <mutex>
std::map<std::string,int> counts; std::mutex cm;
auto s = account.onSymbolTick(req, [&](const pb::OnSymbolTickData& d) {
    if (!d.has_symbol_tick()) return;
    std::lock_guard<std::mutex> lk(cm);
    counts[d.symbol_tick().symbol()]++;
});
```

### 5) Stop on a condition (first tick above a level)

```cpp
#include <atomic>
std::atomic<bool> hit{false};
AccountStream* raw = nullptr;
auto s = account.onSymbolTick(req, [&](const pb::OnSymbolTickData& d) {
    if (d.has_symbol_tick() && d.symbol_tick().bid() >= 1.10000) hit = true;
});
raw = s.get();
while (!hit) std::this_thread::sleep_for(std::chrono::milliseconds(50));
s->stop();
```

### 6) Watch for stream errors

```cpp
auto s = account.onSymbolTick(req, cb);
std::this_thread::sleep_for(std::chrono::seconds(10));
if (!s->running())
    std::printf("stream ended: %s\n", s->lastError().c_str());
```

---

## 🔧 Common Patterns

### Subscribe → work → cancel

```cpp
void withTicks(MT4Account& account, const std::vector<std::string>& syms,
               std::function<void(const pb::OnSymbolMqlTickInfo&)> onTick,
               std::chrono::seconds dur) {
    pb::OnSymbolTickRequest req;
    for (auto& s : syms) req.add_symbol_names(s);
    auto stream = account.onSymbolTick(req, [&](const pb::OnSymbolTickData& d) {
        if (d.has_symbol_tick()) onTick(d.symbol_tick());
    });
    std::this_thread::sleep_for(dur);
    stream->stop();
}
```

---

## 📚 See Also

* [Quote](../3.%20Symbols_Quotes/Quote.md) — the unary price (no last/volume)
* [SymbolSelect](../3.%20Symbols_Quotes/SymbolSelect.md) — select symbols before subscribing
* [OnTrade](./OnTrade.md) — order-event stream
* **Friendly layer:** `MT4Service::subscribeTicks(symbols, cb)` → `ServiceStream`, delivering `Tick{symbol, bid, ask, last, volume, timeUtc}`.
