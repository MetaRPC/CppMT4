#pragma once
// MetaRPC CppMT4 — Layer 3 of 3: MT4Sugar (high-level convenience).
//
// MT4Sugar owns an MT4Service (Layer 2) and offers the shortest possible one-liners for the
// everyday tasks a strategy needs: "what's my balance?", "what's the bid?", "buy 0.1 lots",
// "close everything". Every method is self-contained — no context, no request objects, no protobuf
// — so prototyping code reads like plain English. When you outgrow it, drop down to service() for
// the friendly-typed facade or account() for raw RPCs.
//
// MT4 model note: there are no "positions" — only ORDERS (market + pending in one list). The sugar
// API therefore talks about orders throughout (getOpenOrders, closeOrder, …). A rejected trade on
// MT4 throws from Layer 1; the sugar helpers catch that and report it as a false / 0 ticket, so a
// one-liner never throws on an ordinary broker rejection.
//
// Like MT4Service, this header is protobuf-free and compiles fast.
//
// ──────────────────────────────────────────────────────────────────────────────────────
// METHODS BY GROUP  (~62)
//   Connection     : connect · quickConnect · isConnected · ping · disconnect
//   Balance/margin : getBalance · getEquity · getMargin · getFreeMargin · getMarginLevel · getProfit
//   Prices/quotes  : getBid · getAsk · getSpread · getPriceInfo · waitForPrice
//   Simple trading : buyMarket · sellMarket · buyLimit · sellLimit · buyStop · sellStop
//   Trading SL/TP  : buy/sellMarketWithSLTP · buy/sellLimitWithSLTP · buy/sellMarketWithPips
//   Order mgmt     : closeOrder · closeOrderPartial · closeAllOrders · closeAllBySymbol ·
//                    modifyOrderSL · modifyOrderTP · modifyOrderSLTP
//   Order info     : getOpenOrders · getOrdersBySymbol · getOrderByTicket · hasOpenOrder ·
//                    countOpenOrders · getTotalProfit · getProfitBySymbol
//   History/stats  : getHistoryToday/Yesterday/ThisWeek/ThisMonth/DateRange ·
//                    getProfitToday/ThisWeek/ThisMonth · getDailyStats
//   Symbol info    : getSymbolInfo · getAllSymbols · isSymbolAvailable · getMinStopLevel · getSymbolDigits
//   Risk           : calculatePositionSize · getMaxLotSize · canOpenOrder ·
//                    calculateRequiredMargin · calculateSLTP
//   Account        : getAccountInfo
// ──────────────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

#include "metarpc/mt4_service.hpp"   // MT4Service + the friendly structs/enums

namespace metarpc {

#pragma region Sugar structs
// ======================================================================================
// Small result structs specific to the sugar layer
// ======================================================================================

/// Compact price snapshot, from getPriceInfo().
struct PriceInfo {
    std::string symbol;
    double bid{0};
    double ask{0};
    double spread{0};       ///< ask - bid, in price units
    int64_t time{0};        ///< quote time (UTC seconds by default)
};

/// A day's trading summary, from getDailyStats().
struct DailyStats {
    int totalOrders{0};
    int winningOrders{0};
    int losingOrders{0};
    double winRate{0};      ///< winning / total, in percent
    double totalProfit{0};  ///< net profit (incl. swap & commission)
    double bestOrder{0};
    double worstOrder{0};
};
#pragma endregion

// ======================================================================================
// MT4Sugar — Layer 3 (high-level, no context, no protobuf)
// ======================================================================================
class MT4Sugar {
public:
    /// Construct the sugar layer and the MT4Service (and MT4Account) it owns. Call connect() or
    /// quickConnect() before trading. Same connection args as the lower layers.
    explicit MT4Sugar(const std::string& host = "mt4.mrpc.pro", int port = 443,
                      const std::string& apiKey = "", const std::string& name = "",
                      bool utcTimes = true);
    ~MT4Sugar();
    MT4Sugar(const MT4Sugar&) = delete;
    MT4Sugar& operator=(const MT4Sugar&) = delete;

    /// The facade layer underneath (friendly types, full API).
    MT4Service& service();
    /// The low-level layer (raw protobuf RPCs).
    MT4Account& account();

#pragma region Connection
    // --- connection ---------------------------------------------------------------------

    /// Connect by login/password/server (same as service().connect()).
    void connect(int64_t login, const std::string& password, const std::string& server,
                 const std::string& name = "", int timeoutSeconds = 120);
    /// One-liner connect that reads credentials from the environment variables
    /// MT4_LOGIN / MT4_PASSWORD / MT4_SERVER. @throws std::runtime_error if any is missing.
    void quickConnect();
    bool isConnected() const;                   ///< true after a successful connect
    /// Lightweight liveness check: fetches the account summary and returns true if it succeeds.
    bool ping();
    void disconnect();                          ///< drop the channel (keeps the terminal alive)
#pragma endregion

#pragma region Balance / margin
    // --- balance / margin (each reads one fresh AccountSummary) --------------------------

    double getBalance();        ///< account balance
    double getEquity();         ///< account equity (balance + floating P/L)
    double getMargin();         ///< used margin
    double getFreeMargin();     ///< free margin
    double getMarginLevel();    ///< margin level, %
    double getProfit();         ///< current floating profit (equity - balance)
#pragma endregion

#pragma region Prices / quotes
    // --- prices / quotes ----------------------------------------------------------------

