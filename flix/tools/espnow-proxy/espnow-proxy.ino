// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// ESP-NOW连接代理

#include <vector>
#include <WiFi.h>
#include <ESP32_NOW_Serial.h>
#include <MacAddress.h>
#include <MAVLink.h>
#include <Preferences.h>
#include "../../flix/util.h"

const int CHANNEL = 6;
char key[ESP_NOW_KEY_LEN + 1] = {0}; // 带结尾空字符

Preferences storage;

std::vector<ESPNOWSerial *> peers;

void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
	if (len != 4 || memcmp(data, "flix", 4) != 0) return; // 检查是否为发现消息

	Serial.printf("新对等设备：" MACSTR "\n", MAC2STR(info->src_addr));
	ESPNOWSerial *link = new ESPNOWSerial(info->src_addr, CHANNEL, WIFI_IF_AP);
	link->begin();
	link->setKey((const uint8_t *)key);
	peers.push_back(link);
}

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_AP);
	WiFi.setSleep(false);
	WiFi.setChannel(CHANNEL);

	ESP_NOW.onNewPeer(onNewPeer, NULL);
	ESP_NOW.begin();

	storage.begin("espnow-proxy");
	if (!storage.isKey("key")) {
		generateRandomKey();
		storage.putString("key", key);
	}
	strcpy(key, storage.getString("key").c_str());

	// 发现第一个对等设备
	while (peers.empty()) {
		Serial.printf("espnow %s %s\n", WiFi.softAPmacAddress().c_str(), key); // 该命令可直接复制到无人机控制台使用
		delay(500);
	}
}

void generateRandomKey() {
	const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*-_+=";
	for (int i = 0; i < ESP_NOW_KEY_LEN; i++) {
		key[i] = chars[random(0, strlen(chars))];
	}
}

void loop() {
	uint8_t buf[5000];

	// 从串口发送到ESP-NOW
	while (Serial.available() > 0) {
		int b = Serial.read();
		if (b < 0) {
			break;
		}

		mavlink_message_t msg;
		mavlink_status_t status;
		if (mavlink_parse_char(MAVLINK_COMM_0, (uint8_t)b, &msg, &status)) {
			int len = mavlink_msg_to_send_buffer(buf, &msg);
			for (ESPNOWSerial *link : peers) {
				link->write(buf, len);
			}
		}
	}

	// 从ESP-NOW发送到串口
	for (ESPNOWSerial *link : peers) {
		int len = link->read(buf, sizeof(buf));
		if (len > 0) {
			Serial.write(buf, len);
		}
	}
}
