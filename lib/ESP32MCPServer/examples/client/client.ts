class MCPClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.requestId = 1;
        this.callbacks = new Map();
        this.subscriptions = new Map();
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 1000;
        this.heartbeatInterval = null;
    }
    
    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);
            
            this.ws.onopen = () => {
                console.log('Connected to MCP server');
                this.reconnectAttempts = 0;
                this.startHeartbeat();
                this.initialize().then(resolve).catch(reject);
            };
            
            this.ws.onmessage = (event) => {
                try {
                    const message = JSON.parse(event.data);
                    this.handleMessage(message);
                } catch (error) {
                    console.error('Error parsing message:', error);
                }
            };
            
            this.ws.onclose = () => {
                console.log('Connection closed');
                this.stopHeartbeat();
                this.handleReconnect();
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                reject(error);
            };
        });
    }
    
    disconnect() {
        if (this.ws) {
            this.ws.close();
            this.stopHeartbeat();
        }
    }
    
    async initialize() {
        const response = await this.sendRequest('initialize', {});
        console.log('Server initialized:', response);
        return response;
    }
    
    async listResources() {
        const response = await this.sendRequest('resources/list', {});
        return response.result.data.resources;
    }
    
    async readResource(uri) {
        const response = await this.sendRequest('resources/read', { uri });
        return response.result.data;
    }
    
    async subscribe(uri, callback) {
        const response = await this.sendRequest('resources/subscribe', { uri });
        if (response.result.success) {
            this.subscriptions.set(uri, callback);
        }
        return response.result.success;
    }
    
    async unsubscribe(uri) {
        const response = await this.sendRequest('resources/unsubscribe', { uri });
        if (response.result.success) {
            this.subscriptions.delete(uri);
        }
        return response.result.success;
    }
    
    sendRequest(method, params) {
        return new Promise((resolve, reject) => {
            if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
                reject(new Error('WebSocket is not connected'));
                return;
            }
            
            const id = this.requestId++;
            const request = {
                jsonrpc: '2.0',
                method: method,
                params: params,
                id: id
            };
            
            this.callbacks.set(id, { resolve, reject });
            this.ws.send(JSON.stringify(request));
            
            // Set timeout for request
            setTimeout(() => {
                if (this.callbacks.has(id)) {
                    this.callbacks.delete(id);
                    reject(new Error('Request timeout'));
                }
            }, 5000);
        });
    }
    
    handleMessage(message) {
        // Handle notifications
        if (message.method && message.method.startsWith('notifications/')) {
            this.handleNotification(message);
            return;
        }
        
        // Handle responses
        if (message.id && this.callbacks.has(message.id)) {
            const { resolve, reject } = this.callbacks.get(message.id);
            this.callbacks.delete(message.id);
            
            if (message.error) {
                reject(new Error(message.error.message));
            } else {
                resolve(message);
            }
        }
    }
    
    handleNotification(message) {
        if (message.method === 'notifications/resources/updated') {
            const uri = message.params.uri;
            const callback = this.subscriptions.get(uri);
            if (callback) {
                this.readResource(uri).then(callback);
            }
        }
    }
    
    handleReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error('Max reconnection attempts reached');
            return;
        }
        
        this.reconnectAttempts++;
        const delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1);
        
        console.log(`Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})`);
        setTimeout(() => this.connect(), delay);
    }
    
    startHeartbeat() {
        this.heartbeatInterval = setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send(JSON.stringify({ type: 'ping' }));
            }
        }, 30000);
    }
    
    stopHeartbeat() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }
    }
}

// Example usage:
async function connectToMCP() {
    const client = new MCPClient('ws://your-esp32-ip:9000');
    
    try {
        // Connect and initialize
        await client.connect();
        
        // List available resources
        const resources = await client.listResources();
        console.log('Available resources:', resources);
        
        // Subscribe to system info updates
        await client.subscribe('system://info', (data) => {
            console.log('System info updated:', data);
        });
        
        // Read current network status
        const networkStatus = await client.readResource('system://network');
        console.log('Network status:', networkStatus);
        
    } catch (error) {
        console.error('Error:', error);
    }
}