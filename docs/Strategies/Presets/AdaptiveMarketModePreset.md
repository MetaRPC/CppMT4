# Preset · Adaptive Market Mode

> A **preset** coordinates orchestrators behind one decision. This one reads recent price action, classifies the market, and dispatches to the matching orchestrator:
>
> * **TRENDING** → [Trailing Stop](../Orchestrators/TrailingStop.md) — ride the move
> * **RANGING** → [Grid Trader](../Orchestrators/GridTrader.md) — fade the range
> * **VOLATILE** → [Risk Manager](../Orchestrators/RiskManager.md) — defensive / kill-switch

**Source:** `examples/demos/presets/16_adaptive_preset.cpp` · **Run:** `./build/demos 16` · **API level:** MID (`service().lastBars`) + HIGH (orchestrators)

!!! warning "Educational example"
    The classifier is a two-number heuristic over 20 H1 bars. It demonstrates *routing a decision to the right strategy* — it is not a market regime model. Whatever it dispatches runs on the **demo** account and cleans up after itself.

---

## 🎯 What it does

1. Pulls the last 20 **H1** bars via `service().lastBars` *(MT4 timeframes are in minutes — `Timeframe::H1 == 60`)*.
2. Computes two ratios from the bars:
   * **trend** = net directional move ÷ average bar range (how far price travelled in "average ranges"),
   * **spike** = biggest bar range ÷ average range (how outsized the largest bar was).
3. Classifies: `spike > 2.5` → **VOLATILE**; else `trend > 2.0` → **TRENDING**; else **RANGING**.
4. Dispatches to the matching orchestrator.

## 🧩 The pattern

```
AdaptiveMarketModePreset → analyse(last 20 H1 bars):
    avgRange, maxRange, netMove
    trend = netMove / avgRange ;  spike = maxRange / avgRange
    spike>2.5 → VOLATILE ; trend>2.0 → TRENDING ; else RANGING
→ dispatch: VOLATILE→RiskManager · TRENDING→TrailingStop · RANGING→GridTrader
```

---

## 🏗️ The preset (annotated)

```cpp
enum class MarketMode { Trending, Ranging, Volatile };

class AdaptiveMarketModePreset {
public:
    AdaptiveMarketModePreset(MT4Sugar& sugar, std::string symbol)
        : m_s(sugar), m_sym(std::move(symbol)) {}

    MarketMode analyse() {
        // drop to the mid-level facade for OHLC bars (timeframe in MINUTES: H1 == 60)
        auto bars = m_s.service().lastBars(m_sym, Timeframe::H1, 20);
        if (bars.size() < 5) return MarketMode::Ranging;    // not enough data → neutral

        double sumRange = 0, maxRange = 0;
        for (const auto& b : bars) {
            double r = b.high - b.low;
            sumRange += r;
            maxRange = std::max(maxRange, r);
        }
        double avgRange = sumRange / bars.size();
        double move  = std::fabs(bars.back().close - bars.front().close);  // net move
        double trend = avgRange > 0 ? move / avgRange : 0;                 // in "ranges"
        double spike = avgRange > 0 ? maxRange / avgRange : 1;             // biggest vs typical

        if (spike > 2.5) return MarketMode::Volatile;       // an outsized bar → defensive
        if (trend > 2.0) return MarketMode::Trending;       // strong directional drift
        return MarketMode::Ranging;
    }

    void execute(const Config& cfg) {
        switch (analyse()) {                                // dispatch to one orchestrator
            case MarketMode::Trending: demo_trailing_stop(m_s, cfg); break;
            case MarketMode::Ranging:  demo_grid_trader(m_s, cfg);   break;
            case MarketMode::Volatile: demo_risk_manager(m_s, cfg);  break;
        }
    }
private:
    MT4Sugar& m_s; std::string m_sym;
};
```

**Key calls:** `service().lastBars(symbol, Timeframe::H1, 20)` → classify → call one of `demo_trailing_stop` / `demo_grid_trader` / `demo_risk_manager`.

> **🆚 MT4:** `Timeframe` values are **minute counts** (`H1 = 60`, `D1 = 1440`); `lastBars` supplies the required start time for you.

---

## 🔗 Usage Examples

### 1) Run it

```cpp
MT4Sugar sugar("mt4.mrpc.pro", 443);
sugar.connect(1235959737, "password", "EGlobal-Demo");

AdaptiveMarketModePreset preset(sugar, "EURUSD");
preset.execute(Config{});   // analyse → pick a mode → run the matching orchestrator
```

### 2) Just the classification (no trading)

```cpp
MarketMode mode = AdaptiveMarketModePreset(sugar, "EURUSD").analyse();
// route it yourself, log it, or gate another decision on it
```

### 3) Tune the thresholds

The cutoffs are the knobs. Make it quicker to call "trending", slower to call "volatile":

```cpp
// in analyse():
if (spike > 3.0) return MarketMode::Volatile;   // only a very big bar = volatile
if (trend > 1.5) return MarketMode::Trending;   // call trends sooner
return MarketMode::Ranging;
```

### 4) Use a different timeframe / lookback

```cpp
auto bars = sugar.service().lastBars("EURUSD", Timeframe::M15, 48);   // last 12h of M15
```

### 5) Add a fourth mode (quiet → do nothing)

```cpp
if (trend < 0.5 && spike < 1.5) return MarketMode::Ranging;   // treat "quiet" as skip
// ... and in execute(), map that case to a no-op instead of placing a grid.
```

---

## ⚠️ Pitfalls

* **Heuristic, not a model.** Two ratios over 20 bars is a demo classifier — validate before trusting it with size.
* **Timeframe is minutes.** Pass `Timeframe::H1` (not a raw `4` / index) — that's the low-level `quoteHistory` enum, a different scheme.
* **Needs history.** Fewer than 5 bars → it defaults to RANGING. Thin symbols / fresh connections may return less than you ask for.
* **Dispatch inherits the orchestrator's risk.** Whatever it routes to places live demo orders — the preset is only as safe as the orchestrator it picks.

---

## 📚 Related

* [Strategies Overview](../Strategies.Master.Overview.md)
* Orchestrators it dispatches to: [Trailing Stop](../Orchestrators/TrailingStop.md) · [Grid Trader](../Orchestrators/GridTrader.md) · [Risk Manager](../Orchestrators/RiskManager.md)
* Service: [Symbols & Quotes → bars/lastBars](../../MT4Service/3.%20Symbols_Quotes.md)
