# ENUMs Usage Reference — CppMT4

> Every enumeration the CppMT4 API uses, with exact values and where it appears. The protobuf enums are generated into the `mt4_term_api` namespace (`namespace pb = mt4_term_api;`); the friendly [`MT4Service`](../MT4Service/MT4Service.Overview.md) layer exposes `enum class` mirrors.

## How enums work here

protobuf's C++ codegen puts enum **values at namespace scope** — they are *not* `enum class`. You write the bare constant and compare it directly:

```cpp
namespace pb = mt4_term_api;

pb::OpenedOrdersRequest req;
req.set_sort_type(pb::SORT_BY_OPEN_TIME_ASC);          // just the value

pb::AccountSummaryData a = account.accountSummary();
if (a.account_trade_mode() == pb::ACCOUNT_TRADE_MODE_REAL) { /* ... */ }
```

> The friendly layer uses typed `enum class` instead: `TradeMode`, `OrderType`, `Timeframe` — e.g. `svc.bars(sym, Timeframe::H1, from, to)`.

> ⚠️ **Three order-type enums, same six values.** `OO_OP_*` when **reading** orders (`OpenedOrderType`), `OC_OP_*` when **sending** (`OrderSendOperationType`), `SUB_OP_*` in **stream** frames (`SUB_ORDER_OPERATION_TYPE`). Don't mix the prefixes.

---

## 1. `EnumAccountTradeMode` — account type

**Used by:** `accountSummary` → `AccountSummaryData.account_trade_mode()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::ACCOUNT_TRADE_MODE_DEMO` | 0 | Demo account |
| `pb::ACCOUNT_TRADE_MODE_CONTEST` | 1 | Contest account |
| `pb::ACCOUNT_TRADE_MODE_REAL` | 2 | Live account |

Friendly mirror: `metarpc::TradeMode { Demo, Contest, Real }` on `AccountInfo::tradeMode`.

```cpp
if (account.accountSummary().account_trade_mode() == pb::ACCOUNT_TRADE_MODE_REAL)
    std::puts("LIVE account — be careful");
```

---

## 2. `EnumAccountMarginMode` — margin calculation mode

**Used by:** `accountSummary` → `AccountSummaryData.account_margin_mode()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::ACCOUNT_MARGIN_MODE_RETAIL_NETTING` | 0 | Netting (parity with MT5) |
| `pb::ACCOUNT_MARGIN_MODE_EXCHANGE` | 1 | Exchange |
| `pb::ACCOUNT_MARGIN_MODE_RETAIL_HEDGING` | 2 | Hedging — typical MT4 |

> **MT4 is hedging** (several orders per symbol). The other values exist for parity with MT5.

---

## 3. `EnumOpenedOrderSortType` — open-order sort

**Used by:** `openedOrders` → `OpenedOrdersRequest.sort_type`.

| Value | Int |
|-------|-----|
| `pb::SORT_BY_OPEN_TIME_ASC` | 0 |
| `pb::SORT_BY_OPEN_TIME_DESC` | 1 |
| `pb::SORT_BY_ORDER_TICKET_ID_ASC` | 2 |
| `pb::SORT_BY_ORDER_TICKET_ID_DESC` | 3 |

---

## 4. `EnumOrderHistorySortType` — history sort

**Used by:** `ordersHistory` → `OrdersHistoryRequest.input_sort_mode`.

| Value | Int |
|-------|-----|
| `pb::HISTORY_SORT_BY_OPEN_TIME_ASC` | 0 |
| `pb::HISTORY_SORT_BY_OPEN_TIME_DESC` | 1 |
| `pb::HISTORY_SORT_BY_CLOSE_TIME_ASC` | 2 |
| `pb::HISTORY_SORT_BY_CLOSE_TIME_DESC` | 3 |
| `pb::HISTORY_SORT_BY_ORDER_TICKET_ID_ASC` | 4 |
| `pb::HISTORY_SORT_BY_ORDER_TICKET_ID_DESC` | 5 |

---

## 5. Timeframes — **two different enums**

### `ENUM_QUOTE_HISTORY_TIMEFRAME` — for `quoteHistory` (indices)

