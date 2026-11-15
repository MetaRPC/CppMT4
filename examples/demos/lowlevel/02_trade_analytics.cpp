/*══════════════════════════════════════════════════════════════════════════════
 FILE: 02_trade_analytics.cpp - LOW-LEVEL MT4Account TRADE STATS & ANALYTICS

 PURPOSE:
   The raw read-only analytics RPCs. MT4 has no server-side orderCalcMargin /
   orderCalcProfit / orderCheck (those are MT5-only); instead the TradeStats service
   returns performance statistics, an equity curve and per-order floating-P&L extrema.
   Read-only — it does NOT place any order (see demo [7] for a Sugar round-trip).

 📚 WHAT THIS DEMO COVERS:
   • tickValueWithSize()   -> tick value / size for position sizing
   • tradeStats()          -> StatsData (profit factor, drawdowns, Sharpe…)
   • equityHistory()       -> EquityHistoryData (balance/equity curve points)
   • tradeUnrealizedPL()   -> TradeUnrealizedPLData (max/min floating P&L per order)
   (to actually trade: orderSend / orderModify / orderCloseDelete)

 🔄 API LEVELS:
   LOW-LEVEL (Account) - THIS FILE: raw protobuf analytics
   HIGH-LEVEL (Sugar)  - calculateRequiredMargin()/calculatePositionSize() → sugar

 🚀 HOW TO RUN:  ./build/demos 2
══════════════════════════════════════════════════════════════════════════════*/

#include <iostream>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

namespace pb = mt4_term_api;   // protobuf types for the raw layer

void demo_trade_analytics(metarpc::MT4Sugar& sugar, const Config& cfg) {
    metarpc::MT4Account& acc = sugar.account();
    const std::string sym = cfg.testSymbol;

    // ── tickValueWithSize ────────────────────────────────────────────────────
    printSub(std::string("tickValueWithSize for ") + sym);

    // ──────────────────────────────────────────────────────────────────────────
    //  1  tickValueWithSize(symbol_names)
    //     Tick value (account currency), tick size and contract size — the basis for
    //     turning a price move into money.
    //     Returns: TickValueWithSizeData → infos() (SymbolTickValueWithSizeInfo).
    // ──────────────────────────────────────────────────────────────────────────
    pb::TickValueWithSizeRequest tvreq; tvreq.add_symbol_names(sym);
    pb::TickValueWithSizeData tv = acc.tickValueWithSize(tvreq);
    for (int i = 0; i < tv.infos_size(); ++i) {
        const auto& t = tv.infos(i);
        std::cout << "  " << t.symbolname() << "  tickValue=" << t.tradetickvalue()
                  << " tickSize=" << t.tradeticksize() << " contract=" << t.tradecontractsize() << "\n";
    }

    // ── tradeStats ───────────────────────────────────────────────────────────
    printSub("tradeStats (account performance)");

    // ──────────────────────────────────────────────────────────────────────────
    //  2  tradeStats()
    //     Server-side performance analytics over the account's trade history.
    //     Returns: StatsData → summary(), profit_factor(), sharpe_ratio(), drawdowns…
    // ──────────────────────────────────────────────────────────────────────────
    pb::TradeStatsRequest sreq;
    pb::StatsData st = acc.tradeStats(sreq);
    const auto& sm = st.summary();
    std::cout << "  open trades  : " << sm.open_trades() << "  open P/L=" << sm.open_profit() << "\n";
    std::cout << "  total profit : " << sm.total_profit() << "\n";
    std::cout << "  trades       : " << st.trades() << "  profit factor=" << st.profit_factor() << "\n";
    std::cout << "  drawdown     : balance=" << st.max_balance_drawdown_raw()
              << "  equity=" << st.max_equity_drawdown_raw() << "\n";

    // ── equityHistory ────────────────────────────────────────────────────────
    printSub("equityHistory (curve points)");

    // ──────────────────────────────────────────────────────────────────────────
    //  3  equityHistory()
    //     The balance/equity curve as a series of points.
    //     Returns: EquityHistoryData → points() (EquityPointData: balance, equity…).
    // ──────────────────────────────────────────────────────────────────────────
    pb::EquityHistoryRequest ereq;
    pb::EquityHistoryData eq = acc.equityHistory(ereq);
    std::cout << "  equity points: " << eq.points_size() << "\n";
    if (eq.points_size() > 0) {
        const auto& p = eq.points(eq.points_size() - 1);
        std::cout << "  latest point : balance=" << p.balance() << "  equity=" << p.equity() << "\n";
    }

    // ── tradeUnrealizedPL ────────────────────────────────────────────────────
    printSub("tradeUnrealizedPL (floating-P&L extrema)");

    // ──────────────────────────────────────────────────────────────────────────
    //  4  tradeUnrealizedPL()
    //     Per open order: the best (max) and worst (min) floating P&L it reached.
    //     Returns: TradeUnrealizedPLData → extrema() (TradeUnrealizedExtremaData).
    // ──────────────────────────────────────────────────────────────────────────
    pb::TradeUnrealizedPLRequest ureq;
    pb::TradeUnrealizedPLData up = acc.tradeUnrealizedPL(ureq);
    std::cout << "  orders with extrema: " << up.extrema_size() << "\n";
    for (int i = 0; i < up.extrema_size() && i < 3; ++i) {
        const auto& x = up.extrema(i);
        std::cout << "    #" << x.ticket() << " " << x.symbol()
                  << "  maxPL=" << x.max_unrealized_pl() << "  minPL=" << x.min_unrealized_pl() << "\n";
    }
}

} // namespace demos
