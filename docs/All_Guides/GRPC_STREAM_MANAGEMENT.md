# gRPC Streaming & Subscription Management

> How to subscribe to live data in CppMT4, consume it cleanly, and shut it down — the callback-on-a-background-thread way.

---

## The model in one line

A streaming call returns a **`std::unique_ptr<…Stream>`** and invokes your **callback on a background reader thread** for each payload. **To unsubscribe, let the handle go out of scope or call `stop()`** — the reader thread is joined for you. Keep the handle alive to keep receiving.

```cpp
pb::OnSymbolTickRequest req;
req.add_symbol_names("EURUSD");
auto stream = account.onSymbolTick(req, [](const pb::OnSymbolTickData& d) {
    if (d.has_symbol_tick())
        std::printf("%s %.5f/%.5f\n", d.symbol_tick().symbol().c_str(),
                    d.symbol_tick().bid(), d.symbol_tick().ask());
});

std::this_thread::sleep_for(std::chrono::seconds(5));
stream->stop();   // or just let `stream` go out of scope → unsubscribed
```

The handle exposes `stop()`, `running()`, `count()`, `lastError()`.

> ⚠️ **The callback runs on another thread** — synchronize shared state (mutex/atomic) and keep it short.

---

## The five streams

On **[`MT4Account`](../MT4Account/5.%20Streaming_Methods/Streaming_Methods.Overview.md)** (raw protobuf payloads → `AccountStream`):

| Method | Pushes |
|--------|--------|
| `onSymbolTick(req, cb)` | live ticks for the given symbols (bid/ask **+ last + volume**) |
| `onTrade(req, cb)` | order/trade events (new / updated / removed / new-history) |
| `onOpenedOrdersTickets(req, cb)` | the open-order ticket lists on a timer (market + pending) |
| `onOpenedOrdersProfit(req, cb)` | per-order floating P/L + account snapshot on a timer |
| `onConnectState(req, cb)` | terminal connection-state events |

On **[`MT4Service`](../MT4Service/5.%20Streaming_Methods.md)** the same five deliver **friendly structs** (times in UTC) → `ServiceStream`: `subscribeTicks`, `streamTradeUpdates`, `streamOpenedTickets`, `streamOrderProfits`, `streamConnectState`.

---

## Two ways to consume

### A) Raw (`MT4Account`, full control)

You read protobuf payloads and guard sub-fields with `has_*`:

```cpp
namespace pb = mt4_term_api;

pb::OnSymbolTickRequest req; req.add_symbol_names("EURUSD");
auto s = account.onSymbolTick(req, [](const pb::OnSymbolTickData& d) {
    if (!d.has_symbol_tick()) return;
    const auto& t = d.symbol_tick();
    // raw accessors, last + volume available on the tick stream
    std::printf("%s %.5f/%.5f last %.5f\n", t.symbol().c_str(), t.bid(), t.ask(), t.last());
});
```

### B) Friendly (`MT4Service`, simplest)

The friendly layer converts each payload to a struct and hands it to your callback:

```cpp
auto s = service.subscribeTicks({"EURUSD", "GBPUSD"}, [](const Tick& t) {
    std::printf("%s %.5f/%.5f\n", t.symbol.c_str(), t.bid, t.ask);
});
```

Also: `service.streamOrderProfits(cb, 1000)` → `OrderProfitEvent`, `service.streamOpenedTickets(cb, 1000)` → `TicketsSnapshot`.

---

## Bounded consumption

Streams run until the handle dies. Common ways to bound them:

### Stop after N payloads

```cpp
std::atomic<int> n{0};
AccountStream* raw = nullptr;
auto s = account.onSymbolTick(req, [&](const pb::OnSymbolTickData& d) {
    if (d.has_symbol_tick() && ++n >= 10 && raw) raw->stop();
});
raw = s.get();
while (s->running()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
```

### Stop after a timeout

