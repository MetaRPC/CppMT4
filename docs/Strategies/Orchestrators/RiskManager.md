# Orchestrator · Risk Manager

> A **monitoring** orchestrator — a kill-switch. Each cycle it reads account health (equity, margin level, floating P/L, open orders); if drawdown or margin level breaches a limit, it **closes everything**. On a healthy/flat account it just reports. Built entirely on [`MT4Sugar`](../../MT4Sugar/MT4Sugar.API_Overview.md).

**Source:** `examples/demos/orchestrators/14_risk_manager.cpp` · **Run:** `./build/demos 14` · **API level:** HIGH (Sugar)

!!! note "Read-only unless a limit breaks"
    This orchestrator opens nothing. It only reads metrics — and acts just once, by flattening all orders, if a guard rail is crossed. Safe to run on any account.

---

## 🎯 What it does

1. Captures the start balance.
2. Each cycle reads equity, margin level, floating P/L and the open-order count.
3. Computes drawdown `%` from the start balance.
4. **Breach?** (`drawdown > maxDrawdownPct` **or** `open > 0 && 0 < marginLevel < minMarginLevel`) → `closeAllOrders()` and stop.

## 🧩 The pattern

```
RiskManager{config} → run(cycles):
    read equity / marginLevel / floatP&L / openCount
    ddPct = (startBalance - equity) / startBalance * 100
    if ddPct > maxDrawdownPct OR (open>0 && marginLevel in (0, minMarginLevel)):
        closeAllOrders()   → break
    → repeat
```

## ⚙️ Config

```cpp
struct Config {
    double maxDrawdownPct{5.0};    // flatten if equity is this % below the start balance
    double minMarginLevel{150.0};  // ... or if the margin level drops below this %
    int    cycles{5};
    int    intervalMs{800};
};
```

---

## 🏗️ The orchestrator (annotated)

```cpp
class RiskManager {
public:
    struct Config { double maxDrawdownPct{5.0}; double minMarginLevel{150.0}; int cycles{5}; int intervalMs{800}; };

    RiskManager(MT4Sugar& sugar, Config cfg) : m_s(sugar), m_cfg(cfg) {
        m_startBalance = sugar.getBalance();      // baseline for drawdown
    }

    void run() {
        for (int c = 1; c <= m_cfg.cycles; ++c) {
            double equity      = m_s.getEquity();        // balance + floating P/L
            double marginLevel = m_s.getMarginLevel();   // percent (0 when flat)
            double floating    = m_s.getTotalProfit();   // summed floating P/L
            int    open        = m_s.countOpenOrders();  // live orders
            double ddPct = m_startBalance > 0
                         ? (m_startBalance - equity) / m_startBalance * 100.0 : 0.0;

            bool breach = (ddPct > m_cfg.maxDrawdownPct) ||
                          (open > 0 && marginLevel > 0 && marginLevel < m_cfg.minMarginLevel);
            if (breach) {
                int closed = m_s.closeAllOrders();       // ⚠ kill-switch: flatten everything
                (void)closed;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(m_cfg.intervalMs));
        }
    }
private:
    MT4Sugar& m_s; Config m_cfg; double m_startBalance{0};
};
```

**Key Sugar calls:** `getBalance` (baseline) · `getEquity` · `getMarginLevel` · `getTotalProfit` · `countOpenOrders` · `closeAllOrders`.

> **🆚 MT4:** the kill-switch is `closeAllOrders()` — it closes market orders **and** deletes pendings (MT4's one order list). `getMarginLevel()` is `0` when nothing is open, so the margin test guards on `open > 0 && marginLevel > 0`.

---

## 🔗 Usage Examples

### 1) Run it with defaults

```cpp
MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");

RiskManager rm(sugar, RiskManager::Config{});
rm.run();   // monitors 5 cycles; flattens on a breach
```

### 2) Stricter limits, longer watch

```cpp
RiskManager::Config cfg;
cfg.maxDrawdownPct = 2.0;     // tighter drawdown cap
cfg.minMarginLevel = 300.0;   // demand more headroom
cfg.cycles         = 60;      // watch longer
cfg.intervalMs     = 1000;
RiskManager(sugar, cfg).run();
```

### 3) One-shot health check (no loop)

```cpp
bool breached(MT4Sugar& sugar, double startBalance, double maxDDpct, double minMargin) {
    double ddPct = (startBalance - sugar.getEquity()) / startBalance * 100.0;
    double ml = sugar.getMarginLevel();
    int open = sugar.countOpenOrders();
    return ddPct > maxDDpct || (open > 0 && ml > 0 && ml < minMargin);
}
```

### 4) Daily-loss kill-switch

Flatten when today's realized + floating loss exceeds a cash cap:

```cpp
double todaysPnL = sugar.getProfitToday() + sugar.getTotalProfit();  // realized + floating
if (todaysPnL < -200.0) {                                            // -$200 daily stop
    int closed = sugar.closeAllOrders();
    std::printf("daily stop hit — closed %d\n", closed);
}
```

### 5) Per-symbol guard instead of account-wide

```cpp
void capSymbol(MT4Sugar& sugar, const std::string& sym, double maxLoss) {
    if (sugar.getProfitBySymbol(sym) < -maxLoss)
        sugar.closeAllBySymbol(sym);     // flatten just this instrument
}
```

---

## ⚠️ Pitfalls

* **Margin level is 0 when flat.** Always pair the margin test with `open > 0 && marginLevel > 0`, or a flat account reads as a breach.
* **Each getter is a round-trip.** `getEquity` / `getMarginLevel` / … each fetch a fresh summary; for many figures at once read `getAccountInfo()` once.
* **Kill-switch is total.** `closeAllOrders()` flattens *everything*, including orders other strategies placed. Scope it (e.g. `closeAllBySymbol`) if you share the account.
* **Drawdown baseline.** It's measured against the **start balance** captured in the constructor — reconstruct the object to reset the baseline.

---

## 📚 Related

* [Strategies Overview](../Strategies.Master.Overview.md) · [Trailing Stop](./TrailingStop.md) · [Grid Trader](./GridTrader.md)
* Preset that dispatches to it: [Adaptive Market Mode](../Presets/AdaptiveMarketModePreset.md) (VOLATILE → Risk Manager)
* Sugar: [Connection & Account](../../MT4Sugar/1.%20Connection_Account.md) · [Order Management](../../MT4Sugar/4.%20Order_Management.md)
