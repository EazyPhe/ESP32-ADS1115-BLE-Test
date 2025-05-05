#pragma once

#include <string>
#include <vector>
#include <queue>
#include <functional>

// Mock WebSocket client for testing
class MockWebSocketClient {
public:
    virtual ~MockWebSocketClient() = default;
    
    bool isConnected() const { return connected; }
    void connect() { connected = true; }
    void disconnect() { connected = false; }
    
    virtual void onMessage(const std::string& message) = 0;
    
    void sendMessage(const std::string& message) {
        outgoing_messages.push(message);
    }
    
    std::string receiveMessage() {
        if (incoming_messages.empty()) {
            return "";
        }
        std::string msg = incoming_messages.front();
        incoming_messages.pop();
        return msg;
    }
    
    void processOutgoingMessages() {
        while (!outgoing_messages.empty()) {
            onMessage(outgoing_messages.front());
            outgoing_messages.pop();
        }
    }
    
    void queueIncomingMessage(const std::string& message) {
        incoming_messages.push(message);
    }

private:
    bool connected = false;
    std::queue<std::string> incoming_messages;
    std::queue<std::string> outgoing_messages;
};

// Mock WebSocket server for testing
class MockWebSocketServer {
public:
    void begin() { running = true; }
    void stop() { running = false; }
    bool isRunning() const { return running; }
    
    void addClient(MockWebSocketClient* client) {
        clients.push_back(client);
    }
    
    void removeClient(MockWebSocketClient* client) {
        auto it = std::find(clients.begin(), clients.end(), client);
        if (it != clients.end()) {
            clients.erase(it);
        }
    }
    
    void broadcastMessage(const std::string& message) {
        for (auto client : clients) {
            if (client->isConnected()) {
                client->queueIncomingMessage(message);
            }
        }
    }
    
    void processMessages() {
        for (auto client : clients) {
            if (client->isConnected()) {
                client->processOutgoingMessages();
            }
        }
    }

private:
    bool running = false;
    std::vector<MockWebSocketClient*> clients;
};

// Mock WebSocket event types
enum MockWebSocketEventType {
    WStype_DISCONNECTED,
    WStype_CONNECTED,
    WStype_TEXT,
    WStype_ERROR
};

using MockWebSocketEventCallback = std::function<void(uint8_t, MockWebSocketEventType, uint8_t*, size_t)>;