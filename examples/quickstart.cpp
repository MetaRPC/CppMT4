#include "metarpc/mt4.hpp"
#include <iostream>

int main() {
    metarpc::MT4Client client("mt4.broker.com", 443);

    std::cout << "Connecting to MT4..." << std::endl;
    if (client.connect(100234, "password")) {
        std::cout << "Connected!" << std::endl;

        auto acc = client.getAccountInfo();
        std::cout << "Balance: " << acc.balance << " " << acc.currency << std::endl;

        metarpc::OrderRequest req;
        req.symbol = "EURUSD";
        req.type = metarpc::OrderType::Buy;
        req.lots = 0.1;
        req.comment = "C++ MT4 Algo";

        auto result = client.orderSend(req);
        std::cout << "Order placed! Ticket #" << result.ticket << std::endl;
    }

    return 0;
}
