# MT4Account · Orders & History — Overview

> Everything about **orders** — the live ones and the closed ones. On MT4 there are **no "positions"**: `openedOrders()` returns market **and** pending orders in a single list, `openedOrdersTickets()` gives just their ticket numbers, and `ordersHistory()` pages through closed orders in a time window.

These methods are **read-only** and all live on `mt4_term_api.AccountHelper`. Each returns a typed `*Data` payload.

## 📁 What lives here

* **[OpenedOrders](./OpenedOrders.md)** — full details of **all live orders** (market orders *and* pending orders in one list): ticket, symbol, type, lots, open/SL/TP prices, swap, commission, floating profit, open time, magic number.
* **[OpenedOrdersTickets](./OpenedOrdersTickets.md)** — just the **ticket numbers** of the live orders (a lightweight poll when you only need to know *which* orders exist).
* **[OrdersHistory](./OrdersHistory.md)** — **closed orders** within a time range, with sort mode and pagination; each row adds `close_time` / `close_price` to the order fields.

---

## 🧭 Plain English

* **OpenedOrders** → the **blotter**: one call, every open and pending order with all its numbers. Tell a market order from a pending one by `order_type()`.
* **OpenedOrdersTickets** → the **cheap heartbeat**: did the set of open tickets change since last poll? Far smaller than pulling full details.
* **OrdersHistory** → the **statement**: what closed, when, and for how much — paginated so a long history doesn't arrive in one giant message.

> **🆚 MT4 vs MT5:** MT5 splits *positions* (net market exposure) from *orders* (pending/working). **MT4 has only orders.** There is no `positionsTotal` / `PositionInfo`. A single `OpenedOrderInfo` list carries both the market trades (`OO_OP_BUY` / `OO_OP_SELL`) and the pending ones (`OO_OP_BUYLIMIT` … `OO_OP_SELLSTOP`). History may additionally contain `OO_OP_BALANCE` / `OO_OP_CREDIT` pseudo-orders (deposits, credits). **Tickets are `int32`**, not `int64`.

---

## Quick choose

| If you need…                                   | Use                   | Returns                     | Key enum                      |
| ---------------------------------------------- | --------------------- | --------------------------- | ----------------------------- |
| Every live order with full details             | `openedOrders`        | `OpenedOrdersData`          | `OpenedOrderType` (output), `EnumOpenedOrderSortType` (input) |
| Just the open tickets (lightweight)            | `openedOrdersTickets` | `OpenedOrdersTicketsData`   | —                             |
| Closed orders in a window (paged)              | `ordersHistory`       | `OrdersHistoryData`         | `OpenedOrderType` (output), `EnumOrderHistorySortType` (input) |

---

## ❌ Cross-refs & gotchas

* **Market vs pending** — both live in `openedOrders()`. Classify by `order_type()`: `OO_OP_BUY` (0) / `OO_OP_SELL` (1) are *market*; `OO_OP_BUYLIMIT` (2) … `OO_OP_SELLSTOP` (5) are *pending*. The friendly `Order::isPending()` / `isBuy()` encode exactly this.
* **`commission` is misspelled on the wire** — both `OpenedOrderInfo` and `HistoryOrderInfo` expose it as `commision()` (one “m”), matching the generated accessor. The same applies to `StatsData`.
* **Per-order floating profit** — `OpenedOrderInfo::profit()` is this order's current floating P/L (broker-calculated). Net floating = sum over the list, which is also `equity − balance` from [AccountSummary](../1.%20Account_Information/AccountSummary.md).
* **History can be large** — `ordersHistory` doubles the RPC deadline internally and supports pagination (`page_number` / `items_per_page`, or `page_token` → `next_page_token`). Prefer a bounded `input_from` / `input_to` window.
* **Time fields come in pairs** — every order carries `open_time` (broker clock) and `open_time_utc`, plus `expiration_time(_utc)`, and history adds `close_time(_utc)`. The raw layer hands you both; `MT4Service` picks UTC for you.
* **Balance/Credit rows** — history may include `OO_OP_BALANCE` / `OO_OP_CREDIT` entries that are *not* trades; filter them out when computing trade stats.

---

## 🟢 Minimal snippet