**Used by:** `quoteHistory` → `QuoteHistoryRequest.timeframe`.

| Value | Int | Period |
|-------|-----|--------|
| `pb::QH_PERIOD_M1` | 0 | 1 minute |
| `pb::QH_PERIOD_M5` | 1 | 5 minutes |
| `pb::QH_PERIOD_M15` | 2 | 15 minutes |
| `pb::QH_PERIOD_M30` | 3 | 30 minutes |
| `pb::QH_PERIOD_H1` | 4 | 1 hour |
| `pb::QH_PERIOD_H4` | 5 | 4 hours |
| `pb::QH_PERIOD_D1` | 6 | 1 day |
| `pb::QH_PERIOD_W1` | 7 | 1 week |
| `pb::QH_PERIOD_MN1` | 8 | 1 month |

> ⚠️ **Ordinal 0–8, not minute counts.** `H1` is `4`, not `60`.

### `QUOTE_HISTORY_TIMEFRAME` — for `bars` (minute counts)

**Used by:** `bars` → `QuoteHistoryBarsRequest.timeframe`. This is the scheme the friendly `Timeframe` enum mirrors.

| Value | Int | Friendly |
|-------|-----|----------|
| `pb::QHB_PERIOD_M1` | 1 | `Timeframe::M1` |
| `pb::QHB_PERIOD_M5` | 5 | `Timeframe::M5` |
| `pb::QHB_PERIOD_M15` | 15 | `Timeframe::M15` |
| `pb::QHB_PERIOD_M30` | 30 | `Timeframe::M30` |
| `pb::QHB_PERIOD_H1` | 60 | `Timeframe::H1` |
| `pb::QHB_PERIOD_H4` | 240 | `Timeframe::H4` |
| `pb::QHB_PERIOD_D1` | 1440 | `Timeframe::D1` |
| `pb::QHB_PERIOD_W1` | 10080 | `Timeframe::W1` |
| `pb::QHB_PERIOD_MN1` | 43200 | `Timeframe::MN1` |

---

## 6. `OrderSendOperationType` — order type (requests)

**Used by:** `orderSend` → `OrderSendRequest.operation_type`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::OC_OP_BUY` | 0 | Market buy |
| `pb::OC_OP_SELL` | 1 | Market sell |
| `pb::OC_OP_BUYLIMIT` | 2 | Buy limit |
| `pb::OC_OP_SELLLIMIT` | 3 | Sell limit |
| `pb::OC_OP_BUYSTOP` | 4 | Buy stop |
| `pb::OC_OP_SELLSTOP` | 5 | Sell stop |

> Same numbering as MQL4 `OP_BUY`..`OP_SELLSTOP`. Friendly mirror: `OrderType` on `OrderRequest::type`.

---

## 7. `OpenedOrderType` — order type (reads)

**Used by:** `openedOrders` / `ordersHistory` → `OpenedOrderInfo.order_type()` / `HistoryOrderInfo.order_type()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::OO_OP_BUY` … `pb::OO_OP_SELLSTOP` | 0–5 | market / pending (as above) |
| `pb::OO_OP_BALANCE` | 6 | Balance entry (history only — not a trade) |
| `pb::OO_OP_CREDIT` | 7 | Credit entry (history only — not a trade) |

Friendly mirror: `OrderType { Buy, Sell, BuyLimit, SellLimit, BuyStop, SellStop, Balance, Credit }` with `Order::isPending()` / `isBuy()`.

---

## 8. `OrderCloseDeleteMode` — what a close/delete did (reply)

**Used by:** `orderCloseDelete` → `OrderCloseDeleteData.mode()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::OCD_MARKET_ORDER` | 0 | A market order was closed |
| `pb::OCD_PENDING_ORDER` | 1 | A pending order was deleted |

---

## 9. `SERVER_TIMEZONE_SOURCE` — where the UTC offset came from (reply)

**Used by:** `serverTimezone` → `ServerTimezoneData.source()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::SERVER_TIMEZONE_SOURCE_UNKNOWN` | 0 | Not read yet; `utc_offset_seconds` stays unset |
| `pb::SERVER_TIMEZONE_SOURCE_LIVE` | 1 | Read live by this terminal |
| `pb::SERVER_TIMEZONE_SOURCE_LAST_VALIDATED` | 2 | Last validated offset for this broker |

---

## 10. `MT4_SUB_ENUM_EVENT_GROUP_TYPE` — streaming event group (reply)

**Used by:** `onTrade` → `OnTradeData.type()`; `onOpenedOrdersProfit` → `OnOpenedOrdersProfitData.type()`.

| Value | Int |
|-------|-----|
| `pb::OrderProfit` | 0 |
| `pb::OrderUpdate` | 1 |

---

## 11. `SUB_ORDER_OPERATION_TYPE` — order type in stream frames (reply)

**Used by:** `onTrade` / `onOpenedOrdersProfit` order rows → `.type()` (`OnTradeOrderInfo`, `OnOpenedOrdersProfitOrderInfo`).

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::SUB_OP_BUY` … `pb::SUB_OP_SELLSTOP` | 0–5 | Buy / Sell / *Limit / *Stop |

