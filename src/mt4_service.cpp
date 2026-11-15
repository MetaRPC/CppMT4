// MetaRPC CppMT4 — Layer 2 (MT4Service) implementation: build requests, call MT4Account, convert
// protobuf payloads into the friendly structs, and select broker-server vs UTC times.
//
// MT4 carries BOTH a raw server time and a UTC time on the wire (e.g. open_time / open_time_utc),
// so this layer does not do offset arithmetic on payloads — it simply reads the `_utc` field when
// utcTimes is on, and the server field when it is off. (toUtc/toServerTime still use the cached
// offset for callers who want to convert their own values.)
//
// ──────────────────────────────────────────────────────────────────────────────────────
// METHODS BY GROUP
//   Connection     : connect · reconnect · isConnected · terminalGuid · disconnect · close · options
//   Time           : serverOffsetSeconds · toUtc · toServerTime · serverTimezone
//   Account        : accountSummary
//   Symbols/quotes : quote · quoteMany · symbolSelect · symbolList · symbolParams ·
//                    symbolParamsMany · tickValueWithSize
//   Bars           : bars · lastBars
//   Orders         : openedOrders · openedOrders(magic) · openedOrdersTickets · ordersHistory
//   Trading        : orderSend · orderModify · orderClose · orderCloseBy
//   Trade stats    : tradeStats · equityHistory · tradeUnrealizedPL
//   Streaming      : subscribeTicks · streamTradeUpdates · streamOrderProfits ·
//                    streamOpenedTickets · streamConnectState
// ──────────────────────────────────────────────────────────────────────────────────────
#include "metarpc/mt4_service.hpp"
#include "metarpc/mt4_account.hpp"

#include <google/protobuf/timestamp.pb.h>

#include <algorithm>
#include <ctime>

