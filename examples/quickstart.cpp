#include "metarpc/mt4.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    const char* envApiKey = std::getenv("MRPC_API_KEY");
    std::string apiKey = (argc > 1) ? argv[1] : (envApiKey ? envApiKey : "TRIAL");

    metarpc::MT4Client client("mt4.mrpc.pro", 443, apiKey);

    int login = 176136103;
    std::string password = "nisl4vs";

    try {
        std::cout << "Connecting to MetaTrader 4 (mt4.mrpc.pro:443)..." << std::endl;
        if (client.connect(login, password)) {
            std::cout << "Connected successfully to MT4! Account ID: " << client.getId() << std::endl;

            std::cout << "\nStep 1: Querying Account Balance..." << std::endl;
            auto acc = client.getAccountInfo();
            std::cout << "Account: " << acc.login << " (" << acc.name << ")" << std::endl;
            std::cout << "Balance: " << acc.balance << " " << acc.currency << std::endl;

            std::cout << "\nStep 2: Executing Market Order..." << std::endl;
            metarpc::OrderRequest req;
            req.symbol = "EURUSD";
            req.type = metarpc::OrderType::Buy;
            req.lots = 0.1;
            req.comment = "Cpp MT4 Bot";

            auto res = client.orderSend(req);
            std::cout << "Order placed! Ticket: #" << res.ticket << " (Code: " << res.errorCode << ")" << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }

    client.disconnect();
    std::cout << "\nDisconnected successfully." << std::endl;
    return 0;
}
