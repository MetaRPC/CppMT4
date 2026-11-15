# MT4Account · Trading Operations — Overview

> The four write operations: **place** an order, **modify** it, **close/delete** it, and **close one order by an opposite one**. All live on `mt4_term_api.TradingHelper` and all run on the dedicated **trade** deadline (`ClientOptions::tradeTimeoutMs`).

## 📁 What lives here

* **[OrderSend](./OrderSend.md)** — place a **market** order (`OO_OP_BUY` / `OO_OP_SELL`) or a **pending** order (`*LIMIT` / `*STOP`); returns the new `ticket`, fill `volume`, `price` and `open_time`.
* **[OrderModify](./OrderModify.md)** — change an order's **SL / TP**, and for a pending order its **price / expiration**; returns a `order_was_modified` flag.
* **[OrderCloseDelete](./OrderCloseDelete.md)** — **close** a market order (full or partial) **or delete** a pending order, by ticket; the reply's `mode` tells you which it was.
* **[OrderCloseBy](./OrderCloseBy.md)** — close an order **against an opposite order** on the same symbol; returns `close_price` and `profit`.

---

## 🛑 The one rule that governs this whole group: **no retcode**

> MT4 trades have **no success/return code**. If the broker **accepts** the trade you get a `*Data` payload back. If it **rejects** the trade, the call **throws `metarpc::ApiError`** whose `mqlErrorCode` is the MQL4 error. There is no `10009`-style status field to inspect — **the exception *is* the result.**

```cpp
try {
    pb::OrderSendData r = account.orderSend(req);   // returned → accepted
    std::printf("Opened #%d @ %.5f\n", r.ticket(), r.price());
} catch (const ApiError& e) {
    std::printf("Rejected: %s (MQL4 %d)\n", e.what(), e.mqlErrorCode);
}
```

### MQL4 trade error codes (`ApiError::mqlErrorCode`)

| Code | Meaning                         | Typical cause / fix                                  |
| ---- | ------------------------------- | ---------------------------------------------------- |
| `0`  | no error                        | —                                                    |
| `130`| invalid stops                   | SL/TP too close to price / wrong side — respect `TradeStopsLevel` |
| `131`| invalid trade volume            | volume off the `VolumeMin/Max/Step` grid             |
| `132`| market closed                   | out of session (e.g. FX on the weekend) — try a 24/7 symbol |
| `133`| trade disabled                  | trading off for the symbol/account                   |
| `134`| not enough money                | insufficient free margin                             |
| `135`| price changed                   | re-read the quote and retry                          |
| `136`| off quotes                      | no price available right now                          |
| `138`| requote                         | price moved; retry with a fresh price / more slippage |
| `146`| trade context busy              | serialize your trade calls                            |

> The friendly layers soften this: `MT4Service::orderModify()` returns `bool`, and `MT4Sugar`'s `send()` / `close()` / `modify*()` catch `ApiError` and return `0` / `false` instead of throwing — so a one-liner never throws on an *ordinary* rejection.

---

## 🧭 Plain English

* **Open / place** → `orderSend`.
* **Adjust SL/TP (or a pending's price/expiry)** → `orderModify`.
* **Close a market trade, or cancel a pending** → `orderCloseDelete`.
* **Net two opposite trades against each other** → `orderCloseBy`.

> **🆚 MT4 vs MT5:** orders, **not positions**. There is no `positionsTotal`, no server-side `orderCheck` / `orderCalcMargin` / `orderCalcProfit`, and no stop-limit order type. Validate volume and stops **yourself** (via [SymbolParamsMany](../3.%20Symbols_Quotes/SymbolParamsMany.md) + [TickValueWithSize](../3.%20Symbols_Quotes/TickValueWithSize.md)) before sending.

---

## Quick choose

| If you want to…                              | Use                | Returns                 | Notes                                   |
| -------------------------------------------- | ------------------ | ----------------------- | --------------------------------------- |
| Open a market or place a pending order       | `orderSend`        | `OrderSendData`         | ticket/volume/price/open_time           |
| Change SL / TP (or a pending's price/expiry) | `orderModify`      | `OrderModifyData`       | `order_was_modified`                     |
| Close a market order / delete a pending      | `orderCloseDelete` | `OrderCloseDeleteData`  | `mode` = MARKET vs PENDING              |
| Close an order by an opposite one            | `orderCloseBy`     | `OrderCloseByData`      | `close_price`, `profit`                  |

---

## 🧱 Enums used in Trading Operations

> Namespace-scope values: write `pb::OC_OP_BUY`, `pb::OCD_MARKET_ORDER`, etc.

### `OrderSendOperationType` — what to place (request of `orderSend`)

```protobuf
enum OrderSendOperationType {
  OC_OP_BUY       = 0;  OC_OP_SELL      = 1;   // market
  OC_OP_BUYLIMIT  = 2;  OC_OP_SELLLIMIT = 3;   // pending limit
  OC_OP_BUYSTOP   = 4;  OC_OP_SELLSTOP  = 5;   // pending stop
}
```

> ⚠️ Note the **`OC_` prefix** here — distinct from the **`OO_`** prefix of `OpenedOrderType` used when *reading* orders ([OpenedOrders](../2.%20Orders_History/OpenedOrders.md)). Same six market/pending types, different enum.

### `OrderCloseDeleteMode` — what happened (reply of `orderCloseDelete`)

```protobuf
enum OrderCloseDeleteMode {
  OCD_MARKET_ORDER  = 0;   // a market order was closed
  OCD_PENDING_ORDER = 1;   // a pending order was deleted
}
```

---

## ❌ Cross-refs & gotchas

* **Catch `ApiError` around every trade call** and read `e.mqlErrorCode` — that's the only way to learn why a trade failed.
* **Separate deadline.** Trades use `ClientOptions::tradeTimeoutMs` (not `unaryTimeoutMs`); tune via `account.options()`.
* **Validate first.** Round volume to `VolumeStep`, keep stops beyond `TradeStopsLevel`, and check free margin — MT4 won't pre-check for you.
* **Pending vs market** — one `orderCloseDelete` handles both; the reply `mode` disambiguates.
* **Idempotency.** A trade call is **not** safe to blindly retry on a transport timeout — the first attempt may have executed. Re-query [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) before resending.
* **Market must be open** — `132` on the weekend for FX; test on a 24/7 symbol (e.g. `BTCUSD`).

---

## See also

* **Read side:** [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — the orders you act on here; [OrdersHistory](../2.%20Orders_History/OrdersHistory.md) — what they became.
* **Sizing inputs:** [SymbolParamsMany](../3.%20Symbols_Quotes/SymbolParamsMany.md) · [TickValueWithSize](../3.%20Symbols_Quotes/TickValueWithSize.md).
* **Friendly layer:** `MT4Service::orderSend(OrderRequest)` → `OrderResult`; `orderModify(...)` → `bool`; `orderClose(...)` → `void`; `orderCloseBy(...)` → `CloseByResult`.
* **Sugar layer:** `buyMarket` / `sellMarket` / `buyLimit` / … / `closeOrder` / `closeAllOrders` / `modifyOrderSLTP` — one-liners that never throw on an ordinary rejection.

---

**Back to:** [MT4Account — Master Overview](../MT4Account.Master.Overview.md)