namespace metarpc {

namespace gpb = ::google::protobuf;

// ======================================================================================
// Timeframe / order-type helpers (pure)
// ======================================================================================
#pragma region Enum helpers
// MT4 timeframe values ARE the minute count (M1=1 … MN1=43200), so seconds = minutes * 60.
int timeframeSeconds(Timeframe tf) { return (int)tf * 60; }

const char* timeframeName(Timeframe tf) {
    switch (tf) {
        case Timeframe::M1: return "M1";   case Timeframe::M5: return "M5";   case Timeframe::M15: return "M15";
        case Timeframe::M30: return "M30"; case Timeframe::H1: return "H1";   case Timeframe::H4: return "H4";
        case Timeframe::D1: return "D1";   case Timeframe::W1: return "W1";   case Timeframe::MN1: return "MN1";
    }
    return "?";
}

const char* orderTypeName(OrderType t) {
    switch (t) {
        case OrderType::Buy: return "Buy";             case OrderType::Sell: return "Sell";
        case OrderType::BuyLimit: return "BuyLimit";   case OrderType::SellLimit: return "SellLimit";
        case OrderType::BuyStop: return "BuyStop";     case OrderType::SellStop: return "SellStop";
        case OrderType::Balance: return "Balance";     case OrderType::Credit: return "Credit";
    }
    return "?";
}
#pragma endregion

// ======================================================================================
// protobuf -> friendly struct converters (the heart of this layer)
// ======================================================================================
#pragma region Converters
namespace {

// Pick the server or the UTC seconds of a dual Timestamp pair, by the client's time policy.
inline int64_t pick(bool utc, const gpb::Timestamp& srv, const gpb::Timestamp& u) {
    return utc ? u.seconds() : srv.seconds();
}

Quote quoteFrom(const pb::QuoteData& q, bool utc) {
    Quote o; o.symbol = q.symbol(); o.bid = q.bid(); o.ask = q.ask(); o.high = q.high(); o.low = q.low();
    o.time = pick(utc, q.date_time(), q.date_time_utc());
    return o;
}

Order orderFrom(const pb::OpenedOrderInfo& p, bool utc) {
    Order o; o.ticket = p.ticket(); o.symbol = p.symbol(); o.type = (OrderType)(int)p.order_type();
    o.lots = p.lots(); o.openPrice = p.open_price(); o.stopLoss = p.stop_loss(); o.takeProfit = p.take_profit();
    o.profit = p.profit(); o.swap = p.swap(); o.commission = p.commision(); o.magic = p.magic_number();
    o.comment = p.comment();
    o.openTime = pick(utc, p.open_time(), p.open_time_utc());
    o.expirationTime = pick(utc, p.expiration_time(), p.expiration_time_utc());
    return o;
}

HistoryOrder historyOrderFrom(const pb::HistoryOrderInfo& p, bool utc) {
    HistoryOrder o; o.ticket = p.ticket(); o.symbol = p.symbol(); o.type = (OrderType)(int)p.order_type();
    o.lots = p.lots(); o.openPrice = p.open_price(); o.closePrice = p.close_price();
    o.stopLoss = p.stop_loss(); o.takeProfit = p.take_profit(); o.profit = p.profit(); o.swap = p.swap();
    o.commission = p.commision(); o.magic = p.magic_number(); o.comment = p.comment();
    o.openTime = pick(utc, p.open_time(), p.open_time_utc());
    o.closeTime = pick(utc, p.close_time(), p.close_time_utc());
    o.expirationTime = pick(utc, p.expiration_time(), p.expiration_time_utc());
    return o;
}

// Flatten one SymbolParamsManyInfo entry into a SymbolInfo (the useful subset of ~70 fields).
SymbolInfo symbolFrom(const pb::SymbolParamsManyInfo& p, bool utc) {
    SymbolInfo s;
    s.name = p.symbolname(); s.description = p.symdescription(); s.path = p.path();
    s.currencyBase = p.currencybase(); s.currencyProfit = p.currencyprofit(); s.currencyMargin = p.currencymargin();
    s.digits = p.digits(); s.point = p.point(); s.bid = p.bid(); s.last = p.thelast();
    s.bidHigh = p.bidhigh(); s.bidLow = p.bidlow(); s.askHigh = p.askhigh(); s.askLow = p.asklow();
    s.spreadFloat = p.spreadfloat();
    s.contractSize = p.tradecontractsize(); s.tickSize = p.tradeticksize(); s.tickValue = p.tradetickvalue();
    s.volumeMin = p.volumemin(); s.volumeMax = p.volumemax(); s.volumeStep = p.volumestep(); s.volumeLimit = p.volumelimit();
    s.stopsLevel = p.tradestopslevel(); s.freezeLevel = p.tradefreezelevel();
    s.tradeMode = (int)p.trademode(); s.tradeCalcMode = (int)p.tradecalcmode(); s.tradeExeMode = (int)p.tradeexemode();
    s.swapMode = p.swapmode(); s.swapLong = p.swaplong(); s.swapShort = p.swapshort();
    s.marginInitial = p.margininitial(); s.marginMaintenance = p.marginmaintenance();
    s.visible = p.visible(); s.selected = p.is_selected();
    s.time = utc ? p.timeutc() : p.time();   // these two are plain int64 unix seconds, not Timestamp
    return s;
}

EventAccountInfo eventAccountFrom(const pb::OnEventAccountInfo& a) {
    EventAccountInfo o;
    o.login = a.login(); o.balance = a.balance(); o.credit = a.credit(); o.equity = a.equity();
    o.margin = a.margin(); o.freeMargin = a.free_margin(); o.profit = a.profit(); o.marginLevel = a.margin_level();
    return o;
}

TradeOrderInfo tradeOrderFrom(const pb::OnTradeOrderInfo& p, bool utc) {
    TradeOrderInfo o;
    o.index = p.index(); o.ticket = p.ticket(); o.isHistory = p.is_history(); o.symbol = p.symbol();
    o.type = (OrderType)(int)p.type(); o.lots = p.lots(); o.openPrice = p.open_price(); o.closePrice = p.close_price();
    o.stopLoss = p.stop_loss(); o.takeProfit = p.take_profit(); o.profit = p.order_profit(); o.swap = p.swap();
    o.commission = p.commission(); o.magic = p.magic_number(); o.comment = p.comment();
    o.openTime = pick(utc, p.open_time(), p.open_time_utc());
    o.closeTime = pick(utc, p.close_time(), p.close_time_utc());
    o.expiration = pick(utc, p.expiration(), p.expiration_utc());
    return o;
}

OrderProfitInfo orderProfitFrom(const pb::OnOpenedOrdersProfitOrderInfo& p, bool utc) {
    OrderProfitInfo o;
    o.index = p.index(); o.ticket = p.ticket(); o.symbol = p.symbol(); o.type = (OrderType)(int)p.type();
    o.lots = p.lots(); o.openPrice = p.open_price(); o.profit = p.order_profit(); o.stopLoss = p.stop_loss();
    o.takeProfit = p.take_profit(); o.swap = p.swap(); o.commission = p.commission(); o.magic = p.magic_number();
    o.comment = p.comment();
    o.openTime = pick(utc, p.open_time(), p.open_time_utc());
    return o;
}

} // namespace
#pragma endregion

// ======================================================================================
// MT4Service::Impl — owns the MT4Account and the UTC policy
// ======================================================================================
#pragma region Impl
struct MT4Service::Impl {
    MT4Account account;
    bool utcTimes{true};
    std::string server;   // remembered from connect() so accountSummary() can report it

