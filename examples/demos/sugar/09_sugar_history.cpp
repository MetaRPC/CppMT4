/*══════════════════════════════════════════════════════════════════════════════
 FILE: 09_sugar_history.cpp - SUGAR HISTORY & STATISTICS

 PURPOSE:
   Read-only history helpers: closed orders over common windows and a quick daily
   win/loss summary — one call each, UTC day boundaries handled for you.

 📚 WHAT THIS DEMO COVERS:
   • getHistoryToday / Yesterday / ThisWeek / ThisMonth -> vector<HistoryOrder>
   • getProfitToday / ThisWeek / ThisMonth              -> net profit
   • getDailyStats()                                    -> DailyStats (win rate, best/worst)

 🔄 API LEVELS:  HIGH-LEVEL (Sugar) — THIS FILE.

 🚀 HOW TO RUN:  ./build/demos 9
══════════════════════════════════════════════════════════════════════════════*/

#include <iostream>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

using namespace metarpc;

void demo_sugar_history(MT4Sugar& sugar, const Config& /*cfg*/) {
    printSub("Closed orders by window");

    // ──────────────────────────────────────────────────────────────────────────
    //  Windowed history (UTC day boundaries handled for you):
    //    getHistoryToday/Yesterday/ThisWeek/ThisMonth() → std::vector<HistoryOrder>
    //    getProfitToday/ThisWeek/ThisMonth()            → double (net realized profit)
    // ──────────────────────────────────────────────────────────────────────────
    std::cout << "  today      : " << sugar.getHistoryToday().size()     << " orders,  profit " << sugar.getProfitToday()     << "\n";
    std::cout << "  yesterday  : " << sugar.getHistoryYesterday().size() << " orders\n";
    std::cout << "  this week  : " << sugar.getHistoryThisWeek().size()  << " orders,  profit " << sugar.getProfitThisWeek()  << "\n";
    std::cout << "  this month : " << sugar.getHistoryThisMonth().size() << " orders,  profit " << sugar.getProfitThisMonth() << "\n";

    printSub("Today's statistics");

    // ──────────────────────────────────────────────────────────────────────────
    //  getDailyStats()
    //     Today's win/loss summary in one struct.
    //     Returns: DailyStats (totalOrders, winning/losing, winRate, totalProfit, best/worst).
    // ──────────────────────────────────────────────────────────────────────────
    DailyStats st = sugar.getDailyStats();
    std::cout << "  total orders : " << st.totalOrders << "\n";
    std::cout << "  winning      : " << st.winningOrders << "\n";
    std::cout << "  losing       : " << st.losingOrders << "\n";
    std::cout << "  win rate     : " << st.winRate << " %\n";
    std::cout << "  total profit : " << st.totalProfit << "\n";
    std::cout << "  best / worst : " << st.bestOrder << " / " << st.worstOrder << "\n";

    printSub("Most recent closed orders (up to 5)");

    // ──────────────────────────────────────────────────────────────────────────
    //  getHistoryThisMonth()  — reuse the month window, show the latest few
    //     Returns: std::vector<HistoryOrder> (ticket, symbol, lots, profit, closeTime…).
    // ──────────────────────────────────────────────────────────────────────────
    auto hist = sugar.getHistoryThisMonth();
    int shown = 0;
    for (auto it = hist.rbegin(); it != hist.rend() && shown < 5; ++it, ++shown)
        std::cout << "  #" << it->ticket << "  " << it->symbol
                  << "  lots=" << it->lots << "  profit=" << it->profit
                  << "  closed t=" << it->closeTime << "\n";
    if (hist.empty()) std::cout << "  (no closed orders this month)\n";
}

} // namespace demos
