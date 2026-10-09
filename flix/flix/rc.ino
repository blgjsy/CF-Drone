// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 遥控接收机相关操作

#include <SBUS.h>
#include "util.h"

SBUS rc(Serial1);
int rcRxPin = -1; // -1 表示禁用

uint16_t channels[16]; // 原始遥控通道
int channelZero[16]; // 校准零值
int channelMax[16]; // 校准最大值

float controlRoll, controlPitch, controlYaw, controlThrottle; // 飞手输入，范围 [-1, 1]
float controlMode = NAN;
float controlTime = NAN; // 上次控制更新的时间

int rollChannel = -1, pitchChannel = -1, throttleChannel = -1, yawChannel = -1, modeChannel = -1; // 通道映射

void setupRC() {
	if (rcRxPin < 0) return;
	print("正在初始化遥控器\n");
	rc.begin(rcRxPin);
}

bool readRC() {
	if (rcRxPin < 0) return false;
	if (!rc.read()) return false;

	rc.getChannels(channels);
	normalizeRC();
	controlTime = t;
	return true;
}

void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
	// 更新控制值
	controlRoll = rollChannel < 0 ? 0 : controls[rollChannel];
	controlPitch = pitchChannel < 0 ? 0 : controls[pitchChannel];
	controlYaw = yawChannel < 0 ? 0 : controls[yawChannel];
	controlThrottle = throttleChannel < 0 ? 0 : controls[throttleChannel];
	controlMode = modeChannel < 0 ? NAN : controls[modeChannel]; // 模式控制未映射时无效
}

void calibrateRC() {
	if (rcRxPin < 0) {
		print("RC_RX_PIN = %d，请设置遥控接收引脚！\n", rcRxPin);
		return;
	}

	uint16_t zero[16]; // 用于零位
	uint16_t center[16]; // 用于中位
	uint16_t _[16]; // 用于未使用数据
	print("1/8 正在校准遥控器：将所有开关拨到默认位置 [3秒]\n");
	pause(3);
	calibrateRCChannel(NULL, _, zero, "2/8 拨动摇杆 [3秒]\n...     ...\n...     .o.\n.o.     ...\n");
	calibrateRCChannel(&throttleChannel, zero, _, "3/8 拨动摇杆 [3秒]\n.o.     ...\n...     .o.\n...     ...\n");
	calibrateRCChannel(NULL, _, center, "4/8 拨动摇杆 [3秒]\n...     ...\n.o.     .o.\n...     ...\n");
	calibrateRCChannel(&yawChannel, center, _, "5/8 拨动摇杆 [3秒]\n...     ...\n..o     .o.\n...     ...\n");
	calibrateRCChannel(&pitchChannel, zero, _, "6/8 拨动摇杆 [3秒]\n...     .o.\n...     ...\n.o.     ...\n");
	calibrateRCChannel(&rollChannel, zero, _, "7/8 拨动摇杆 [3秒]\n...     ...\n...     ..o\n.o.     ...\n");
	calibrateRCChannel(&modeChannel, zero, _, "8/8 将模式开关拨到最大 [3秒]\n");
	printRCCalibration();
}

void calibrateRCChannel(int *channel, uint16_t in[16], uint16_t out[16], const char *str) {
	print("%s", str);
	pause(3);
	for (int i = 0; i < 30; i++) readRC(); // 最多尝试更新30次
	memcpy(out, channels, sizeof(channels));

	if (channel == NULL) return; // 没有要校准的通道

	// 找出in和out之间变化最大的通道
	int ch = -1, diff = 0;
	for (int i = 0; i < 16; i++) {
		if (abs(out[i] - in[i]) > diff) {
			ch = i;
			diff = abs(out[i] - in[i]);
		}
	}
	if (ch >= 0 && diff > 10) { // 差异阈值为10
		*channel = ch;
		channelZero[ch] = in[ch];
		channelMax[ch] = out[ch];
	} else {
		*channel = -1;
	}
}

void printRCCalibration() {
	print("控制   通道   零值   最大值\n");
	print("横滚     %-7d%-7d%-7d\n", rollChannel, rollChannel < 0 ? 0 : channelZero[rollChannel], rollChannel < 0 ? 0 : channelMax[rollChannel]);
	print("俯仰     %-7d%-7d%-7d\n", pitchChannel, pitchChannel < 0 ? 0 : channelZero[pitchChannel], pitchChannel < 0 ? 0 : channelMax[pitchChannel]);
	print("偏航     %-7d%-7d%-7d\n", yawChannel, yawChannel < 0 ? 0 : channelZero[yawChannel], yawChannel < 0 ? 0 : channelMax[yawChannel]);
	print("油门     %-7d%-7d%-7d\n", throttleChannel, throttleChannel < 0 ? 0 : channelZero[throttleChannel], throttleChannel < 0 ? 0 : channelMax[throttleChannel]);
	print("模式     %-7d%-7d%-7d\n", modeChannel, modeChannel < 0 ? 0 : channelZero[modeChannel], modeChannel < 0 ? 0 : channelMax[modeChannel]);
}
