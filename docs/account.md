# Account Information

```cpp
metarpc::AccountInfo acc = client.getAccountInfo();
std::cout << "Login:    " << acc.login << std::endl;
std::cout << "Balance:  " << acc.balance << std::endl;
std::cout << "Equity:   " << acc.equity << std::endl;
std::cout << "Leverage: 1:" << acc.leverage << std::endl;
```