```cpp
// All live orders (market + pending)
pb::OpenedOrdersData oo = account.openedOrders(pb::OpenedOrdersRequest{});
std::printf("%d live orders\n", oo.order_infos_size());
for (const auto& o : oo.order_infos()) {
    bool pending = o.order_type() >= pb::OO_OP_BUYLIMIT &&
                   o.order_type() <= pb::OO_OP_SELLSTOP;
    std::printf("  #%d %-8s %.2f lots  %s  P/L %.2f\n",
                o.ticket(), o.symbol().c_str(), o.lots(),
                pending ? "PENDING" : "MARKET", o.profit());
}
```

---

## 🧱 Enums used in Orders & History

> In C++ the generated enum **values live at namespace scope** (not `enum class`): write `pb::OO_OP_BUY`, `pb::SORT_BY_OPEN_TIME_DESC`, etc.

### `OpenedOrderType` — the order type (output, both methods)

```protobuf
enum OpenedOrderType {
  OO_OP_BUY       = 0;  // market buy
  OO_OP_SELL      = 1;  // market sell
  OO_OP_BUYLIMIT  = 2;  // pending buy limit
  OO_OP_SELLLIMIT = 3;  // pending sell limit
  OO_OP_BUYSTOP   = 4;  // pending buy stop
  OO_OP_SELLSTOP  = 5;  // pending sell stop
  OO_OP_BALANCE   = 6;  // balance operation (history only, not a trade)
  OO_OP_CREDIT    = 7;  // credit operation (history only, not a trade)
}
```

### `EnumOpenedOrderSortType` — sort of `openedOrders` (input)

```protobuf
enum EnumOpenedOrderSortType {
  SORT_BY_OPEN_TIME_ASC        = 0;
  SORT_BY_OPEN_TIME_DESC       = 1;
  SORT_BY_ORDER_TICKET_ID_ASC  = 2;
  SORT_BY_ORDER_TICKET_ID_DESC = 3;
}
```

### `EnumOrderHistorySortType` — sort of `ordersHistory` (input)

```protobuf
enum EnumOrderHistorySortType {
  HISTORY_SORT_BY_OPEN_TIME_ASC        = 0;
  HISTORY_SORT_BY_OPEN_TIME_DESC       = 1;
  HISTORY_SORT_BY_CLOSE_TIME_ASC       = 2;
  HISTORY_SORT_BY_CLOSE_TIME_DESC      = 3;
  HISTORY_SORT_BY_ORDER_TICKET_ID_ASC  = 4;
  HISTORY_SORT_BY_ORDER_TICKET_ID_DESC = 5;
}
```

**Helper to label an order type:**

```cpp
const char* orderTypeToString(pb::OpenedOrderType t) {
    switch (t) {
        case pb::OO_OP_BUY:       return "BUY";
        case pb::OO_OP_SELL:      return "SELL";
        case pb::OO_OP_BUYLIMIT:  return "BUYLIMIT";
        case pb::OO_OP_SELLLIMIT: return "SELLLIMIT";
        case pb::OO_OP_BUYSTOP:   return "BUYSTOP";
        case pb::OO_OP_SELLSTOP:  return "SELLSTOP";
        case pb::OO_OP_BALANCE:   return "BALANCE";
        case pb::OO_OP_CREDIT:    return "CREDIT";
        default:                  return "UNKNOWN";
    }
}
```

---

## See also

* **Account:** [AccountSummary](../1.%20Account_Information/AccountSummary.md) — net floating P/L = `equity − balance`
* **Trading:** [OrderSend](../4.%20Trading_Operations/OrderSend.md) · [OrderModify](../4.%20Trading_Operations/OrderModify.md) · [OrderCloseDelete](../4.%20Trading_Operations/OrderCloseDelete.md) — act on the orders you listed here
* **Streaming:** [OnOpenedOrdersTickets](../5.%20Streaming_Methods/OnOpenedOrdersTickets.md) — get ticket snapshots pushed instead of polling
* **Friendly layer:** `MT4Service::openedOrders()` returns `std::vector<Order>` (unified, with `isPending()` / `isBuy()`); `ordersHistory(fromUnix, toUnix)` returns `std::vector<HistoryOrder>`.
* **Sugar layer:** `MT4Sugar::getOpenOrders()` / `getOrderByTicket()` / `getHistoryToday()` / `getHistoryDateRange()`.

---

**Back to:** [MT4Account — Master Overview](../MT4Account.Master.Overview.md)
