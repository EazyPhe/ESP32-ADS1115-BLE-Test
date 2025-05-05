#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <ArduinoJson.h>
#include "MCPTypes.h"
#include <unordered_map>
#include <string>
#include <functional>

namespace mcp {

struct Implementation {
    std::string name;
    std::string version;
};

struct ServerCapabilities {
    bool supportsSubscriptions;
    bool supportsResources;
};

class MCPServer {
public:
    MCPServer(uint16_t port = 9000);

    void begin(bool isConnected);
    void handleClient();
    void handleInitialize(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleResourcesList(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleResourceRead(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleSubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleUnsubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void unregisterResource(const std::string &uri);
    void sendResponse(uint8_t clientId, const RequestId &id, const MCPResponse &response);
    void sendError(uint8_t clientId, const RequestId &id, int code, const std::string &message);
    void broadcastResourceUpdate(const std::string &uri);

private:
    uint16_t port_;
    Implementation serverInfo{"esp32-mcp-server", "1.0.0"};
    ServerCapabilities capabilities{true, true};

    MCPRequest parseRequest(const std::string &json);
    std::string serializeResponse(const RequestId &id, const MCPResponse &response);
};

} // namespace mcp

#endif // MCP_SERVER_H
