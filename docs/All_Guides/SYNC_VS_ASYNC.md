# Synchronous Calls vs Streaming — How CppMT4 Executes

> CppMT4's execution model in one line: **unary calls block; streams run on a background thread and call you back.** There is no `async`/`await` and no `CompletionQueue` to manage — the SDK hides the gRPC machinery.

---

## 🎯 Two execution modes

| | Unary (request → reply) | Streaming (subscription) |
|---|---|---|
| **Style** | synchronous, **blocks** until the reply | callback on a **background thread** |
| **Examples** | `accountSummary`, `quote`, `orderSend`, `ordersHistory` | `onSymbolTick`, `onTrade`, `onOpenedOrdersProfit`, … |
| **Returns** | the value (or throws `ApiError` / `TransportError`) | a stream handle; delivers payloads until stopped |
| **You manage** | nothing — call it and read the result | keep the handle alive; `stop()` to cancel |
| **Good for** | account reads, placing/closing orders, history | live ticks, trade events, health monitoring |

---

## 🔁 Unary calls — synchronous and blocking

Every non-streaming method is a blocking round-trip. Transient failures are retried with back-off and a dropped terminal is re-attached automatically, so you don't poll or retry yourself.

```cpp
MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");   // blocks until ready

double balance = sugar.getBalance();                     // blocks, returns the value
int ticket     = sugar.buyMarket("EURUSD", 0.01);        // blocks, returns the ticket (0 = rejected)
```

Deadlines come from `ClientOptions` (`unaryTimeoutMs` for reads, `tradeTimeoutMs` for trades). Tune them via `account().options()` / `service().options()`.

---

## 📡 Streaming — background thread + callbacks

Each `onX()` / `stream*` opens a server stream and invokes your callback on a **dedicated background thread**. Keep the returned handle alive to keep receiving; destroy it or call `stop()` to cancel.

```cpp
auto stream = sugar.service().subscribeTicks({"EURUSD"}, [](const Tick& t) {
    // ⚠️ runs on another thread — synchronize shared state, keep this short
    std::printf("%s %.5f/%.5f\n", t.symbol.c_str(), t.bid, t.ask);
});

std::this_thread::sleep_for(std::chrono::seconds(5));
stream->stop();   // (also happens automatically when `stream` is destroyed)
```

The handle exposes `stop()`, `running()`, `count()` and `lastError()` — see [gRPC Streaming](GRPC_STREAM_MANAGEMENT.md).

---

## 💡 Rules of thumb

- **Reads and trades** → call the unary method directly; it blocks and returns (or throws).
- **Live data / monitoring** → subscribe to a stream; handle events in the callback.
- **Don't poll a quote in a tight loop** when a tick stream will push it to you.
- **Guard callback state** with a mutex/atomic — callbacks fire on a background thread.
- **One thread, many streams** is fine; the SDK runs each stream's reader independently.

> **🆚 Coming from an async SDK (Rust/Python)?** CppMT4 is **not** `async`/`await`-based. Unary calls simply block; concurrency comes from running your own threads and from the background reader threads behind streams.
