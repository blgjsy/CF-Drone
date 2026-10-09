// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 工具函数

#pragma once

#include <math.h>
#include <ESP32_NOW_Serial.h>

const float ONE_G = 9.80665;
extern float t;

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool invalid(float x) {
	return !isfinite(x);
}

bool valid(float x) {
	return isfinite(x);
}

bool floatEquals(float a, float b, float epsilon = 0) {
	if (isnan(a) && isnan(b)) return true;
	if (a == b) return true;
	return fabsf(a - b) <= epsilon;
}

// 将角度包装到 [-PI, PI)
float wrapAngle(float angle) {
	angle = fmodf(angle, 2 * PI);
	if (angle > PI) {
		angle -= 2 * PI;
	} else if (angle < -PI) {
		angle += 2 * PI;
	}
	return angle;
}

// 去除首尾空格并按空格拆分字符串
void splitString(String& str, String& token0, String& token1, String& token2) {
	str.trim();
	if (str.isEmpty()) return;
	char chars[str.length() + 1];
	str.toCharArray(chars, str.length() + 1);
	token0 = strtok(chars, " ");
	token1 = strtok(NULL, " ");
	token2 = strtok(NULL, "");
	if (token1.c_str() == NULL) token1 = "";
	if (token2.c_str() == NULL) token2 = "";
}

// 不带重发功能的简化版ESP-NOW串口
class ESPNOWSerial : public ESP_NOW_Serial_Class {
public:
	int lost = 0;
	using ESP_NOW_Serial_Class::ESP_NOW_Serial_Class;
	void onSent(bool success) override {
		if (!success) lost++;
		ESP_NOW_Serial_Class::onSent(true); // 始终报告成功以避免重发
	}
};

// 频率限制器
class Rate {
public:
	float rate;
	float last = -INFINITY;
	Rate(float rate) : rate(rate) {}

	operator bool() {
		if (t == last) {
			return true; // 同一控制周期
		}
		if (t - last >= 1 / rate) {
			last = t;
			return true;
		}
		return false;
	}
};

// 布尔信号的延迟滤波器 - 确保信号至少持续 'delay' 秒
class Delay {
public:
	float delay;
	float start = NAN;
	Delay(float delay) : delay(delay) {}

	bool update(bool on) {
		if (!on) {
			start = NAN;
			return false;
		} else if (isnan(start)) {
			start = t;
		}
		return t - start >= delay;
	}
};
