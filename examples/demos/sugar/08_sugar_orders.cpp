/*══════════════════════════════════════════════════════════════════════════════
 FILE: 08_sugar_orders.cpp - SUGAR ORDER INFORMATION (read-only)

 PURPOSE:
   Inspect live orders with the high-level helpers. Read-only: it lists and sums but
   does not close anything (closing helpers are listed at the end). MT4 keeps market
   AND pending orders in one list — Order::isPending() tells them apart.

 📚 WHAT THIS DEMO COVERS:
   • getOpenOrders() / getOrdersBySymbol()
   • hasOpenOrder() / countOpenOrders()
   • getTotalProfit() / getProfitBySymbol()
   (closing: closeOrder, closeOrderPartial, closeAllOrders, closeAllBySymbol)

 🔄 API LEVELS:  HIGH-LEVEL (Sugar) — THIS FILE.

 🚀 HOW TO RUN:  ./build/demos 8
══════════════════════════════════════════════════════════════════════════════*/

#include <iostream>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

using namespace metarpc;

void demo_sugar_orders(MT4Sugar& sugar, const Config& cfg) {
    const std::string sym = cfg.testSymbol;

    printSub("Overview");

    // ──────────────────────────────────────────────────────────────────────────
    //  Aggregate reads (one call each):
    //    countOpenOrders()     → int        — how many orders are live
    //    getTotalProfit()      → double     — summed floating P&L
    //    hasOpenOrder(sym)     → bool        — any order on this symbol?
    //    getProfitBySymbol(sym)→ double      — floating P&L on this symbol
    // ──────────────────────────────────────────────────────────────────────────
    std::cout << "  live orders    : " << sugar.countOpenOrders() << "\n";
    std::cout << "  total profit   : " << sugar.getTotalProfit() << "\n";
    std::cout << "  has " << sym << " order : " << (sugar.hasOpenOrder(sym) ? "yes" : "no") << "\n";
    std::cout << "  profit on " << sym << "  : " << sugar.getProfitBySymbol(sym) << "\n";

    printSub("All live orders");

    // ──────────────────────────────────────────────────────────────────────────
    //  getOpenOrders()
    //     Every live order (market + pending) as a friendly struct.
    //     Returns: std::vector<Order> (ticket, symbol, type, lots, P/L…).
    // ──────────────────────────────────────────────────────────────────────────
    auto orders = sugar.getOpenOrders();
    if (orders.empty()) {
        std::cout << "  (none — run demo [7] to open a short-lived test order)\n";
    } else {
        for (const auto& o : orders)
            std::cout << "  #" << o.ticket << "  " << o.symbol << "  " << orderTypeName(o.type)
                      << (o.isPending() ? " [pending]" : " [market]")
                      << "  lots=" << o.lots << "  open=" << o.openPrice << "  P/L=" << o.profit << "\n";
    }

    printSub(std::string("Orders on ") + sym);

    // ──────────────────────────────────────────────────────────────────────────
    //  getOrdersBySymbol(symbol)
    //     Only the orders on one symbol.
    //     Returns: std::vector<Order>.
    // ──────────────────────────────────────────────────────────────────────────
    auto bySym = sugar.getOrdersBySymbol(sym);
    std::cout << "  " << bySym.size() << " order(s) on " << sym << "\n";
}

} // namespace demos
