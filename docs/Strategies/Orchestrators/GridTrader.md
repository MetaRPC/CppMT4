# Orchestrator · Grid Trader

> Lay a **symmetric grid of pending orders** around the current price — buy-limits below, sell-limits above, `levels` each side — list what went live, then clean the whole grid up. Built on [`MT4Sugar`](../../MT4Sugar/MT4Sugar.API_Overview.md) plus one drop to `service()`.

**Source:** `examples/demos/orchestrators/13_grid_trader.cpp` · **Run:** `./build/demos 13` · **API level:** HIGH (Sugar) + MID (`service()`)

!!! warning "Educational example"
    Places small live **pending** orders on the **demo** account and deletes them all before returning — nothing is left open. A fixed grid is a template, not a turnkey system.

---

## 🎯 What it does

1. Reads bid/ask to centre the grid on price.
2. For each level `i`, places a **buy-limit** `i` steps below and a **sell-limit** `i` steps above.
3. Drops to `service().openedOrders()` and counts the live pendings (filter by `isPending()`).
4. Deletes every order it placed (`closeOrder` deletes a pending by ticket).

## 🧩 The pattern

```
GridTrader{config} → read bid/ask → place grid (buyLimit/sellLimit each level)
    → list pendings (service().openedOrders filtered by isPending)
    → remove all (closeOrder per ticket)
```

## ⚙️ Config

```cpp
struct Config {
    int    levels{2};        // pending orders on each side
    double stepPips{20.0};   // distance between levels
    double volume{0.01};
};
```

---

## 🏗️ The orchestrator (annotated)

```cpp
class GridTrader {
public:
    struct Config { int levels{2}; double stepPips{20.0}; double volume{0.01}; };

    GridTrader(MT4Sugar& sugar, std::string symbol, Config cfg)
        : m_s(sugar), m_sym(std::move(symbol)), m_cfg(cfg) {
        m_pip = sugar.getSymbolInfo(m_sym).point;
        int d = sugar.getSymbolDigits(m_sym);
        if (d == 3 || d == 5) m_pip *= 10.0;
    }

    void run() {
        double bid = m_s.getBid(m_sym), ask = m_s.getAsk(m_sym);

        // place the grid: a buy-limit i steps below, a sell-limit i steps above
        std::vector<int> tickets;
        for (int i = 1; i <= m_cfg.levels; ++i) {
            double below = bid - i * m_cfg.stepPips * m_pip;   // buy-limit under price
            double above = ask + i * m_cfg.stepPips * m_pip;   // sell-limit over price
            int b = m_s.buyLimit (m_sym, m_cfg.volume, below);
            int s = m_s.sellLimit(m_sym, m_cfg.volume, above);
            if (b) tickets.push_back(b);
            if (s) tickets.push_back(s);
        }
        if (tickets.empty()) return;            // market closed / too close to price

        // count the live pendings via the mid-level facade
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        int pending = 0;
        for (const auto& o : m_s.service().openedOrders())
            if (o.isPending()) ++pending;

        // clean up: delete everything we placed
        for (int t : tickets) m_s.closeOrder(t);  // closeOrder deletes a pending by ticket
    }
private:
    MT4Sugar& m_s; std::string m_sym; double m_pip{0}; Config m_cfg;
};
```

**Key calls:** `getBid` / `getAsk` · `buyLimit` / `sellLimit` · `service().openedOrders()` + `Order::isPending()` · `closeOrder`.

> **🆚 MT4:** pending and market orders share **one** list — `service().openedOrders()` returns both, so filter with `o.isPending()`. The **same `closeOrder(ticket)`** deletes a pending or closes a market order.

---

## 🔗 Usage Examples

### 1) Run it with defaults

```cpp
MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");

GridTrader gt(sugar, "EURUSD", GridTrader::Config{});
gt.run();   // places 2 levels/side, lists, removes
```

### 2) A wider, denser grid

```cpp
GridTrader::Config cfg;
cfg.levels   = 5;       // 5 orders each side (10 total)
cfg.stepPips = 30;      // spaced 30 pips apart
cfg.volume   = 0.02;
GridTrader(sugar, "EURUSD", cfg).run();
```

### 3) Leave the grid live (don't auto-clean)

The demo removes its grid; a real strategy keeps it and reacts to fills. Just place and return:

```cpp
std::vector<int> placeGrid(MT4Sugar& sugar, const std::string& sym,
                           int levels, double stepPips, double vol) {
    double point = sugar.getSymbolInfo(sym).point;
    double pip = (sugar.getSymbolDigits(sym) % 2 == 1) ? point * 10 : point;
    double bid = sugar.getBid(sym), ask = sugar.getAsk(sym);
    std::vector<int> tickets;
    for (int i = 1; i <= levels; ++i) {
        if (int b = sugar.buyLimit (sym, vol, bid - i * stepPips * pip)) tickets.push_back(b);
        if (int s = sugar.sellLimit(sym, vol, ask + i * stepPips * pip)) tickets.push_back(s);
    }
    return tickets;   // caller decides when to closeOrder() them
}
```

### 4) One-sided grid (buy-limits only)

```cpp
for (int i = 1; i <= levels; ++i)
    sugar.buyLimit("EURUSD", 0.01, bid - i * stepPips * pip);   // accumulate on dips
```

### 5) Remove only this symbol's pendings

```cpp
int removeGrid(MT4Sugar& sugar, const std::string& sym) {
    int n = 0;
    for (const auto& o : sugar.getOrdersBySymbol(sym))
        if (o.isPending() && sugar.closeOrder(o.ticket)) ++n;
    return n;
}
```

---

## ⚠️ Pitfalls

* **Too close to price.** A limit/stop inside the broker's `getMinStopLevel` is rejected — `buyLimit`/`sellLimit` return `0`. Keep `stepPips` comfortably above the minimum.
* **One shared list.** `openedOrders()` includes any *market* orders too — always filter by `isPending()` before treating a row as a grid order.
* **Fills turn pending → market.** If a limit triggers, that ticket becomes a market order; `closeOrder` still handles it, but it's no longer "pending".
* **Clean up.** The demo deletes every ticket it placed; if you keep the grid, track the tickets yourself.

---

## 📚 Related

* [Strategies Overview](../Strategies.Master.Overview.md) · [Trailing Stop](./TrailingStop.md) · [Risk Manager](./RiskManager.md)
* Preset that dispatches to it: [Adaptive Market Mode](../Presets/AdaptiveMarketModePreset.md) (RANGING → Grid Trader)
* Sugar: [Trading](../../MT4Sugar/3.%20Trading.md) · [Order Management](../../MT4Sugar/4.%20Order_Management.md) · Service: [Orders & History](../../MT4Service/2.%20Orders_History.md)
