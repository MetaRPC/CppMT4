// MetaRPC CppMT4 — Layer 3 (MT4Sugar) implementation: thin conveniences over MT4Service.
//
// MT4 specifics woven in here: (1) a rejected order throws from Layer 1, so every trade helper
// wraps the call and reports failure as 0/false instead of propagating the exception; (2) there
// are no "positions" — closeOrder()/getOpenOrders() work on the unified order list; (3) MT4 has no
// accountInfoDouble or orderCalcMargin RPC, so the balance getters read AccountSummary and the
// margin estimate is computed client-side.
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
#include "metarpc/mt4_sugar.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <thread>

namespace metarpc {

namespace {
int64_t nowUnix() {
    return (int64_t)std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}
// Start of today's UTC day (00:00:00) in unix seconds.
int64_t todayStartUtc() {
    int64_t now = nowUnix();
    return now - (now % 86400);
}
// Net money of one historical order (MT4 has no separate fee field).
double netProfit(const HistoryOrder& h) { return h.profit + h.swap + h.commission; }
} // namespace

// ======================================================================================
// MT4Sugar::Impl — owns the service; small shared helpers
// ======================================================================================
struct MT4Sugar::Impl {
    MT4Service service;
    explicit Impl(const std::string& host, int port, const std::string& apiKey,
                  const std::string& name, bool utcTimes)
        : service(host, port, apiKey, name, utcTimes) {}

    // One pip for a symbol: a 5-/3-digit quote uses 10 points per pip, others one point.
    double pipSize(const SymbolInfo& s) const {
        return (s.digits == 3 || s.digits == 5) ? s.point * 10.0 : s.point;
    }

    // Place an order, returning the resulting ticket. MT4 throws on rejection — translate that to a
    // 0 ticket so the sugar one-liners have the "0 == didn't open" contract callers expect.
    int send(const std::string& symbol, OrderType type, double volume,
             double price, double sl, double tp) {
        OrderRequest q; q.symbol = symbol; q.type = type; q.volume = volume;
        q.price = price; q.stopLoss = sl; q.takeProfit = tp;
        try { OrderResult r = service.orderSend(q); return r.ok() ? r.ticket : 0; }
        catch (const ApiError&) { return 0; }
    }

