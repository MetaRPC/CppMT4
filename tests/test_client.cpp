#include "metarpc/mt4.hpp"
#include <cassert>
#include <iostream>

void testClientLifecycle() {
    metarpc::MT4Client client("mt4.mrpc.pro", 443);
    assert(!client.isConnected());

    bool connected = client.connect(1001, "demo_pass");
    assert(connected);
    assert(client.isConnected());

    metarpc::AccountInfo acc = client.getAccountInfo();
    assert(acc.login == 100234);
    assert(acc.currency == "USD");
    assert(acc.balance == 10000.0);
    assert(acc.server == "mt4.mrpc.pro");

    metarpc::OrderRequest req;
    req.symbol = "EURUSD";
    req.type = metarpc::OrderType::Buy;
    req.lots = 0.1;
    req.price = 1.0850;
    req.comment = "Test order";

    metarpc::OrderResult res = client.orderSend(req);
    assert(res.errorCode == 0);
    assert(res.lots == 0.1);
    assert(res.price == 1.0850);
    assert(res.ticket > 0);

    assert(client.orderModify(res.ticket, 1.0800, 1.0900));
    assert(client.orderClose(res.ticket, 0.1));

    client.disconnect();
    assert(!client.isConnected());

    std::cout << "All CppMT4 client tests passed successfully!" << std::endl;
}

int main() {
    testClientLifecycle();
    return 0;
}