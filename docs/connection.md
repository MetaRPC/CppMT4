# Connection & Authentication

Managing connections with `metarpc::MT4Client`.

```cpp
#include <metarpc/mt4.hpp>

metarpc::MT4Client client("mt4.mrpc.pro", 443);

if (!client.connect(100234, "password")) {
    std::cerr << "Failed to connect to MT4 server!" << std::endl;
    return -1;
}

// Check status
if (client.isConnected()) {
    std::cout << "Ready for trading operations." << std::endl;
}

// Disconnect
client.disconnect();
```


> **Authentication Note**: Connecting to `mt4.mrpc.pro:443` requires a valid MetaRPC API key. Register for free at [https://mrpc.pro/signup](https://mrpc.pro/signup) and generate your token in [https://mrpc.pro/my](https://mrpc.pro/my).