    // Close one order by ticket. For a market order we must hand MT4 a closing price (bid to close a
    // buy, ask to close a sell); a pending order is simply deleted (no price). Returns false if the
    // ticket isn't live or the broker rejects the close.
    bool close(int ticket, double lots) {
        std::vector<Order> live = service.openedOrders();
        for (const auto& o : live) {
            if (o.ticket != ticket) continue;
            double price = 0;
            if (!o.isPending()) {
                Quote qt = service.quote(o.symbol);
                price = o.isBuy() ? qt.bid : qt.ask;   // close buy @ bid, sell @ ask
            }
            try { service.orderClose(ticket, lots, price, 10); return true; }
            catch (const ApiError&) { return false; }
        }
        return false;   // ticket not among the live orders
    }
};

// ======================================================================================
// MT4Sugar — lifecycle / accessors
// ======================================================================================
MT4Sugar::MT4Sugar(const std::string& host, int port, const std::string& apiKey,
                   const std::string& name, bool utcTimes)
    : m_impl(new Impl(host, port, apiKey, name, utcTimes)) {}
MT4Sugar::~MT4Sugar() = default;

MT4Service& MT4Sugar::service() { return m_impl->service; }
MT4Account& MT4Sugar::account() { return m_impl->service.account(); }

// ======================================================================================
// Connection
// ======================================================================================
#pragma region Connection
void MT4Sugar::connect(int64_t login, const std::string& password, const std::string& server,
                       const std::string& name, int timeoutSeconds) {
    m_impl->service.connect(login, password, server, name, timeoutSeconds);
}

// Read MT4_LOGIN / MT4_PASSWORD / MT4_SERVER from the environment and connect — the fastest way to
// stand up a session in an example or a throwaway script without hard-coding credentials.
void MT4Sugar::quickConnect() {
    const char* login = std::getenv("MT4_LOGIN");
    const char* pass = std::getenv("MT4_PASSWORD");
    const char* server = std::getenv("MT4_SERVER");
    if (!login || !pass || !server)
        throw std::runtime_error("quickConnect: set MT4_LOGIN, MT4_PASSWORD and MT4_SERVER");
    connect(std::strtoll(login, nullptr, 10), pass, server);
}

bool MT4Sugar::isConnected() const { return m_impl->service.isConnected(); }

// Liveness: a successful account summary means the terminal is answering.
bool MT4Sugar::ping() {
    try { m_impl->service.accountSummary(); return true; }
    catch (...) { return false; }
}

void MT4Sugar::disconnect() { m_impl->service.close(); }
#pragma endregion

// ======================================================================================
// Balance / margin — MT4 has no per-property getter, so each reads one AccountSummary snapshot.
// ======================================================================================
#pragma region Balance / margin
double MT4Sugar::getBalance()     { return m_impl->service.accountSummary().balance; }
double MT4Sugar::getEquity()      { return m_impl->service.accountSummary().equity; }
double MT4Sugar::getMargin()      { return m_impl->service.accountSummary().margin; }
double MT4Sugar::getFreeMargin()  { return m_impl->service.accountSummary().marginFree; }
double MT4Sugar::getMarginLevel() { return m_impl->service.accountSummary().marginLevel; }
// Floating P/L = equity - balance (one snapshot answers both).
double MT4Sugar::getProfit()      { AccountInfo a = m_impl->service.accountSummary(); return a.equity - a.balance; }
#pragma endregion

// ======================================================================================
// Prices / quotes
// ======================================================================================
#pragma region Prices / quotes
double MT4Sugar::getBid(const std::string& symbol) { return m_impl->service.quote(symbol).bid; }
double MT4Sugar::getAsk(const std::string& symbol) { return m_impl->service.quote(symbol).ask; }
double MT4Sugar::getSpread(const std::string& symbol) {
    Quote q = m_impl->service.quote(symbol);
    return q.ask - q.bid;
}

PriceInfo MT4Sugar::getPriceInfo(const std::string& symbol) {
    Quote q = m_impl->service.quote(symbol);
    return PriceInfo{ symbol, q.bid, q.ask, q.ask - q.bid, q.time };
}

// Poll the quote until the bid reaches targetBid (or any tick when targetBid == 0), up to timeoutMs.
// Returns the last price seen; the symbol is cleared on timeout so callers can detect it.
PriceInfo MT4Sugar::waitForPrice(const std::string& symbol, double targetBid, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    PriceInfo last;
    while (std::chrono::steady_clock::now() < deadline) {
        Quote q = m_impl->service.quote(symbol);
        last = PriceInfo{ symbol, q.bid, q.ask, q.ask - q.bid, q.time };
        if (targetBid <= 0 || q.bid >= targetBid) return last;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    last.symbol.clear();   // timed out
    return last;
}
#pragma endregion

// ======================================================================================
// Simple trading — each is one send() with the matching order type; 0 means "not opened".
// ======================================================================================
#pragma region Simple trading
int MT4Sugar::buyMarket(const std::string& symbol, double volume)  { return m_impl->send(symbol, OrderType::Buy, volume, 0, 0, 0); }
int MT4Sugar::sellMarket(const std::string& symbol, double volume) { return m_impl->send(symbol, OrderType::Sell, volume, 0, 0, 0); }
int MT4Sugar::buyLimit(const std::string& symbol, double volume, double price)  { return m_impl->send(symbol, OrderType::BuyLimit, volume, price, 0, 0); }
int MT4Sugar::sellLimit(const std::string& symbol, double volume, double price) { return m_impl->send(symbol, OrderType::SellLimit, volume, price, 0, 0); }
int MT4Sugar::buyStop(const std::string& symbol, double volume, double price)   { return m_impl->send(symbol, OrderType::BuyStop, volume, price, 0, 0); }
int MT4Sugar::sellStop(const std::string& symbol, double volume, double price)  { return m_impl->send(symbol, OrderType::SellStop, volume, price, 0, 0); }
#pragma endregion

// ======================================================================================
// Trading with SL/TP
// ======================================================================================
#pragma region Trading with SL/TP
int MT4Sugar::buyMarketWithSLTP(const std::string& symbol, double volume, double sl, double tp)  { return m_impl->send(symbol, OrderType::Buy, volume, 0, sl, tp); }
int MT4Sugar::sellMarketWithSLTP(const std::string& symbol, double volume, double sl, double tp) { return m_impl->send(symbol, OrderType::Sell, volume, 0, sl, tp); }
int MT4Sugar::buyLimitWithSLTP(const std::string& symbol, double volume, double price, double sl, double tp)  { return m_impl->send(symbol, OrderType::BuyLimit, volume, price, sl, tp); }
int MT4Sugar::sellLimitWithSLTP(const std::string& symbol, double volume, double price, double sl, double tp) { return m_impl->send(symbol, OrderType::SellLimit, volume, price, sl, tp); }

// Convert pip distances to absolute SL/TP around the current price, then send a market buy. SL sits
// below the ask, TP above it; a 0 pip distance leaves that level unset.
int MT4Sugar::buyMarketWithPips(const std::string& symbol, double volume, double slPips, double tpPips) {
    SymbolInfo s = m_impl->service.symbolParams(symbol);
    double pip = m_impl->pipSize(s);
    Quote q = m_impl->service.quote(symbol);
    double sl = slPips > 0 ? q.ask - slPips * pip : 0;
    double tp = tpPips > 0 ? q.ask + tpPips * pip : 0;
    return m_impl->send(symbol, OrderType::Buy, volume, 0, sl, tp);
}
// Mirror of the above for a sell: SL above the bid, TP below it.
int MT4Sugar::sellMarketWithPips(const std::string& symbol, double volume, double slPips, double tpPips) {
    SymbolInfo s = m_impl->service.symbolParams(symbol);
    double pip = m_impl->pipSize(s);
    Quote q = m_impl->service.quote(symbol);
    double sl = slPips > 0 ? q.bid + slPips * pip : 0;
    double tp = tpPips > 0 ? q.bid - tpPips * pip : 0;
    return m_impl->send(symbol, OrderType::Sell, volume, 0, sl, tp);
}
#pragma endregion

// ======================================================================================
// Order management
// ======================================================================================
#pragma region Order management
// Close a whole market order (lots 0 = full) or delete a pending order.
bool MT4Sugar::closeOrder(int ticket) { return m_impl->close(ticket, 0); }
// Close part of a market order.
bool MT4Sugar::closeOrderPartial(int ticket, double lots) { return m_impl->close(ticket, lots); }

// Close/delete every live order; returns how many succeeded. Re-reads the list first so partial
// failures don't skip the rest.
int MT4Sugar::closeAllOrders() {
    auto live = m_impl->service.openedOrders();
    int closed = 0;
    for (const auto& o : live) if (m_impl->close(o.ticket, 0)) ++closed;
    return closed;
}

// Same, limited to one symbol.
int MT4Sugar::closeAllBySymbol(const std::string& symbol) {
    auto live = m_impl->service.openedOrders();
    int closed = 0;
    for (const auto& o : live) if (o.symbol == symbol && m_impl->close(o.ticket, 0)) ++closed;
    return closed;
}

// Modify only the SL: look up the order's current TP first so we don't clear it. Like the other
// trade helpers, a broker rejection (e.g. ErrInvalidStops — the levels are too close to market) is
// reported as false rather than thrown, so a one-liner never aborts a sequence.
bool MT4Sugar::modifyOrderSL(int ticket, double sl) {
    bool found = false; Order o = getOrderByTicket(ticket, found);
    double tp = found ? o.takeProfit : 0;
    try { return m_impl->service.orderModify(ticket, sl, tp); } catch (const ApiError&) { return false; }
}
// Modify only the TP: keep the current SL.
bool MT4Sugar::modifyOrderTP(int ticket, double tp) {
    bool found = false; Order o = getOrderByTicket(ticket, found);
    double sl = found ? o.stopLoss : 0;
    try { return m_impl->service.orderModify(ticket, sl, tp); } catch (const ApiError&) { return false; }
}
bool MT4Sugar::modifyOrderSLTP(int ticket, double sl, double tp) {
    try { return m_impl->service.orderModify(ticket, sl, tp); } catch (const ApiError&) { return false; }
}
#pragma endregion

// ======================================================================================
// Order information
// ======================================================================================
#pragma region Order information
std::vector<Order> MT4Sugar::getOpenOrders() { return m_impl->service.openedOrders(); }

std::vector<Order> MT4Sugar::getOrdersBySymbol(const std::string& symbol) {
    std::vector<Order> out;
    for (const auto& o : m_impl->service.openedOrders()) if (o.symbol == symbol) out.push_back(o);
    return out;
}

Order MT4Sugar::getOrderByTicket(int ticket, bool& found) {
    for (const auto& o : m_impl->service.openedOrders())
        if (o.ticket == ticket) { found = true; return o; }
    found = false; return Order{};
}

bool MT4Sugar::hasOpenOrder(const std::string& symbol) {
    for (const auto& o : m_impl->service.openedOrders()) if (o.symbol == symbol) return true;
    return false;
}

int MT4Sugar::countOpenOrders() { return (int)m_impl->service.openedOrders().size(); }

double MT4Sugar::getTotalProfit() {
    double sum = 0;
    for (const auto& o : m_impl->service.openedOrders()) sum += o.profit;
    return sum;
}

double MT4Sugar::getProfitBySymbol(const std::string& symbol) {
    double sum = 0;
    for (const auto& o : m_impl->service.openedOrders()) if (o.symbol == symbol) sum += o.profit;
    return sum;
}
#pragma endregion

// ======================================================================================
// History / statistics
// ======================================================================================
#pragma region History / statistics
std::vector<HistoryOrder> MT4Sugar::getHistoryDateRange(int64_t fromUnix, int64_t toUnix) {
    return m_impl->service.ordersHistory(fromUnix, toUnix);
}
std::vector<HistoryOrder> MT4Sugar::getHistoryToday()     { return getHistoryDateRange(todayStartUtc(), nowUnix()); }
std::vector<HistoryOrder> MT4Sugar::getHistoryYesterday() { int64_t s = todayStartUtc(); return getHistoryDateRange(s - 86400, s); }
std::vector<HistoryOrder> MT4Sugar::getHistoryThisWeek()  { return getHistoryDateRange(todayStartUtc() - 6 * 86400, nowUnix()); }
std::vector<HistoryOrder> MT4Sugar::getHistoryThisMonth() { return getHistoryDateRange(todayStartUtc() - 29 * 86400, nowUnix()); }

static double sumNet(const std::vector<HistoryOrder>& v) {
    double s = 0; for (const auto& h : v) s += netProfit(h); return s;
}
double MT4Sugar::getProfitToday()     { return sumNet(getHistoryToday()); }
double MT4Sugar::getProfitThisWeek()  { return sumNet(getHistoryThisWeek()); }
double MT4Sugar::getProfitThisMonth() { return sumNet(getHistoryThisMonth()); }

// Today's win/loss summary from the closed orders (net = profit + swap + commission).
DailyStats MT4Sugar::getDailyStats() {
    auto orders = getHistoryToday();
    DailyStats st;
    for (const auto& h : orders) {
        double net = netProfit(h);
        st.totalOrders++;
        st.totalProfit += net;
        if (net >= 0) st.winningOrders++; else st.losingOrders++;
        st.bestOrder = std::max(st.bestOrder, net);
        st.worstOrder = std::min(st.worstOrder, net);
    }
    st.winRate = st.totalOrders ? (100.0 * st.winningOrders / st.totalOrders) : 0;
    return st;
}
#pragma endregion

// ======================================================================================
// Symbol information
// ======================================================================================
#pragma region Symbol information
SymbolInfo MT4Sugar::getSymbolInfo(const std::string& symbol) { return m_impl->service.symbolParams(symbol); }
std::vector<std::string> MT4Sugar::getAllSymbols() { return m_impl->service.symbolList(); }

// "Available" = the broker returns params for it (symbolParams throws ApiError when it doesn't).
bool MT4Sugar::isSymbolAvailable(const std::string& symbol) {
    try { m_impl->service.symbolParams(symbol); return true; } catch (...) { return false; }
}

int MT4Sugar::getMinStopLevel(const std::string& symbol) {
    return m_impl->service.symbolParams(symbol).stopsLevel;
}
int MT4Sugar::getSymbolDigits(const std::string& symbol) {
    return m_impl->service.symbolParams(symbol).digits;
}
#pragma endregion

// ======================================================================================
// Risk management
// ======================================================================================
#pragma region Risk management
// Margin to open `volume` lots. MT4 exposes no server-side margin calc, so estimate it: prefer the
// symbol's margin-initial when the broker provides it, else fall back to the classic
// contract-size / leverage formula at the current ask.
double MT4Sugar::calculateRequiredMargin(const std::string& symbol, double volume) {
    SymbolInfo s = m_impl->service.symbolParams(symbol);
    if (s.marginInitial > 0) return s.marginInitial * volume;
    AccountInfo a = m_impl->service.accountSummary();
    double ask = m_impl->service.quote(symbol).ask;
    double leverage = a.leverage > 0 ? (double)a.leverage : 100.0;
    double contract = s.contractSize > 0 ? s.contractSize : 100000.0;
    return (contract * volume * ask) / leverage;
}

// Lot size so that `stopLossPips` risks `riskPercent`% of the balance, clamped to the symbol's
// volume min/max and rounded down to its volume step.
double MT4Sugar::calculatePositionSize(const std::string& symbol, double riskPercent, double stopLossPips) {
    if (stopLossPips <= 0) return 0;
    SymbolInfo s = m_impl->service.symbolParams(symbol);
    double pip = m_impl->pipSize(s);
    double valuePerPipPerLot = (s.tickSize > 0) ? s.tickValue * (pip / s.tickSize) : s.tickValue;
    if (valuePerPipPerLot <= 0) return 0;
    double riskAmount = getBalance() * riskPercent / 100.0;
    double lot = riskAmount / (stopLossPips * valuePerPipPerLot);
    if (s.volumeStep > 0) lot = std::floor(lot / s.volumeStep) * s.volumeStep;
    lot = std::max(s.volumeMin, std::min(lot, s.volumeMax));
    return lot;
}

// Biggest lot the free margin can open now, rounded down to the volume step.
double MT4Sugar::getMaxLotSize(const std::string& symbol) {
    double marginPerLot = calculateRequiredMargin(symbol, 1.0);
    if (marginPerLot <= 0) return 0;
    SymbolInfo s = m_impl->service.symbolParams(symbol);
    double lot = getFreeMargin() / marginPerLot;
    if (s.volumeStep > 0) lot = std::floor(lot / s.volumeStep) * s.volumeStep;
    return std::max(0.0, std::min(lot, s.volumeMax));
}

// Pre-flight check: is there enough free margin for `volume` lots right now?
bool MT4Sugar::canOpenOrder(const std::string& symbol, double volume, std::string& reason) {
    double need = calculateRequiredMargin(symbol, volume);
    double free = getFreeMargin();
    if (need > free) {
        reason = "insufficient free margin (need " + std::to_string(need) + ", have " + std::to_string(free) + ")";
        return false;
    }
    reason.clear();
    return true;
}

// Absolute SL/TP prices for a "buy"/"sell" around entryPrice given pip distances.
void MT4Sugar::calculateSLTP(const std::string& symbol, const std::string& direction, double entryPrice,
                             double stopLossPips, double takeProfitPips, double& slOut, double& tpOut) {
    SymbolInfo s = m_impl->service.symbolParams(symbol);
    double pip = m_impl->pipSize(s);
    const bool buy = (direction == "buy" || direction == "BUY" || direction == "Buy");
    if (buy) {
        slOut = stopLossPips > 0 ? entryPrice - stopLossPips * pip : 0;
        tpOut = takeProfitPips > 0 ? entryPrice + takeProfitPips * pip : 0;
    } else {
        slOut = stopLossPips > 0 ? entryPrice + stopLossPips * pip : 0;
        tpOut = takeProfitPips > 0 ? entryPrice - takeProfitPips * pip : 0;
    }
}
#pragma endregion

// ======================================================================================
// Account information
// ======================================================================================
#pragma region Account information
AccountInfo MT4Sugar::getAccountInfo() { return m_impl->service.accountSummary(); }
#pragma endregion

} // namespace metarpc
