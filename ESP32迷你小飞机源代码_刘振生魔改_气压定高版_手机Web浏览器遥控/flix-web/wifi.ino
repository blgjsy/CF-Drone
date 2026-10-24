// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi support - 完整修复版

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

#define WIFI_SSID "FlixDrone"
#define WIFI_PASSWORD "12345678"
#define WIFI_UDP_PORT 14550
#define WIFI_UDP_REMOTE_PORT 14550
#define WIFI_UDP_REMOTE_ADDR "255.255.255.255"

// WiFi配置优化参数
#define WIFI_CHANNEL 1           // 固定信道，减少干扰
#define WIFI_MAX_CONN 4          // 最大连接数
#define WIFI_BEACON_INTERVAL 100 // 信标间隔(ms)，默认100

WiFiUDP udp;

// 新增：客户端管理变量
IPAddress lastClientIP;
uint16_t lastClientPort = 0;
bool clientActive = false;
unsigned long lastClientSeen = 0;
#define CLIENT_TIMEOUT_MS 3000   // 客户端3秒超时

void setupWiFi() {
    print("Setup Wi-Fi for MAVLink communication\n");
    
    // 配置WiFi为AP模式
    WiFi.mode(WIFI_AP);
    
    // 优化WiFi参数
    WiFi.setSleep(false);  // 禁用睡眠模式
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // 设置发射功率
    
    // 配置AP参数
    if (WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1), 
        IPAddress(192, 168, 4, 1), 
        IPAddress(255, 255, 255, 0))) {
        print("AP配置成功\n");
    } else {
        print("AP配置失败\n");
    }
    
    // 启动AP
    if (WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, false, WIFI_MAX_CONN)) {
        print("WiFi AP started\n");
        print("  SSID: %s\n", WIFI_SSID);
        print("  IP: %s\n", WiFi.softAPIP().toString().c_str());
        print("  Channel: %d\n", WIFI_CHANNEL);
        print("  Max clients: %d\n", WIFI_MAX_CONN);
        
        // 启动mDNS
        if (MDNS.begin("flix")) {
            MDNS.addService("http", "tcp", 80);
            print("mDNS: http://flix.local\n");
        } else {
            print("mDNS failed\n");
        }
    } else {
        print("Failed to start WiFi AP - MAVLink可能不可用\n");
    }
    
    // 启动UDP用于MAVLink
    if (udp.begin(WIFI_UDP_PORT)) {
        print("UDP started on port %d for MAVLink\n", WIFI_UDP_PORT);
    } else {
        print("Failed to start UDP - MAVLink不可用\n");
    }
    
    // 延迟确保WiFi稳定
    delay(1000);
    
    // 验证网络状态
    print("WiFi AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    print("WiFi AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
    print("WiFi clients: %d\n", WiFi.softAPgetStationNum());
    print("WiFi setup complete\n");
}

void sendWiFi(const uint8_t *buf, int len) {
    if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && WiFi.status() != WL_CONNECTED) return;
    
    // 新增：检查客户端超时
    unsigned long now = millis();
    if (clientActive && (now - lastClientSeen > CLIENT_TIMEOUT_MS)) {
        clientActive = false;
        print("Client timeout, reverting to broadcast\n");
    }
    
    // 修改：优先发送给已知客户端，否则广播
    if (clientActive) {
        // 发送给已知客户端
        udp.beginPacket(lastClientIP, lastClientPort);
        udp.write(buf, len);
        udp.endPacket();
        
        // 调试：每50个包输出一次
        static int sendCount = 0;
        if (++sendCount % 50 == 0) {
            print("WiFi sent %d bytes to %s:%d\n", 
                  len, lastClientIP.toString().c_str(), lastClientPort);
        }
    } else {
        // 发送到广播地址
        udp.beginPacket(WIFI_UDP_REMOTE_ADDR, WIFI_UDP_REMOTE_PORT);
        udp.write(buf, len);
        udp.endPacket();
        
        // 调试：每100个包输出一次
        static int broadcastCount = 0;
        if (++broadcastCount % 100 == 0) {
            print("WiFi broadcast %d bytes\n", len);
        }
    }
}

int receiveWiFi(uint8_t *buf, int len) {
    // 检查是否有数据包
    int packetSize = udp.parsePacket();
    if (packetSize) {
        // 新增：获取并记录客户端信息
        IPAddress remoteIP = udp.remoteIP();
        uint16_t remotePort = udp.remotePort();
        
        // 更新客户端信息
        if (!clientActive || remoteIP != lastClientIP || remotePort != lastClientPort) {
            lastClientIP = remoteIP;
            lastClientPort = remotePort;
            print("New MAVLink client: %s:%d\n", 
                  remoteIP.toString().c_str(), remotePort);
        }
        
        clientActive = true;
        lastClientSeen = millis();
        
        // 调试：每20个包输出一次
        static int receiveCount = 0;
        if (++receiveCount % 20 == 0) {
            print("WiFi received %d bytes from %s:%d\n", 
                  packetSize, remoteIP.toString().c_str(), remotePort);
        }
        
        return udp.read(buf, len);
    }
    return 0;
}

// 获取WiFi状态
void printWiFiStatus() {
    static unsigned long lastStatusTime = 0;
    unsigned long now = millis();
    
    if (now - lastStatusTime > 10000) { // 10秒一次
        print("WiFi Status:\n");
        print("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        print("  Clients: %d\n", WiFi.softAPgetStationNum());
        print("  MAVLink UDP: %s\n", clientActive ? "Connected" : "No client");
        if (clientActive) {
            print("  Client IP: %s:%d\n", 
                  lastClientIP.toString().c_str(), lastClientPort);
            print("  Last seen: %lu ms ago\n", now - lastClientSeen);
        }
        lastStatusTime = now;
    }
}

#endif