#include "metarpc/mt4.hpp"
#include <sstream>
#include <iomanip>

namespace metarpc {

MT4Client::MT4Client(const std::string& host, int port, const std::string& apiKey)
    : m_host(host), m_port(port), m_apiKey(apiKey), m_connected(false) {}

MT4Client::~MT4Client() {
    disconnect();
}

std::string MT4Client::getId(int login, const std::string& password) {
    if (m_id.empty()) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(8) << (login & 0xFFFFFFFF) << "-"
           << std::setw(4) << (password.length() & 0xFFFF) << "-4000-8000-"
           << std::setw(12) << (login & 0xFFFFFFFFFFFFLL);
        m_id = ss.str();
    }
    return m_id;
}

bool MT4Client::connect(int login, const std::string& password) {
    if (m_id.empty()) {
        getId(login, password);
    }
    m_connected = true;
    return true;
}

void MT4Client::disconnect() {
    m_connected = false;
}

bool MT4Client::isConnected() const {
    return m_connected;
}

AccountInfo MT4Client::getAccountInfo() {
    AccountInfo info;
    info.login = 100234;
    info.currency = "USD";
    info.balance = 10000.0;
    info.equity = 10000.0;
    info.margin = 0.0;
    info.freeMargin = 10000.0;
    info.leverage = 100;
    info.name = "Demo Account";
    info.server = m_host;
    return info;
}

void MT4Client::subscribeQuotes(const std::vector<std::string>& symbols, std::function<void(const Quote&)> callback) {
    // Quote streaming hook
}

OrderResult MT4Client::orderSend(const OrderRequest& request) {
    OrderResult res;
    res.ticket = 8765432;
    res.errorCode = 0;
    res.price = request.price > 0 ? request.price : 1.0850;
    res.lots = request.lots;
    res.message = "Success";
    return res;
}

bool MT4Client::orderModify(int ticket, double stopLoss, double takeProfit) {
    return true;
}

bool MT4Client::orderClose(int ticket, double lots) {
    return true;
}

} // namespace metarpc
