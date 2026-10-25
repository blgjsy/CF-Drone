// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 电源管理（电压与电流检测）

#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include "filter.h"
#include "util.h"

float voltage = NAN;      // 电池电压 (V)
float current = NAN;      // 电池电流 (A)
LowPassFilter<float> voltageFilter(1);
int voltagePin = -1;
float voltageScale = 2;
int currentPin = -1;        // 电流传感器 ADC 引脚，-1 表示禁用
float currentOffset = 2.5;  // 电流为零时的输出电压 (V)，ACS712 等霍尔传感器通常为 2.5V
float currentSens = 0.185;  // 电流传感器灵敏度 (V/A)，ACS712-5A 为 0.185 V/A

void setupPower() {
	REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA); // 低电压时禁用复位
	if (digitalPinToAnalogChannel(voltagePin) == -1) voltagePin = -1; // 测试ADC引脚
	if (digitalPinToAnalogChannel(currentPin) == -1) currentPin = -1;
}

void readVoltage() {
	if (voltagePin < 0 && currentPin < 0) return;

	static Rate rate(10);
	if (!rate) return;

	if (voltagePin >= 0) {
		float v = analogReadMilliVolts(voltagePin) * voltageScale / 1000.0f;
		voltage = voltageFilter.update(v);
	}

	if (currentPin >= 0) {
		// 电流 = (输出电压 - 零点电压) / 灵敏度
		float v = analogReadMilliVolts(currentPin) / 1000.0f;
		current = (v - currentOffset) / currentSens;
	}
}
