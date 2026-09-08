
## 🆔 Generate Account ID (`GetId`)

> ⚠️ **Important**: You must generate your deterministic account ID with `GetId` **firstly** before connecting or streaming.

Before establishing a connection, call `GetId` with your trading account number and password to obtain your deterministic terminal GUID:

```bash
curl -X GET "https://mt4.mrpc.pro/GetId?user=YOUR_LOGIN&password=YOUR_PASSWORD" \
     -H "APIKey: YOUR_API_KEY"
```

The resulting `id` must be provided as the `id` header alongside `APIKey` for all subsequent operations.


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

