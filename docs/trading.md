# Trading & Orders

Opening, modifying, and closing positions.

```cpp
// Market Buy
metarpc::OrderRequest req;
req.symbol = "EURUSD";
req.type = metarpc::OrderType::Buy;
req.lots = 0.1;
req.stopLoss = 1.0800;
req.takeProfit = 1.0950;

metarpc::OrderResult res = client.orderSend(req);
int ticket = res.ticket;

// Modify SL/TP
client.orderModify(ticket, 1.0820, 1.0960);

// Close Order
client.orderClose(ticket, 0.1);
```
