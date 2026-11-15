# 📉 Getting Unrealized-P/L Extrema

> **Request:** the **maximum and minimum floating P/L** each open order has reached (`TradeUnrealizedPLData`) — how far every live trade swung into profit and into loss — from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::tradeUnrealizedPL(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.TradeStats`
* **Proto definition:** `TradeUnrealizedPL` (defined in `mt4-term-api-trade-stats.proto`)

### RPC

* **Service:** `mt4_term_api.TradeStats`
* **Method:** `TradeUnrealizedPL(TradeUnrealizedPLRequest) → TradeUnrealizedPLReply`
* **Low-level client (generated):** `TradeStats::Stub::TradeUnrealizedPL(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning, per open order, the **max** and **min** floating P/L it reached while running (and when).
* **Why you need it.** Measure MAE/MFE (maximum adverse / favorable excursion) — how much heat a trade took and how much profit it gave back.
* **Sanity check.** `extrema_size()` matches your open orders; `max_unrealized_pl() ≥ min_unrealized_pl()`.

---

## 🎯 Purpose

* Analyze MAE/MFE to tune stops and targets.
* See which open trades swung hardest.
* Feed trade-quality analytics (how much of the peak profit was kept).

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Max/min floating P/L reached per open order (with timestamps).
    // Throws ApiError / TransportError on failure.
    pb::TradeUnrealizedPLData tradeUnrealizedPL(const pb::TradeUnrealizedPLRequest& req);
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

```protobuf
TradeUnrealizedPLRequest {
  optional string from     = 1;   // broker-time start, "yyyy-MM-ddTHH:mm:ss"
  optional string from_utc = 2;   // UTC start, same format
}
```

Send **one** of `from` / `from_utc`; omit both to measure over each order's full life.

**Reply message:**

```protobuf
TradeUnrealizedPLReply {
  oneof response {
    TradeUnrealizedPLData data  = 1;
    Error                 error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                               | Description                               |
| --------- | ---------------------------------- | ----------------------------------------- |
| `req`     | `const pb::TradeUnrealizedPLRequest&` | Optional `from` / `from_utc` start.       |

```cpp
pb::TradeUnrealizedPLRequest req;          // full life of each open order
pb::TradeUnrealizedPLData d = account.tradeUnrealizedPL(req);
```

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `TradeUnrealizedPLData`

| Field (proto) | C++ accessor | C++ type                                   | Description                      |
| ------------- | ------------ | ------------------------------------------ | -------------------------------- |
| `extrema`     | `extrema()`  | `RepeatedPtrField<TradeUnrealizedExtremaData>` | One entry per open order. Use `extrema_size()`. |

### `TradeUnrealizedExtremaData` (one order)

| Field (proto)             | C++ accessor                 | C++ type                      | Description                        |
| ------------------------- | ---------------------------- | ----------------------------- | ---------------------------------- |
| `ticket`                  | `ticket()`                   | `int64_t`                     | Order ticket.                      |
| `symbol`                  | `symbol()`                   | `const std::string&`          | Instrument.                        |
| `max_unrealized_pl`       | `max_unrealized_pl()`        | `double`                      | Highest floating profit reached (MFE). |
| `min_unrealized_pl`       | `min_unrealized_pl()`        | `double`                      | Lowest floating P/L reached (MAE). |
| `max_unrealized_time`     | `max_unrealized_time()`      | `google::protobuf::Timestamp` | When the max was reached (broker). |
| `max_unrealized_time_utc` | `max_unrealized_time_utc()`  | `google::protobuf::Timestamp` | When the max was reached (UTC).    |
| `min_unrealized_time`     | `min_unrealized_time()`      | `google::protobuf::Timestamp` | When the min was reached (broker). |
| `min_unrealized_time_utc` | `min_unrealized_time_utc()`  | `google::protobuf::Timestamp` | When the min was reached (UTC).    |

> **MFE** = `max_unrealized_pl()` (best it got), **MAE** = `min_unrealized_pl()` (worst it got). The current floating P/L is on [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) (`profit()`).

---

## 🧩 Notes & Tips

* **Open orders only** — this tracks extrema for *live* orders; closed trades are in [OrdersHistory](../2.%20Orders_History/OrdersHistory.md).
* **MAE/MFE** — pair with the current `profit()` to see how much of the peak is left, or how far underwater it went.
* **String dates** — `from` / `from_utc` are ISO-8601 strings (not `Timestamp`).
* **`ticket` is `int64`** here (the stats service uses 64-bit tickets), whereas live-order tickets are `int32`.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic — MFE/MAE per order

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::TradeUnrealizedPLData d = account.tradeUnrealizedPL(pb::TradeUnrealizedPLRequest{});
    for (const auto& e : d.extrema())
        std::printf("#%lld %-8s  MFE %.2f  MAE %.2f\n",
                    (long long)e.ticket(), e.symbol().c_str(),
                    e.max_unrealized_pl(), e.min_unrealized_pl());
    return 0;
}
```

### 2) Profit given back from the peak

```cpp
// pair extrema with current profit from openedOrders()
#include <map>
void givebacks(MT4Account& account) {
    std::map<int, double> peak;
    for (const auto& e : account.tradeUnrealizedPL(pb::TradeUnrealizedPLRequest{}).extrema())
        peak[(int)e.ticket()] = e.max_unrealized_pl();
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        auto it = peak.find(o.ticket());
        if (it != peak.end())
            std::printf("#%d gave back %.2f from peak\n", o.ticket(), it->second - o.profit());
    }
}
```

### 3) Worst heat taken (biggest MAE)

```cpp
void worstHeat(const pb::TradeUnrealizedPLData& d) {
    const pb::TradeUnrealizedExtremaData* worst = nullptr;
    for (const auto& e : d.extrema())
        if (!worst || e.min_unrealized_pl() < worst->min_unrealized_pl()) worst = &e;
    if (worst) std::printf("worst heat: #%lld %s MAE %.2f\n",
                           (long long)worst->ticket(), worst->symbol().c_str(), worst->min_unrealized_pl());
}
```

### 4) When each peak happened

```cpp
#include <ctime>
void peakTimes(const pb::TradeUnrealizedPLData& d) {
    for (const auto& e : d.extrema()) {
        std::time_t t = (std::time_t)e.max_unrealized_time_utc().seconds();
        char buf[20]; std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::gmtime(&t));
        std::printf("#%lld peak %.2f @ %s UTC\n", (long long)e.ticket(), e.max_unrealized_pl(), buf);
    }
}
```

### 5) Orders still underwater (MFE ≤ 0)

```cpp
void neverGreen(const pb::TradeUnrealizedPLData& d) {
    for (const auto& e : d.extrema())
        if (e.max_unrealized_pl() <= 0)
            std::printf("#%lld %s never went positive\n", (long long)e.ticket(), e.symbol().c_str());
}
```

### 6) Error handling

```cpp
bool tryUnrealized(MT4Account& account, pb::TradeUnrealizedPLData& out) {
    try { out = account.tradeUnrealizedPL(pb::TradeUnrealizedPLRequest{}); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Trade-quality score (kept fraction of peak)

```cpp
#include <map>
void keptFraction(MT4Account& account) {
    std::map<int, double> peak;
    for (const auto& e : account.tradeUnrealizedPL(pb::TradeUnrealizedPLRequest{}).extrema())
        peak[(int)e.ticket()] = e.max_unrealized_pl();
    for (const auto& o : account.openedOrders(pb::OpenedOrdersRequest{}).order_infos()) {
        auto it = peak.find(o.ticket());
        if (it != peak.end() && it->second > 0)
            std::printf("#%d kept %.0f%% of peak\n", o.ticket(), 100.0 * o.profit() / it->second);
    }
}
```

---

## 📚 See Also

* [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — the current floating `profit()` to compare against the peak
* [TradeStats](./TradeStats.md) — account-wide performance aggregates
* [AccountSummary](./AccountSummary.md) — net floating P/L = `equity − balance`
* **Friendly layer:** `MT4Service::tradeUnrealizedPL(...)` returns `std::vector<UnrealizedExtremum>` with UTC times.
