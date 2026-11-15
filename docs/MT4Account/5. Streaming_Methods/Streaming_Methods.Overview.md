# MT4Account · Streaming Methods — Overview

> Real-time **server streams**: live ticks, trade events, periodic floating-P/L snapshots, periodic ticket snapshots, and terminal connection-state changes. Each `onX()` opens a gRPC server stream and pumps raw protobuf payloads to **your callback on a dedicated background thread**.

Four streams live on `mt4_term_api.SubscriptionService`; `onConnectState` lives on `mt4_term_api.Connection`.

## 📁 What lives here

* **[OnSymbolTick](./OnSymbolTick.md)** — live **ticks** for one or more symbols (bid/ask **plus `last` and `volume`** — unlike the unary `quote`).
* **[OnTrade](./OnTrade.md)** — **trade events**: orders opening, changing, closing, or landing in history, each with an account snapshot.
* **[OnOpenedOrdersProfit](./OnOpenedOrdersProfit.md)** — periodic **floating-P/L snapshots** of the open orders (you set the timer).
* **[OnOpenedOrdersTickets](./OnOpenedOrdersTickets.md)** — periodic **ticket snapshots**, split into market vs pending (lightweight).
* **[OnConnectState](./OnConnectState.md)** — terminal **connection-state** changes (connected / alive / api-alive).

---

## 🔌 How streaming works — the `AccountStream` handle

Every `onX()` returns a `std::unique_ptr<AccountStream>`. The SDK reads the stream on a background thread and calls your callback for each message.

```cpp
class AccountStream {
public:
    void stop();                    // cancel the stream and join the reader thread (idempotent)
    bool running() const;           // false once the stream ended or was cancelled
    uint64_t count() const;         // payloads delivered to the callback so far
    std::string lastError() const;  // the error that ended the stream, or "" if clean
};
```

* **Keep the handle alive** to keep receiving — destroying it (or calling `stop()`) cancels the subscription.
* **The callback runs on another thread** — guard shared state (mutex / atomics); don't block it for long.
* **`lastError()`** is `""` on a clean end, otherwise the reason the stream stopped.

```cpp
pb::OnSymbolTickRequest req;
req.add_symbol_names("EURUSD");
auto stream = account.onSymbolTick(req, [](const pb::OnSymbolTickData& d) {
    if (d.has_symbol_tick())
        std::printf("%s %.5f/%.5f\n", d.symbol_tick().symbol().c_str(),
                    d.symbol_tick().bid(), d.symbol_tick().ask());
});
std::this_thread::sleep_for(std::chrono::seconds(5));
stream->stop();   // (also happens automatically when `stream` is destroyed)
```

---

## 🧭 Plain English

* **Live prices** → `onSymbolTick`.
* **"Tell me when an order changes"** → `onTrade`.
* **"Update my floating P/L every N ms"** → `onOpenedOrdersProfit`.
* **"Tell me which tickets exist, cheaply"** → `onOpenedOrdersTickets`.
* **"Is the terminal still connected?"** → `onConnectState`.

> **🆚 MT4 vs MT5:** the tick stream (`OnSymbolMqlTickInfo`) **does** carry `last` and `volume` (the unary `quote` does not). Trade/profit events talk **orders**, not positions, and the streamed order type uses the **`SUB_OP_*`** enum. Note `OnTradeOrderInfo::commission()` is spelled **correctly** here (two m's) — unlike the read-side `OpenedOrderInfo::commision()`.

---

## Quick choose

| If you want…                             | Use                     | Request knob                      | Payload                       |
| ---------------------------------------- | ----------------------- | --------------------------------- | ----------------------------- |
| Live ticks (bid/ask/last/volume)         | `onSymbolTick`          | `symbol_names`                    | `OnSymbolTickData`            |
| Order lifecycle events                   | `onTrade`               | — (empty request)                 | `OnTradeData`                 |
| Floating-P/L on a timer                  | `onOpenedOrdersProfit`  | `timer_period_milliseconds`       | `OnOpenedOrdersProfitData`    |
| Ticket snapshots on a timer              | `onOpenedOrdersTickets` | `pull_interval_milliseconds`      | `OnOpenedOrdersTicketsData`   |
| Connection-state changes                 | `onConnectState`        | `terminal_ids`                    | `ConnectStateData`            |

---

## 🧱 Enums used in Streaming

> Namespace-scope values: `pb::SUB_OP_BUY`, `pb::OrderUpdate`, etc.

### `SUB_ORDER_OPERATION_TYPE` — streamed order type

```protobuf
enum SUB_ORDER_OPERATION_TYPE {
  SUB_OP_BUY      = 0;  SUB_OP_SELL      = 1;   // market
  SUB_OP_BUYLIMIT = 2;  SUB_OP_SELLLIMIT = 3;   // pending limit
  SUB_OP_BUYSTOP  = 4;  SUB_OP_SELLSTOP  = 5;   // pending stop
}
```

> A **third** order-type enum: `SUB_OP_*` on streams, vs `OO_OP_*` when reading ([OpenedOrders](../2.%20Orders_History/OpenedOrders.md)) and `OC_OP_*` when sending ([OrderSend](../4.%20Trading_Operations/OrderSend.md)). Same six values, three prefixes.

### `MT4_SUB_ENUM_EVENT_GROUP_TYPE` — event group (on `OnTradeData` / `OnOpenedOrdersProfitData`)

```protobuf
enum MT4_SUB_ENUM_EVENT_GROUP_TYPE {
  OrderProfit = 0;   // a profit-update group
  OrderUpdate = 1;   // an order-change group
}
```

---

## ❌ Cross-refs & gotchas

* **Thread safety.** Callbacks fire on a background thread — never touch UI/shared state without synchronization, and keep the callback short.
* **Lifetime.** The subscription lives as long as the `AccountStream` handle; store it, don't let it go out of scope.
* **`has_*` guards.** Payloads use message sub-fields (`has_symbol_tick()`, `has_event_data()`, `has_account_info()`) — check before reading.
* **Timers are server-side.** `onOpenedOrdersProfit` / `onOpenedOrdersTickets` push on the interval you request, not on every change.
* **Reconnect.** A dropped terminal is re-attached for unary calls automatically; for streams, watch `running()` / `lastError()` and re-subscribe if needed.

---

## See also

* **Poll instead of push:** [Quote](../3.%20Symbols_Quotes/Quote.md) · [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) · [OpenedOrdersTickets](../2.%20Orders_History/OpenedOrdersTickets.md).
* **Friendly layer:** `MT4Service::subscribeTicks()` / `streamTradeUpdates()` / `streamOrderProfits()` / `streamOpenedTickets()` / `streamConnectState()` return a `ServiceStream` and deliver friendly structs (`Tick`, `TradeEvent`, `OrderProfitEvent`, `TicketsSnapshot`, `ConnectState`).

---

**Back to:** [MT4Account — Master Overview](../MT4Account.Master.Overview.md)