---

## 12. Symbol-parameter enums (`SymbolParamsMany`)

Three enums describe a symbol's trading rules, on `SymbolParamsManyInfo` (from `symbolParamsMany`).

### `SP_ENUM_TRADE_CALC_MODE` — margin calculation

**Used by:** `SymbolParamsManyInfo.tradecalcmode()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::SYMBOL_TRADE_MARGINE_CALC_MODE_FOREX` | 0 | Forex |
| `pb::SYMBOL_TRADE_MARGINE_CALC_MODE_FUTURES` | 1 | Futures |
| `pb::SYMBOL_TRADE_MARGINE_CALC_MODE_CFD` | 2 | CFD |

> Note the proto typo `MARGINE` (not `MARGIN`) — frozen into the contract.

### `SP_ENUM_SYMBOL_TRADE_MODE` — what trading is allowed

**Used by:** `SymbolParamsManyInfo.trademode()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::SYMBOL_TRADE_MODE_DISABLED` | 0 | Trading disabled |
| `pb::SYMBOL_TRADE_MODE_LONGONLY` | 1 | Long only |
| `pb::SYMBOL_TRADE_MODE_SHORTONLY` | 2 | Short only |
| `pb::SYMBOL_TRADE_MODE_CLOSEONLY` | 3 | Close only |
| `pb::SYMBOL_TRADE_MODE_FULL` | 4 | Full access |

### `SP_ENUM_SYMBOL_TRADE_EXECUTION` — execution mode

**Used by:** `SymbolParamsManyInfo.tradeexemode()`.

| Value | Int | Meaning |
|-------|-----|---------|
| `pb::SYMBOL_TRADE_EXECUTION_REQUEST` | 0 | Request execution |
| `pb::SYMBOL_TRADE_EXECUTION_INSTANT` | 1 | Instant execution |
| `pb::SYMBOL_TRADE_EXECUTION_MARKET` | 2 | Market execution |
| `pb::SYMBOL_TRADE_EXECUTION_EXCHANGE` | 3 | Exchange execution |

---

## Not passed as values, but you'll meet them

* **`MqlErrorCode`** — the MQL4 trade-error numbers (`0`, `130`, `132`, …) that arrive in `ApiError::mqlErrorCode`. Full table in [Return Codes](RETURN_CODES_REFERENCE.md).
* **`ErrorType`** — the gateway `Error` payload's category (`type` field), folded into `ApiError` for you.

> Other enums in the generated pool (`ChartTemplateMode`, `EA_PARAM_TYPE`, `GuiFormFieldType`, `LogType`, `TerminalType`, `ProxyTypes`, …) belong to admin / GUI / charting RPCs that the trading SDK (`MT4Account` / `MT4Service` / `MT4Sugar`) does not surface.

---

**See also:** [MT4Account — Master Overview](../MT4Account/MT4Account.Master.Overview.md) · [SymbolParamsMany](../MT4Account/3.%20Symbols_Quotes/SymbolParamsMany.md) · [Return Codes](RETURN_CODES_REFERENCE.md) · [MT4Service API](../MT4Service/MT4Service.Overview.md)
