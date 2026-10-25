// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 仿真的sendWiFi和receiveWiFi实现

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/poll.h>
#include <gazebo/gazebo.hh>

// 模拟桩(mock)
int wifiMode = 1;
int wifiLongRange = 0;
int wifiBroadcast = 0;
int espnowChannel = 6;
const int W_DISABLED = 0, W_AP = 1, W_STA = 2, W_ESPNOW = 3;

int udpLocalPort = 14580;
int udpRemotePort = 14550;
const char *udpRemoteIP = "255.255.255.255";

int wifiSocket;

void setupWiFi() {
	wifiSocket = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in addr; // 本地地址
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(udpLocalPort);
	if (bind(wifiSocket, (sockaddr *)&addr, sizeof(addr))) {
		gzerr << "无法在端口 " << udpLocalPort << " 上绑定WiFi UDP套接字" << std::endl;
		return;
	}
	int broadcast = 1;
	setsockopt(wifiSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)); // 启用广播
	gzmsg << "WiFi UDP套接字已在端口 " << udpLocalPort << " 初始化（远端端口 " << udpRemotePort << "）" << std::endl;
}

void sendWiFi(const uint8_t *buf, int len) {
	if (wifiSocket == 0) setupWiFi();
	sockaddr_in addr; // 远端地址
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(udpRemoteIP);
	addr.sin_port = htons(udpRemotePort);
	sendto(wifiSocket, buf, len, 0, (sockaddr *)&addr, sizeof(addr));
}

int receiveWiFi(uint8_t *buf, int len) {
	struct pollfd pfd = { .fd = wifiSocket, .events = POLLIN };
	if (poll(&pfd, 1, 0) <= 0) return 0; // 检查是否有数据可读
	return recv(wifiSocket, buf, len, 0);
}
