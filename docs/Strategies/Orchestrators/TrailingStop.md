# Orchestrator · Trailing Stop

> Open one order, then **ratchet its stop loss up** as price advances — never down. The classic *"let winners run, cap the downside"* manager, built entirely on [`MT4Sugar`](../../MT4Sugar/MT4Sugar.API_Overview.md).

**Source:** `examples/demos/orchestrators/11_trailing_stop.cpp` · **Run:** `./build/demos 11` · **API level:** HIGH (Sugar)

!!! warning "Educational example"
    Places a small live `0.01` BUY on the **demo** account and closes it before returning. The heuristic (a fixed pip trail) is a template to build on, not a turnkey system.

---

## 🎯 What it does

1. Opens a market BUY.
2. Each cycle, reads the bid and computes a stop `trailPips` behind it.
3. Raises the SL **only if** the new level is higher than the current one (monotonic ratchet).
4. Closes the order at the end of the run.

## 🧩 The pattern

```
TrailingStop{config} → buyMarket → run(cycles):
    read bid → wantSL = bid - trailPips·pip → if wantSL > bestSL: raise SL → repeat
→ closeOrder
```

## ⚙️ Config

```cpp
struct Config {
    double trailPips{20.0};   // keep the SL this many pips behind the best bid
    int    cycles{8};         // how many times to re-evaluate
    int    intervalMs{700};   // pause between cycles
};
```

---

## 🏗️ The orchestrator (annotated)

```cpp
class TrailingStop {
public:
    struct Config { double trailPips{20.0}; int cycles{8}; int intervalMs{700}; };

    TrailingStop(MT4Sugar& sugar, std::string symbol, double volume, Config cfg)
        : m_s(sugar), m_sym(std::move(symbol)), m_vol(volume), m_cfg(cfg) {
        // pip = point, scaled ×10 on 3/5-digit symbols (so "pips" means what a trader means)
        m_pip = sugar.getSymbolInfo(m_sym).point;
        int d = sugar.getSymbolDigits(m_sym);
        if (d == 3 || d == 5) m_pip *= 10.0;
    }

    void run() {
        int ticket = m_s.buyMarket(m_sym, m_vol);
        if (ticket == 0) return;                 // rejected (market closed?) — nothing to trail

        double bestSL = 0;
        for (int c = 1; c <= m_cfg.cycles; ++c) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_cfg.intervalMs));

            bool found = false;
            Order o = m_s.getOrderByTicket(ticket, found);
            if (!found) return;                  // closed elsewhere — stop managing it

            double bid    = m_s.getBid(m_sym);
            double wantSL = bid - m_cfg.trailPips * m_pip;
            if (wantSL > bestSL) {               // only ever tighten
                bestSL = wantSL;
                m_s.modifyOrderSL(ticket, bestSL);
            }
        }
        m_s.closeOrder(ticket);
    }
private:
    MT4Sugar& m_s; std::string m_sym; double m_vol, m_pip{0}; Config m_cfg;
};
```

**Key Sugar calls:** `buyMarket` · `getSymbolInfo` / `getSymbolDigits` (pip sizing) · `getBid` · `getOrderByTicket` · `modifyOrderSL` · `closeOrder`.

> **🆚 MT4:** `buyMarket` returns an **`int` ticket** (`0` = rejected, never throws), and trailing works on an **order** via `modifyOrderSL` / `getOrderByTicket` — MT4 has no separate "position" object.

---

## 🔗 Usage Examples

### 1) Run it with defaults

```cpp
MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");

TrailingStop ts(sugar, "EURUSD", 0.01, TrailingStop::Config{});
ts.run();   // opens, trails for 8 cycles, closes
```

### 2) A tighter, faster trail

```cpp
TrailingStop::Config cfg;
cfg.trailPips  = 10;    // hug price closer
cfg.cycles     = 30;    // manage longer
cfg.intervalMs = 400;   // check more often
TrailingStop(sugar, "EURUSD", 0.02, cfg).run();
```

### 3) Trail an **existing** order instead of opening one

The demo opens its own trade; in a real strategy you often already hold one. The core is just the ratchet — apply it to any ticket:

```cpp
void trailExisting(MT4Sugar& sugar, int ticket, double trailPips, int cycles) {
    bool found = false;
    Order o0 = sugar.getOrderByTicket(ticket, found);
    if (!found) return;
    double pip = sugar.getSymbolInfo(o0.symbol).point;
    int d = sugar.getSymbolDigits(o0.symbol);
    if (d == 3 || d == 5) pip *= 10.0;          // 3/5-digit → ×10

    double bestSL = o0.stopLoss;
    for (int c = 0; c < cycles; ++c) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bool ok = false; Order o = sugar.getOrderByTicket(ticket, ok);
        if (!ok) return;
        double wantSL = sugar.getBid(o.symbol) - trailPips * pip;
        if (wantSL > bestSL) { bestSL = wantSL; sugar.modifyOrderSL(ticket, bestSL); }
    }
}
```

### 4) Break-even first, then trail

Lock the stop at entry once in profit, then start trailing:

```cpp
bool found = false;
Order o = sugar.getOrderByTicket(ticket, found);
double pip = sugar.getSymbolInfo(o.symbol).point * 10.0;   // 5-digit FX

if (found && sugar.getBid(o.symbol) > o.openPrice + 10 * pip) {
    sugar.modifyOrderSL(ticket, o.openPrice);              // break-even
    // ... then run the trail loop from example 3 ...
}
```

### 5) Respect the broker's minimum stop distance

A stop too close to price is rejected (`ApiError` 130). Clamp the trail to `getMinStopLevel`:

```cpp
double point   = sugar.getSymbolInfo("EURUSD").point;
double pip      = point * 10.0;
double minDist  = sugar.getMinStopLevel("EURUSD") * point;
double trail    = std::max(20 * pip, minDist);             // never closer than the broker allows
double wantSL   = sugar.getBid("EURUSD") - trail;
```

---

## ⚠️ Pitfalls

* **Only raises the SL.** By design it never loosens the stop — a reversal just hits the last locked level.
* **BUY-only demo.** The example trails a long; for shorts, mirror it (`getAsk`, `wantSL = ask + trail`, only ever *lower* the SL).
* **Min stop distance.** Too tight a `trailPips` → `modifyOrderSL` returns `false` (MQL4 `130`, see example 5).
* **Order may vanish.** `getOrderByTicket(found=false)` means it was closed elsewhere (TP hit, manual close) — the loop bails out cleanly.

---

## 📚 Related

* [Strategies Overview](../Strategies.Master.Overview.md) · [Grid Trader](./GridTrader.md) · [Risk Manager](./RiskManager.md)
* Preset that dispatches to it: [Adaptive Market Mode](../Presets/AdaptiveMarketModePreset.md) (TRENDING → Trailing Stop)
* Sugar: [Order Management](../../MT4Sugar/4.%20Order_Management.md) · [Prices](../../MT4Sugar/2.%20Prices.md)
