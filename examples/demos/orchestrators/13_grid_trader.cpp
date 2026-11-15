/*══════════════════════════════════════════════════════════════════════════════
 FILE: 13_grid_trader.cpp - ORCHESTRATOR: GRID TRADER  (⚠ places demo pending orders)

 PURPOSE:
   Lays a symmetric grid of pending orders around the current price: buy-limits below
   and sell-limits above, `levels` each side, `stepPips` apart. Lists what it placed,
   then cleans the whole grid up again. Demonstrates pending-order placement + removal.

 📚 PATTERN:
   GridTrader{config} → place grid (buyLimit / sellLimit) → list (service().openedOrders()
   filtered by isPending()) → remove (closeOrder deletes a pending by ticket). Built on
   MT4Sugar + service().

 ⚠ SAFETY: places small live pending orders on the demo account and deletes them all
   before returning. Nothing is left open.

 🔄 API LEVELS:  HIGH-LEVEL (Sugar) + service().   🚀 HOW TO RUN:  ./build/demos 13
══════════════════════════════════════════════════════════════════════════════*/

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

using namespace metarpc;

class GridTrader {
public:
    struct Config {
        int    levels{2};        // pending orders on each side
        double stepPips{20.0};   // distance between levels
        double volume{0.01};
    };

    GridTrader(MT4Sugar& sugar, std::string symbol, Config cfg)
        : m_s(sugar), m_sym(std::move(symbol)), m_cfg(cfg) {
        m_pip = sugar.getSymbolInfo(m_sym).point;
        if (sugar.getSymbolDigits(m_sym) == 3 || sugar.getSymbolDigits(m_sym) == 5) m_pip *= 10.0;
    }

    void run() {
        // ──────────────────────────────────────────────────────────────────────
        //  1  getBid(symbol) / getAsk(symbol) → double   — centre the grid on price
        // ──────────────────────────────────────────────────────────────────────
        double bid = m_s.getBid(m_sym), ask = m_s.getAsk(m_sym);
        std::cout << "  grid around bid=" << bid << " ask=" << ask
                  << "  (" << m_cfg.levels << " levels/side, " << m_cfg.stepPips << " pips step)\n";

        // ── place the grid ───────────────────────────────────────────────────
        // ──────────────────────────────────────────────────────────────────────
        //  2  buyLimit(symbol, volume, price)  / sellLimit(...)   ⚠ REAL pending orders
        //     A buy-limit `i` steps below and a sell-limit `i` steps above, per level.
        //     Returns: int ticket each (0 if rejected).
        // ──────────────────────────────────────────────────────────────────────
        std::vector<int> tickets;
        for (int i = 1; i <= m_cfg.levels; ++i) {
            double below = bid - i * m_cfg.stepPips * m_pip;    // buy-limit under price
            double above = ask + i * m_cfg.stepPips * m_pip;    // sell-limit over price
            int b = m_s.buyLimit(m_sym, m_cfg.volume, below);
            int s = m_s.sellLimit(m_sym, m_cfg.volume, above);
            if (b) { tickets.push_back(b); std::cout << "    buyLimit  @ " << below << "  #" << b << "\n"; }
            if (s) { tickets.push_back(s); std::cout << "    sellLimit @ " << above << "  #" << s << "\n"; }
        }
        if (tickets.empty()) { std::cout << "  no orders placed (market closed / too close to price?)\n"; return; }

        // ── show the live pending orders ─────────────────────────────────────
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        // ──────────────────────────────────────────────────────────────────────
        //  3  service().openedOrders()   — drop to mid-level; count the pendings
        //     MT4 returns market + pending together — filter by Order::isPending().
        // ──────────────────────────────────────────────────────────────────────
        int pending = 0;
        for (const auto& o : m_s.service().openedOrders()) if (o.isPending()) ++pending;
        std::cout << "  pending orders now live: " << pending << "\n";

        // ── clean up: delete everything we placed ────────────────────────────
        // ──────────────────────────────────────────────────────────────────────
        //  4  closeOrder(ticket)   ⚠ deletes a pending order by ticket
        //     (same call closes a market order; here each ticket is a pending order).
        // ──────────────────────────────────────────────────────────────────────
        int removed = 0;
        for (int t : tickets) if (m_s.closeOrder(t)) ++removed;
        std::cout << "  removed " << removed << "/" << tickets.size() << " grid orders\n";
    }

private:
    MT4Sugar& m_s;
    std::string m_sym;
    double m_pip{0};
    Config m_cfg;
};

void demo_grid_trader(MT4Sugar& sugar, const Config& cfg) {
    printSub("Grid trader (place a grid, list it, remove it)");
    GridTrader gt(sugar, cfg.testSymbol, GridTrader::Config{2, 20.0, cfg.testVolume});
    gt.run();
}

} // namespace demos