    double getBid(const std::string& symbol);       ///< current bid
    double getAsk(const std::string& symbol);       ///< current ask
    double getSpread(const std::string& symbol);    ///< ask - bid (price units)
    PriceInfo getPriceInfo(const std::string& symbol);  ///< bid/ask/spread/time in one call
    /// Poll until the bid is at/beyond `targetBid` (or `targetBid` is 0 for any tick), up to
    /// `timeoutMs`. Returns the last price seen; empty symbol on timeout.
    PriceInfo waitForPrice(const std::string& symbol, double targetBid, int timeoutMs = 10000);
#pragma endregion

#pragma region Simple trading
    // --- simple trading (return the resulting ticket, 0 on a rejected order) --------------

    int buyMarket(const std::string& symbol, double volume);
    int sellMarket(const std::string& symbol, double volume);
    int buyLimit(const std::string& symbol, double volume, double price);
    int sellLimit(const std::string& symbol, double volume, double price);
    int buyStop(const std::string& symbol, double volume, double price);
    int sellStop(const std::string& symbol, double volume, double price);
#pragma endregion

#pragma region Trading with SL/TP
    // --- market/pending orders with stop loss / take profit -----------------------------

    int buyMarketWithSLTP(const std::string& symbol, double volume, double sl, double tp);
    int sellMarketWithSLTP(const std::string& symbol, double volume, double sl, double tp);
    /// Pending buy-limit at `price` with absolute SL/TP.
    int buyLimitWithSLTP(const std::string& symbol, double volume, double price, double sl, double tp);
    /// Pending sell-limit at `price` with absolute SL/TP.
    int sellLimitWithSLTP(const std::string& symbol, double volume, double price, double sl, double tp);
    /// Buy at market with SL/TP expressed as a distance in pips from entry.
    int buyMarketWithPips(const std::string& symbol, double volume, double slPips, double tpPips);
    int sellMarketWithPips(const std::string& symbol, double volume, double slPips, double tpPips);
#pragma endregion

#pragma region Order management
    // --- order management ---------------------------------------------------------------

    bool closeOrder(int ticket);                                 ///< close a market order / delete a pending
    bool closeOrderPartial(int ticket, double lots);            ///< close part of a market order
    int closeAllOrders();                                        ///< close/delete every order; returns count
    int closeAllBySymbol(const std::string& symbol);             ///< close/delete all orders on a symbol
    bool modifyOrderSL(int ticket, double sl);                   ///< set stop loss only (keeps TP)
    bool modifyOrderTP(int ticket, double tp);                   ///< set take profit only (keeps SL)
    bool modifyOrderSLTP(int ticket, double sl, double tp);      ///< set both SL and TP
#pragma endregion

#pragma region Order information
    // --- order information (market + pending; MT4 has no "positions") --------------------

    std::vector<Order> getOpenOrders();                          ///< all live orders
    std::vector<Order> getOrdersBySymbol(const std::string& symbol);
    /// The order with this ticket; `found` is set false if there is none.
    Order getOrderByTicket(int ticket, bool& found);
    bool hasOpenOrder(const std::string& symbol);                ///< any order on the symbol?
    int countOpenOrders();                                       ///< number of live orders
    double getTotalProfit();                                     ///< summed floating profit
    double getProfitBySymbol(const std::string& symbol);         ///< floating profit on one symbol
#pragma endregion

#pragma region History / statistics
    // --- history / statistics (UTC day boundaries) --------------------------------------

    std::vector<HistoryOrder> getHistoryToday();
    std::vector<HistoryOrder> getHistoryYesterday();
    std::vector<HistoryOrder> getHistoryThisWeek();
    std::vector<HistoryOrder> getHistoryThisMonth();
    std::vector<HistoryOrder> getHistoryDateRange(int64_t fromUnix, int64_t toUnix);
    double getProfitToday();
    double getProfitThisWeek();
    double getProfitThisMonth();
    DailyStats getDailyStats();     ///< today's win/loss summary
#pragma endregion

#pragma region Symbol information
    // --- symbol information -------------------------------------------------------------

    SymbolInfo getSymbolInfo(const std::string& symbol);    ///< full params for a symbol
    std::vector<std::string> getAllSymbols();               ///< every symbol name
    bool isSymbolAvailable(const std::string& symbol);      ///< the broker knows this symbol
    int getMinStopLevel(const std::string& symbol);         ///< min SL/TP distance, in points
    int getSymbolDigits(const std::string& symbol);         ///< price decimal places
#pragma endregion

#pragma region Risk management
    // --- risk management ----------------------------------------------------------------

    /// Lot size so that a `stopLossPips` stop risks `riskPercent`% of the balance.
    double calculatePositionSize(const std::string& symbol, double riskPercent, double stopLossPips);
    /// Largest lot the current free margin can open on this symbol.
    double getMaxLotSize(const std::string& symbol);
    /// Whether a `volume`-lot order can be opened now; `reason` explains a false.
    bool canOpenOrder(const std::string& symbol, double volume, std::string& reason);
    /// Margin required to open `volume` lots on this symbol. MT4 has no server-side margin RPC, so
    /// this is estimated from the symbol's margin-initial (or contract size / leverage).
    double calculateRequiredMargin(const std::string& symbol, double volume);
    /// Absolute SL/TP prices for a direction ("buy"/"sell") given pip distances from entry.
    void calculateSLTP(const std::string& symbol, const std::string& direction, double entryPrice,
                       double stopLossPips, double takeProfitPips, double& slOut, double& tpOut);
#pragma endregion

#pragma region Account information
    // --- account information ------------------------------------------------------------

    AccountInfo getAccountInfo();   ///< full account snapshot
#pragma endregion

    struct Impl;
private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace metarpc