```cpp
auto s = account.onSymbolTick(req, cb);
std::this_thread::sleep_for(std::chrono::seconds(10));
s->stop();   // unsubscribed
```

### Stop on a condition

```cpp
std::atomic<bool> hit{false};
auto s = service.subscribeTicks({"EURUSD"}, [&](const Tick& t) {
    if (t.bid >= 1.0950) hit = true;
});
while (!hit) std::this_thread::sleep_for(std::chrono::milliseconds(50));
s->stop();
```

> For "wait until price X", `MT4Sugar::waitForPrice(symbol, targetBid, timeoutMs)` already does the blocking wait for you.

---

## Filtering by enum

Stream frames carry the same enums as the rest of the API (see [ENUMs Reference](ENUMS_USAGE_REFERENCE.md)). Example — react only to closes on `onTrade`:

```cpp
auto s = account.onTrade(pb::OnTradeRequest{}, [](const pb::OnTradeData& d) {
    if (!d.has_event_data()) return;
    for (const auto& o : d.event_data().removed_orders())      // order closed/deleted
        std::printf("CLOSE #%d %s P/L %+.2f\n", o.ticket(), o.symbol().c_str(), o.order_profit());
});
```

> `OnTradeData::type()` / `OnOpenedOrdersProfitData::type()` are `MT4_SUB_ENUM_EVENT_GROUP_TYPE`; order rows carry `SUB_ORDER_OPERATION_TYPE` (`SUB_OP_BUY` … `SUB_OP_SELLSTOP`). `OnTradeOrderInfo::commission()` is the correctly-spelled one (two m's).

---

## Multiple concurrent streams

Each stream has its own reader thread — just hold several handles at once:

```cpp
auto ticks   = service.subscribeTicks({"EURUSD"}, [](const Tick& t) { /* … */ });
auto profits = service.streamOrderProfits([](const OrderProfitEvent& e) {
    double total = 0; for (const auto& o : e.orders) total += o.profit;
    std::printf("\rfloating %.2f", total); std::fflush(stdout);
}, 1000);

std::this_thread::sleep_for(std::chrono::seconds(30));
ticks->stop();
profits->stop();   // (or let both go out of scope)
```

Share a `std::atomic` / mutex-guarded structure between callbacks if they feed the same state.

---

## Reconnection

Pair streams with `onConnectState` (or `streamConnectState`) to know when the broker link drops, and re-subscribe after a reconnect:

```cpp
std::atomic<bool> linkUp{true};
auto cs = service.streamConnectState([&](const ConnectState& st) {
    linkUp = st.isConnected && st.isAlive && st.apiIsAlive;
    if (!linkUp) std::puts("link down — pause trading, re-open data streams after reconnect");
});
```

Unary calls auto-retry transient transport errors and terminal-not-found; **streams are long-lived**, so re-opening them after a drop is your logic.

---

## ✅ Do / ❌ Don't

**✅ Do**
- Keep the handle alive while you need events; `stop()` (or scope exit) to unsubscribe — it's deterministic.
- Bound streams (count/condition + `stop()`, or sleep + `stop()`).
- Synchronize callback state (mutex/atomic) and keep the callback fast.
- Prefer **one stream with many symbols** over many single-symbol streams.
- Use the friendly `MT4Service` streams unless you need raw protobuf fields.

**❌ Don't**
- Don't let the handle go out of scope while you still want events — that cancels the stream.
- Don't do heavy work inside the callback (it blocks the reader thread) — hand off to a queue.
- Don't poll `quote` in a tight loop when a tick stream exists.
- Don't assume "no payloads" means "broken" — a quiet market simply sends nothing (check `running()` / `lastError()`).

---

**See also:** [MT4Account · Streaming Methods](../MT4Account/5.%20Streaming_Methods/Streaming_Methods.Overview.md) · [MT4Service · Streaming](../MT4Service/5.%20Streaming_Methods.md) · [Sync vs Streaming](SYNC_VS_ASYNC.md)
