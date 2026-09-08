# API Reference

### `metarpc::MT4Client`
- `bool connect(int login, const std::string& password)`
- `void disconnect()`
- `bool isConnected() const`
- `AccountInfo getAccountInfo()`
- `void subscribeQuotes(const std::vector<std::string>& symbols, std::function<void(const Quote&)> callback)`
- `OrderResult orderSend(const OrderRequest& request)`
- `bool orderModify(int ticket, double stopLoss, double takeProfit)`
- `bool orderClose(int ticket, double lots)`