    explicit Impl(const std::string& host, int port, const std::string& apiKey, const std::string& name)
        : account(host, port, apiKey, name) {}
};
#pragma endregion

// ======================================================================================
// MT4Service — lifecycle / connection / time
// ======================================================================================
#pragma region Lifecycle / connection / time
MT4Service::MT4Service(const std::string& host, int port, const std::string& apiKey,
                       const std::string& name, bool utcTimes)
    : m_impl(new Impl(host, port, apiKey, name)) {
    m_impl->utcTimes = utcTimes;
}
MT4Service::~MT4Service() = default;

MT4Account& MT4Service::account() { return m_impl->account; }

// Remember the server name (for accountSummary) and delegate the actual ConnectEx to Layer 1.
void MT4Service::connect(int64_t login, const std::string& password, const std::string& server,
                         const std::string& name, int timeoutSeconds) {
    m_impl->server = server;
    m_impl->account.connect(login, password, server, name, timeoutSeconds);
}
void MT4Service::reconnect() { m_impl->account.reconnect(); }
bool MT4Service::isConnected() const { return m_impl->account.isConnected(); }
std::string MT4Service::terminalGuid() const { return m_impl->account.terminalGuid(); }
void MT4Service::disconnect(const std::string& reason) { m_impl->account.disconnect(reason); }
void MT4Service::close() { m_impl->account.close(); }
ClientOptions& MT4Service::options() { return m_impl->account.options(); }

// Time conversion helpers for values YOU hold (payloads are already converted via the _utc fields).
// serverOffsetSeconds() is the broker-clock-minus-UTC shift, learned once from an AccountSummary and
// cached in Layer 1; toUtc()/toServerTime() apply it both ways. A 0 argument passes through as 0.
int64_t MT4Service::serverOffsetSeconds() { return m_impl->account.serverOffsetSeconds(); }
int64_t MT4Service::toUtc(int64_t serverTime) { return serverTime ? serverTime - m_impl->account.serverOffsetSeconds() : 0; }
int64_t MT4Service::toServerTime(int64_t utc) { return utc ? utc + m_impl->account.serverOffsetSeconds() : 0; }

// Ask the gateway for the broker's timezone directly (name, UTC offset in seconds, and how that
// offset was determined). Unlike serverOffsetSeconds() — which infers the shift from an
// AccountSummary — this is the broker's own authoritative answer, handy for auditing/logging.
ServerTimezoneInfo MT4Service::serverTimezone() {
    pb::ServerTimezoneData d = m_impl->account.serverTimezone();
    ServerTimezoneInfo o;
    o.server = d.server(); o.utcOffsetSeconds = d.utc_offset_seconds(); o.source = (int)d.source();
    o.validatedUtc = d.validated_utc().seconds();
    return o;
}
#pragma endregion

// ======================================================================================
// MT4Service — account
// ======================================================================================
#pragma region Account
// Fetch the raw AccountSummary and project it onto AccountInfo, filling both the raw server time
// and the same instant in UTC (the wire carries both).
AccountInfo MT4Service::accountSummary() {
    pb::AccountSummaryData d = m_impl->account.accountSummary();
    AccountInfo a;
    a.login = d.account_login(); a.currency = d.account_currency(); a.balance = d.account_balance();
    a.equity = d.account_equity(); a.credit = d.account_credit(); a.leverage = d.account_leverage();
    a.margin = d.account_margin(); a.marginFree = d.account_margin_free(); a.marginLevel = d.account_margin_level();
    a.name = d.account_user_name(); a.company = d.account_company_name();
    a.tradeMode = (TradeMode)(int)d.account_trade_mode();
    a.investor = d.is_investor(); a.tradeAllowed = d.account_trade_allowed(); a.tradeExpert = d.account_trade_expert();
    a.serverTime = d.server_time().seconds(); a.serverTimeUtc = d.server_time_utc().seconds();
    a.utcShiftMinutes = d.utc_server_time_shift_minutes();
    a.server = m_impl->server;
    return a;
}
#pragma endregion

// ======================================================================================
// MT4Service — symbols / quotes
// ======================================================================================
#pragma region Symbols / quotes
// Latest bid/ask (plus the session high/low) for one symbol, in one round-trip. The symbol must be
// in Market Watch — call symbolSelect() first for an exotic one the terminal isn't tracking yet.
Quote MT4Service::quote(const std::string& symbol) {
    pb::QuoteRequest req; req.set_symbol(symbol);
    return quoteFrom(m_impl->account.quote(req), m_impl->utcTimes);
}

// Latest quotes for several symbols in a SINGLE round-trip — much cheaper than N quote() calls
// when you refresh a basket. The reply order follows the request order.
std::vector<Quote> MT4Service::quoteMany(const std::vector<std::string>& symbols) {
    pb::QuoteManyRequest req;
    for (const auto& s : symbols) req.add_symbols(s);
    pb::QuoteManyData d = m_impl->account.quoteMany(req);
    std::vector<Quote> out; out.reserve(d.quotes_size());
    for (int i = 0; i < d.quotes_size(); ++i) out.push_back(quoteFrom(d.quotes(i), m_impl->utcTimes));
    return out;
}

// Add (select=true) or remove (false) a symbol from the terminal's Market Watch. A symbol must be
// selected before the terminal will stream quotes or accept orders for it; returns the broker's
// success flag (false e.g. if the broker doesn't offer the symbol at all).
bool MT4Service::symbolSelect(const std::string& symbol, bool select) {
    pb::SymbolSelectRequest req; req.set_symbol(symbol); req.set_select(select);
    return m_impl->account.symbolSelect(req).success();
}

// Every symbol the broker offers (whether or not it's in Market Watch) — just the names. Use it to
// discover instruments; pair with symbolParams() when you need each symbol's full spec.
std::vector<std::string> MT4Service::symbolList() {
    pb::SymbolsData d = m_impl->account.symbols();
    std::vector<std::string> out; out.reserve(d.symbolnameinfos_size());
    for (int i = 0; i < d.symbolnameinfos_size(); ++i) out.push_back(d.symbolnameinfos(i).symbol_name());
    return out;
}

// All params in one call; convert each entry (the filter may return partial matches).
std::vector<SymbolInfo> MT4Service::symbolParamsMany(const std::string& nameFilter) {
    pb::SymbolParamsManyRequest req;
    if (!nameFilter.empty()) req.set_symbol_name(nameFilter);
    pb::SymbolParamsManyData d = m_impl->account.symbolParamsMany(req);
    std::vector<SymbolInfo> out; out.reserve(d.symbol_infos_size());
    for (int i = 0; i < d.symbol_infos_size(); ++i) out.push_back(symbolFrom(d.symbol_infos(i), m_impl->utcTimes));
    return out;
}

// Exact-name lookup built on symbolParamsMany (the filter may return partial matches).
SymbolInfo MT4Service::symbolParams(const std::string& symbol) {
    auto all = symbolParamsMany(symbol);
    for (const auto& s : all) if (s.name == symbol) return s;
    throw ApiError("SYMBOL_NOT_FOUND", "SymbolParamsMany returned no entry named '" + symbol + "' (" +
                   std::to_string(all.size()) + " partial matches)", 0, 0, "", "SymbolParamsMany");
}

// Tick value (what one tick is worth in the ACCOUNT currency), tick size and contract size for
// several symbols at once. These three numbers are what you need to turn a price move into money —
// the basis for position sizing and risk-per-trade math (see the risk helpers in MT4Sugar).
std::vector<TickValueSize> MT4Service::tickValueWithSize(const std::vector<std::string>& symbols) {
    pb::TickValueWithSizeRequest req;
    for (const auto& s : symbols) req.add_symbol_names(s);
    pb::TickValueWithSizeData d = m_impl->account.tickValueWithSize(req);
    std::vector<TickValueSize> out; out.reserve(d.infos_size());
    for (int i = 0; i < d.infos_size(); ++i) {
        const auto& t = d.infos(i);
        out.push_back(TickValueSize{ t.symbolname(), t.tradetickvalue(), t.tradeticksize(), t.tradecontractsize() });
    }
    return out;
}
#pragma endregion

// ======================================================================================
// MT4Service — bars
// ======================================================================================
#pragma region Bars
// Build the request (UTC input goes into the *_utc wire fields), fetch bars, read each back from
// the matching time field, and sort oldest-first.
std::vector<Bar> MT4Service::bars(const std::string& symbol, Timeframe tf, int64_t fromUnix,
                                  int64_t toUnix, int maxBars) {
    pb::QuoteHistoryBarsRequest req;
    req.set_symbol(symbol);
    req.set_timeframe((pb::QUOTE_HISTORY_TIMEFRAME)(int)tf);
    if (m_impl->utcTimes) { req.set_from_time_utc(fromUnix); req.set_to_time_utc(toUnix); }
    else                  { req.set_from_time(fromUnix);     req.set_to_time(toUnix); }
    if (maxBars > 0) req.set_page_size(maxBars);
    pb::QuoteHistoryBarsData d = m_impl->account.bars(req);
    std::vector<Bar> out; out.reserve(d.bars_size());
    for (int i = 0; i < d.bars_size(); ++i) {
        const auto& b = d.bars(i);
        Bar o; o.time = m_impl->utcTimes ? b.time_utc() : b.time();   // plain int64 unix seconds
        o.open = b.open(); o.high = b.high(); o.low = b.low(); o.close = b.close();
        o.tickVolume = b.tick_volume(); o.realVolume = b.real_volume(); o.spread = b.spread();
        out.push_back(o);
    }
    std::sort(out.begin(), out.end(), [](const Bar& a, const Bar& b) { return a.time < b.time; });
    return out;
}

// The newest `count` bars. Unlike MT5, the MT4 gateway REQUIRES a from_time — a zero window is
// rejected (QUOTE_HISTORY_FROM_TIME_REQUIRED) — so derive a window ending now and reaching back
// count*timeframe, padded generously (×3 + a week) to absorb weekends/holidays/gaps, then keep the
// last `count` bars.
std::vector<Bar> MT4Service::lastBars(const std::string& symbol, Timeframe tf, int count) {
    int64_t now  = (int64_t)std::time(nullptr);
    int64_t span = (int64_t)count * timeframeSeconds(tf);
    int64_t from = now - span * 3 - 7 * 86400;
    auto b = bars(symbol, tf, from, now, count);
    if ((int)b.size() > count) b.erase(b.begin(), b.end() - count);
    return b;
}
#pragma endregion

// ======================================================================================
// MT4Service — orders
// ======================================================================================
#pragma region Orders
// Every live order on the account — MT4 keeps market orders AND pending orders in ONE list (there
// is no separate "positions" concept as in MT5), so this single call returns both. Tell them apart
// with Order::isPending(); times come back in UTC when utcTimes is on.
std::vector<Order> MT4Service::openedOrders() {
    pb::OpenedOrdersRequest req;
    pb::OpenedOrdersData d = m_impl->account.openedOrders(req);
    std::vector<Order> out; out.reserve(d.order_infos_size());
    for (int i = 0; i < d.order_infos_size(); ++i) out.push_back(orderFrom(d.order_infos(i), m_impl->utcTimes));
    return out;
}

// Same, but keep only orders tagged with a given magic number — the usual way to isolate the orders
// placed by one Expert Advisor / strategy. Filtering is done client-side on the full snapshot.
std::vector<Order> MT4Service::openedOrders(int32_t magic) {
    auto all = openedOrders();
    std::vector<Order> out;
    for (auto& o : all) if (o.magic == magic) out.push_back(o);
    return out;
}

// Just the TICKETS of the live orders — a much lighter payload than openedOrders() when all you
// need is "which tickets are open right now" (e.g. to diff against a previous snapshot).
std::vector<int32_t> MT4Service::openedOrdersTickets() {
    pb::OpenedOrdersTicketsRequest req;
    pb::OpenedOrdersTicketsData d = m_impl->account.openedOrdersTickets(req);
    std::vector<int32_t> out; out.reserve(d.tickets_size());
    for (int i = 0; i < d.tickets_size(); ++i) out.push_back(d.tickets(i));
    return out;
}

// Closed orders in a window: UTC input goes into the *_utc wire fields; each returned time is read
// from the matching field.
std::vector<HistoryOrder> MT4Service::ordersHistory(int64_t fromUnix, int64_t toUnix) {
    pb::OrdersHistoryRequest req;
    if (m_impl->utcTimes) {
        req.mutable_input_from_utc()->set_seconds(fromUnix);
        req.mutable_input_to_utc()->set_seconds(toUnix);
    } else {
        req.mutable_input_from()->set_seconds(fromUnix);
        req.mutable_input_to()->set_seconds(toUnix);
    }
    pb::OrdersHistoryData d = m_impl->account.ordersHistory(req);
    std::vector<HistoryOrder> out; out.reserve(d.orders_info_size());
    for (int i = 0; i < d.orders_info_size(); ++i) out.push_back(historyOrderFrom(d.orders_info(i), m_impl->utcTimes));
    return out;
}
#pragma endregion

// ======================================================================================
// MT4Service — trading
// ======================================================================================
#pragma region Trading
// Translate the friendly OrderRequest into an OrderSendRequest, send it, and project the result.
// MT4 has no retcode: a rejection throws ApiError from Layer 1, so a returned OrderResult is OK.
OrderResult MT4Service::orderSend(const OrderRequest& q) {
    pb::OrderSendRequest req;
    req.set_symbol(q.symbol);
    req.set_operation_type((pb::OrderSendOperationType)(int)q.type);
    req.set_volume(q.volume);
    if (q.price > 0) req.set_price(q.price);
    req.set_slippage(std::max(0, q.slippage));
    if (q.stopLoss > 0) req.set_stoploss(q.stopLoss);
    if (q.takeProfit > 0) req.set_takeprofit(q.takeProfit);
    if (!q.comment.empty()) req.set_comment(q.comment);
    if (q.magic != 0) req.set_magic_number(q.magic);
    if (q.expiration > 0) {
        if (m_impl->utcTimes) req.mutable_expiration_utc()->set_seconds(q.expiration);
        else                  req.mutable_expiration()->set_seconds(q.expiration);
    }
    pb::OrderSendData d = m_impl->account.orderSend(req);
    OrderResult r; r.ticket = d.ticket(); r.volume = d.volume(); r.price = d.price();
    r.openTime = m_impl->utcTimes ? d.open_time_utc().seconds() : d.open_time().seconds();
    return r;
}

// Change an existing order's SL/TP (and, for a pending order, its trigger price and expiry) by
// ticket. SL/TP are always sent; price/expiration are only applied when > 0, so passing 0 leaves
// them untouched. Returns the terminal's "was it actually modified" flag (false = broker rejected
// the new levels, e.g. too close to market per the symbol's stops level).
bool MT4Service::orderModify(int32_t ticket, double stopLoss, double takeProfit,
                             double price, int64_t expiration) {
    pb::OrderModifyRequest req;
    req.set_order_ticket(ticket);
    req.set_new_stop_loss(stopLoss);
    req.set_new_take_profit(takeProfit);
    if (price > 0) req.set_new_price(price);
    if (expiration > 0) {
        if (m_impl->utcTimes) req.mutable_new_expiration_utc()->set_seconds(expiration);
        else                  req.mutable_new_expiration()->set_seconds(expiration);
    }
    return m_impl->account.orderModify(req).order_was_modified();
}

// Close a market order (lots 0 = whole order) or delete a pending order by ticket. A failure
// throws ApiError from Layer 1.
void MT4Service::orderClose(int32_t ticket, double lots, double price, int slippage) {
    pb::OrderCloseDeleteRequest req;
    req.set_order_ticket(ticket);
    if (lots > 0) req.set_lots(lots);
    if (price > 0) req.set_closing_price(price);
    req.set_slippage(std::max(0, slippage));
    m_impl->account.orderCloseDelete(req);
}

// Close one order AGAINST an opposite one on the same symbol (e.g. a Buy against a Sell). MT4 nets
// the two out in a single operation — often with lower cost/slippage than closing each at market.
// Both tickets must be open market orders on the same symbol in opposite directions.
CloseByResult MT4Service::orderCloseBy(int32_t ticket, int32_t oppositeTicket) {
    pb::OrderCloseByRequest req;
    req.set_ticket_to_close(ticket);
    req.set_opposite_ticket_closing_by(oppositeTicket);
    pb::OrderCloseByData d = m_impl->account.orderCloseBy(req);
    CloseByResult r; r.closePrice = d.close_price(); r.profit = d.profit();
    r.closeTime = m_impl->utcTimes ? d.close_time_utc().seconds() : d.close_time().seconds();
    return r;
}
#pragma endregion

// ======================================================================================
// MT4Service — trade statistics
// ======================================================================================
#pragma region Trade statistics
// Headline performance analytics for the account, computed server-side over its trade history:
// the running P&L summary (open/day/week/month/total), drawdowns, profit factor, Sharpe ratio,
// GHPR, deposits/withdrawals, etc. We surface the most-used subset of StatsData's ~29 fields.
TradeStats MT4Service::tradeStats() {
    pb::TradeStatsRequest req;
    pb::StatsData d = m_impl->account.tradeStats(req);
    TradeStats s;
    const auto& sm = d.summary();
    s.openTrades = sm.open_trades(); s.openProfit = sm.open_profit(); s.dayProfit = sm.day_profit();
    s.weekProfit = sm.week_profit(); s.monthProfit = sm.month_profit(); s.totalProfit = sm.total_profit();
    s.trades = d.trades(); s.pips = d.pips(); s.lots = d.lots(); s.commissions = d.comissions();
    s.profitFactor = d.profit_factor(); s.sharpeRatio = d.sharpe_ratio(); s.standardDeviation = d.standard_deviation();
    s.ghpr = d.ghpr(); s.growthPercent = d.growth_percent(); s.deposits = d.deposits(); s.withdrawals = d.withdrawals();
    s.maxBalanceDrawdownRaw = d.max_balance_drawdown_raw(); s.maxBalanceDrawdownRelative = d.max_balance_drawdown_relative();
    s.maxEquityDrawdownRaw = d.max_equity_drawdown_raw(); s.maxEquityDrawdownRelative = d.max_equity_drawdown_relative();
    s.averageTradeLength = d.average_trade_length();
    return s;
}

// The equity curve as a series of points (balance + equity + running drawdowns + realized/
// unrealized P&L at each step). Feed it straight into a chart, or scan it for the worst drawdown.
std::vector<EquityPoint> MT4Service::equityHistory() {
    pb::EquityHistoryRequest req;
    pb::EquityHistoryData d = m_impl->account.equityHistory(req);
    std::vector<EquityPoint> out; out.reserve(d.points_size());
    for (int i = 0; i < d.points_size(); ++i) {
        const auto& p = d.points(i);
        EquityPoint e; e.time = pick(m_impl->utcTimes, p.time(), p.time_utc());
        e.balance = p.balance(); e.equity = p.equity(); e.realizedPL = p.realized_pl(); e.unrealizedPL = p.unrealized_pl();
        e.cashFlow = p.cash_flow(); e.balanceDrawdownRaw = p.balance_drawdown_raw(); e.equityDrawdownRaw = p.equity_drawdown_raw();
        out.push_back(e);
    }
    return out;
}

// Per-open-order floating-P&L extremes: for each live order, the best (max) and worst (min)
// unrealised profit it has reached, with the timestamps of each. Useful for "how far in the money /
// under water did this trade get" analysis and for trailing-stop / MFE-MAE studies.
std::vector<UnrealizedExtremum> MT4Service::tradeUnrealizedPL() {
    pb::TradeUnrealizedPLRequest req;
    pb::TradeUnrealizedPLData d = m_impl->account.tradeUnrealizedPL(req);
    std::vector<UnrealizedExtremum> out; out.reserve(d.extrema_size());
    for (int i = 0; i < d.extrema_size(); ++i) {
        const auto& x = d.extrema(i);
        UnrealizedExtremum e; e.ticket = x.ticket(); e.symbol = x.symbol();
        e.maxTime = pick(m_impl->utcTimes, x.max_unrealized_time(), x.max_unrealized_time_utc()); e.maxPL = x.max_unrealized_pl();
        e.minTime = pick(m_impl->utcTimes, x.min_unrealized_time(), x.min_unrealized_time_utc()); e.minPL = x.min_unrealized_pl();
        out.push_back(e);
    }
    return out;
}
#pragma endregion

// ======================================================================================
// MT4Service — streaming
// ======================================================================================
#pragma region Streaming
// ServiceStream just owns the low-level AccountStream and forwards control/query calls; the
// pb -> friendly conversion happens in the callback wrapper installed by each subscribe method.
struct ServiceStream::Impl {
    std::unique_ptr<AccountStream> inner;
};

ServiceStream::ServiceStream() : m_impl(new Impl()) {}
ServiceStream::~ServiceStream() = default;
void ServiceStream::stop() { if (m_impl->inner) m_impl->inner->stop(); }
bool ServiceStream::running() const { return m_impl->inner && m_impl->inner->running(); }
uint64_t ServiceStream::count() const { return m_impl->inner ? m_impl->inner->count() : 0; }
std::string ServiceStream::lastError() const { return m_impl->inner ? m_impl->inner->lastError() : ""; }

// Live ticks: convert each raw payload into a friendly Tick (times from the matching wire field).
std::unique_ptr<ServiceStream> MT4Service::subscribeTicks(
    const std::vector<std::string>& symbols, std::function<void(const Tick&)> callback) {
    const bool utc = m_impl->utcTimes;
    pb::OnSymbolTickRequest req;
    for (const auto& s : symbols) req.add_symbol_names(s);
    std::unique_ptr<ServiceStream> s(new ServiceStream());
    auto cb = std::move(callback);
    s->m_impl->inner = m_impl->account.onSymbolTick(req, [cb, utc](const pb::OnSymbolTickData& data) {
        if (!data.has_symbol_tick()) return;
        const auto& t = data.symbol_tick();
        Tick o; o.symbol = t.symbol(); o.bid = t.bid(); o.ask = t.ask(); o.last = t.last(); o.volume = t.volume();
        o.time = utc ? t.time_utc().seconds() : t.time().seconds();
        o.timeMsc = utc ? t.time_msc_utc() : t.time_msc();
        if (cb) cb(o);
    });
    return s;
}

// Trade events: convert the OnTrade payload into a TradeEvent.
std::unique_ptr<ServiceStream> MT4Service::streamTradeUpdates(std::function<void(const TradeEvent&)> callback) {
    const bool utc = m_impl->utcTimes;
    std::unique_ptr<ServiceStream> s(new ServiceStream());
    auto cb = std::move(callback);
    s->m_impl->inner = m_impl->account.onTrade(pb::OnTradeRequest{}, [cb, utc](const pb::OnTradeData& d) {
        TradeEvent e; e.type = (int)d.type(); e.terminalGuid = d.terminal_instance_guid_id();
        if (d.has_account_info()) e.account = eventAccountFrom(d.account_info());
        if (d.has_event_data()) {
            const auto& ev = d.event_data();
            for (int i = 0; i < ev.new_orders_size(); ++i) e.newOrders.push_back(tradeOrderFrom(ev.new_orders(i), utc));
            for (int i = 0; i < ev.removed_orders_size(); ++i) e.removedOrders.push_back(tradeOrderFrom(ev.removed_orders(i), utc));
            for (int i = 0; i < ev.new_history_orders_size(); ++i) e.newHistoryOrders.push_back(tradeOrderFrom(ev.new_history_orders(i), utc));
            for (int i = 0; i < ev.updated_orders_size(); ++i)
                e.updatedOrders.push_back({ tradeOrderFrom(ev.updated_orders(i).previous(), utc),
                                            tradeOrderFrom(ev.updated_orders(i).current(), utc) });
        }
        if (cb) cb(e);
    });
    return s;
}

// Floating-P&L snapshots: convert into an OrderProfitEvent.
std::unique_ptr<ServiceStream> MT4Service::streamOrderProfits(
    std::function<void(const OrderProfitEvent&)> callback, int periodMs) {
    const bool utc = m_impl->utcTimes;
    pb::OnOpenedOrdersProfitRequest req; req.set_timer_period_milliseconds(periodMs);
    std::unique_ptr<ServiceStream> s(new ServiceStream());
    auto cb = std::move(callback);
    s->m_impl->inner = m_impl->account.onOpenedOrdersProfit(req, [cb, utc](const pb::OnOpenedOrdersProfitData& d) {
        OrderProfitEvent e; e.type = (int)d.type(); e.terminalGuid = d.terminal_instance_guid_id();
        if (d.has_account_info()) e.account = eventAccountFrom(d.account_info());
        for (int i = 0; i < d.opened_orders_with_profit_updated_size(); ++i)
            e.orders.push_back(orderProfitFrom(d.opened_orders_with_profit_updated(i), utc));
        if (cb) cb(e);
    });
    return s;
}

// Ticket snapshots: convert into a TicketsSnapshot.
std::unique_ptr<ServiceStream> MT4Service::streamOpenedTickets(
    std::function<void(const TicketsSnapshot&)> callback, int intervalMs) {
    const bool utc = m_impl->utcTimes;
    pb::OnOpenedOrdersTicketsRequest req; req.set_pull_interval_milliseconds(intervalMs);
    std::unique_ptr<ServiceStream> s(new ServiceStream());
    auto cb = std::move(callback);
    s->m_impl->inner = m_impl->account.onOpenedOrdersTickets(req, [cb, utc](const pb::OnOpenedOrdersTicketsData& d) {
        TicketsSnapshot e; e.terminalGuid = d.terminal_instance_guid_id();
        for (int i = 0; i < d.position_tickets_size(); ++i) e.positionTickets.push_back(d.position_tickets(i));
        for (int i = 0; i < d.pending_order_tickets_size(); ++i) e.pendingOrderTickets.push_back(d.pending_order_tickets(i));
        e.serverTime = utc ? d.server_time_utc().seconds() : d.server_time().seconds();
        if (cb) cb(e);
    });
    return s;
}

// Connection-state changes: convert into a ConnectState.
std::unique_ptr<ServiceStream> MT4Service::streamConnectState(std::function<void(const ConnectState&)> callback) {
    std::unique_ptr<ServiceStream> s(new ServiceStream());
    auto cb = std::move(callback);
    s->m_impl->inner = m_impl->account.onConnectState(pb::OnConnectStateRequest{}, [cb](const pb::ConnectStateData& d) {
        ConnectState e; e.id = d.id(); e.state = d.state(); e.isConnected = d.is_connected(); e.isAlive = d.is_alive();
        e.apiIsAlive = d.api_is_alive(); e.server = d.server(); e.account = d.account(); e.errorMessage = d.error_message();
        e.timestamp = d.timestamp().seconds();
        if (cb) cb(e);
    });
    return s;
}
#pragma endregion

} // namespace metarpc
