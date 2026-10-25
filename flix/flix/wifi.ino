// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi 和 ESP-NOW 通信

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <MacAddress.h>
#include <ESP32_NOW_Serial.h>
#include <Preferences.h>
#include "util.h"

extern Preferences storage; // 使用主参数存储

const int W_DISABLED = 0, W_AP = 1, W_STA = 2, W_ESPNOW = 3;
int wifiMode = W_AP;

int wifiLongRange = 0;
int wifiBroadcast = 0; // 0 - 连接前一直广播，1 - 始终广播
int udpLocalPort = 14550;
int udpRemotePort = 14550;
IPAddress udpRemoteIP = "255.255.255.255";
WiFiUDP udp;

ESPNOWSerial espnow(NULL, 0, WIFI_IF_AP);
ESPNOWSerial espnowBroadcast(ESP_NOW.BROADCAST_ADDR, 0, WIFI_IF_AP);
int espnowChannel = 6;

void setupWiFi() {
	print("正在初始化Wi-Fi\n");
	WiFi.enableLongRange(wifiLongRange);

	if (wifiMode == W_AP) {
		WiFi.softAP(storage.getString("WIFI_AP_SSID", "flix").c_str(), storage.getString("WIFI_AP_PASS", "flixwifi").c_str());
		udp.begin(udpLocalPort);
	}

	if (wifiMode == W_STA) {
		WiFi.begin(storage.getString("WIFI_STA_SSID", "").c_str(), storage.getString("WIFI_STA_PASS", "").c_str());
		udp.begin(udpLocalPort);
	}

	if (wifiMode == W_ESPNOW) {
		WiFi.mode(WIFI_AP);
		WiFi.setChannel(espnowChannel);
		espnow.addr(MacAddress(storage.getString("ESPNOW_PEER_MAC", "FF:FF:FF:FF:FF:FF").c_str()));
		String key = storage.getString("ESPNOW_PEER_KEY", "");
		espnow.setKey(key.isEmpty() ? nullptr : (const uint8_t *)key.c_str());
		espnow.begin();
		espnowBroadcast.begin();
	}

	WiFi.setSleep(false); // 禁用省电模式
}

void sendWiFi(const uint8_t *buf, int len) {
	if (espnow) {
		espnow.write(buf, len);

		static Rate discovery(2);
		if (espnow.isEncrypted() && discovery) espnowBroadcast.write((const uint8_t *)"flix", 4); // 广播消息以帮助发现此设备
		return;
	}

	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return;

	bool broadcast = wifiBroadcast || !(t - mavlinkTime < 5); // 失去连接时广播
	udp.beginPacket(broadcast ? IPAddress(255, 255, 255, 255) : udpRemoteIP, udpRemotePort);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	if (espnow) {
		return espnow.read(buf, len);
	}

	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return 0;

	udp.parsePacket();
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP();
	return udp.read(buf, len);
}

void printWiFiInfo() {
	if (espnow) {
		print("模式：ESP-NOW\n");
		print("ESP-NOW版本：%d\n", ESP_NOW.getVersion());
		print("最大数据包大小：%d\n", ESP_NOW.getMaxDataLen());
		print("MAC地址：%s\n", WiFi.softAPmacAddress().c_str());
		print("对等设备MAC：%s\n", MacAddress(espnow.addr()).toString().c_str());
		print("加密：%d\n", espnow.isEncrypted());
		print("信道：%d\n", espnow.getChannel());
		print("丢失的数据包：%d\n", espnow.lost);
	} else if (WiFi.getMode() == WIFI_MODE_AP) {
		print("模式：接入点 (AP)\n");
		print("MAC地址：%s\n", WiFi.softAPmacAddress().c_str());
		print("SSID：%s\n", WiFi.softAPSSID().c_str());
		print("密码：***\n");
		print("信道：%d\n", WiFi.channel());
		print("客户端数：%d\n", WiFi.softAPgetStationNum());
		print("IP地址：%s\n", WiFi.softAPIP().toString().c_str());
		print("远端IP：%s\n", udpRemoteIP.toString().c_str());
	} else if (WiFi.getMode() == WIFI_MODE_STA) {
		print("模式：客户端 (STA)\n");
		print("已连接：%d\n", WiFi.isConnected());
		print("MAC地址：%s\n", WiFi.macAddress().c_str());
		print("SSID：%s\n", WiFi.SSID().c_str());
		print("密码：***\n");
		print("信道：%d\n", WiFi.channel());
		print("信号强度：%d dBm\n", WiFi.RSSI());
		print("IP地址：%s\n", WiFi.localIP().toString().c_str());
		print("远端IP：%s\n", udpRemoteIP.toString().c_str());
	} else {
		print("模式：已禁用\n");
	}
	print("MAVLink已连接：%d\n", valid(mavlinkTime));
}

void configWiFi(int mode, const char *first, const char *second) {
	MacAddress mac;
	if (mode == W_AP && strlen(first) > 0 && strlen(second) >= 8) {
		storage.putString("WIFI_AP_SSID", first);
		storage.putString("WIFI_AP_PASS", second);
	} else if (mode == W_STA && strlen(first) > 0 && strlen(second) >= 8) {
		storage.putString("WIFI_STA_SSID", first);
		storage.putString("WIFI_STA_PASS", second);
	} else if (mode == W_ESPNOW && mac.fromString(first)) {
		storage.putString("ESPNOW_PEER_MAC", first);
		storage.putString("ESPNOW_PEER_KEY", strlen(second) == ESP_NOW_KEY_LEN ? second : "");
	} else {
		print("无效配置\n");
		return;
	}
	print("✓ 重启以应用新设置\n");
}

void setWiFiMode(const String& mode) {
	if (mode == "ap") {
		wifiMode = W_AP;
	} else if (mode == "sta") {
		wifiMode = W_STA;
	} else if (mode == "espnow") {
		wifiMode = W_ESPNOW;
	} else if (mode == "off") {
		wifiMode = W_DISABLED;
	} else {
		print("无效的Wi-Fi模式\n");
		return;
	}
	static const char *modes[] = {"已禁用", "接入点 (AP)", "客户端 (STA)", "ESP-NOW"};
	print("✓ Wi-Fi模式已设置为 %s，重启后生效\n", modes[wifiMode]);
}
