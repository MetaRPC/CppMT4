#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace metarpc {

struct AccountInfo {
    int login;
    std::string currency;
    double balance;
    double equity;
    double margin;
    double freeMargin;
    int leverage;
    std::string name;
    std::string server;
};

struct Quote {
    std::string symbol;
    double bid;
    double ask;
    int64_t timestamp;
};

enum class OrderType {
    Buy = 0,
    Sell = 1,
    BuyLimit = 2,
    SellLimit = 3,
    BuyStop = 4,
    SellStop = 5
};

struct OrderRequest {
    std::string symbol;
    OrderType type;
    double lots;
    double price{0.0};
    double stopLoss{0.0};
    double takeProfit{0.0};
    int slippage{30};
    std::string comment;
};

struct OrderResult {
    int ticket;
    int errorCode;
    double price;
    double lots;
    std::string message;
};

class MT4Client {
public:
    MT4Client(const std::string& host, int port = 443);
    ~MT4Client();

    bool connect(int login, const std::string& password);
    void disconnect();
    bool isConnected() const;

    AccountInfo getAccountInfo();
    void subscribeQuotes(const std::vector<std::string>& symbols, std::function<void(const Quote&)> callback);
    OrderResult orderSend(const OrderRequest& request);
    bool orderModify(int ticket, double stopLoss, double takeProfit);
    bool orderClose(int ticket, double lots);

private:
    std::string m_host;
    int m_port;
    bool m_connected{false};
};

} // namespace metarpc
