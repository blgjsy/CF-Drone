// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 气压计模块（BMP581）——提供相对高度测量，供定高功能使用
// 依赖库：SparkFun BMP581 Arduino Library（Arduino 库管理器可安装，
// 或复制到 flix/flix/libraries/）。未安装库时本模块自动降级为"不可用"，
// 不影响编译，定高命令会给出相应提醒。

#if BARO_ENABLED

#include <Wire.h>
#include "filter.h"
#include "util.h"

#if __has_include(<SparkFun_BMP581_Arduino_Library.h>)
#include <SparkFun_BMP581_Arduino_Library.h>
#define BARO_HAVE_LIBRARY 1
#endif

// 参数
int baroSdaPin = 21;   // 默认 I2C 引脚（注意：与 SPI 默认引脚 18/19 区分，避免冲突）
int baroSclPin = 22;
int baroAddress = 0;   // I2C 地址，0 表示自动检测（0x47/0x46/0x77）
LowPassFilter<float> altitudeFilter(0.15); // 高度低通滤波

// 状态
bool baroDetected = false;      // 传感器已连接
bool baroCalibrated = false;    // 基准高度已标定
float baroPressure = NAN;       // 滤波后气压 (Pa)
float baroAltitude = NAN;       // 相对高度 (m)
float baroAbsoluteAltitude = NAN; // 绝对高度 (m)
float baroTemperature = NAN;    // 温度 (°C)
float baroBaselineAltitude = 0; // 基准绝对高度 (m)
int baroConsecutiveErrors = 0;

// 由气压换算绝对高度（国际标准大气公式）
float altitudeFromPressure(float pressure) {
	if (!(pressure > 0)) return NAN;
	return 44330.8f * (1.0f - powf(pressure / 101325.0f, 0.190284f));
}

#if BARO_HAVE_LIBRARY

BMP581 baro;

void setupBaro() {
	print("正在初始化气压计\n");
	Wire.begin(baroSdaPin, baroSclPin);
	Wire.setClock(400000);

	// 自动检测常见地址
	if (baroAddress == 0) {
		uint8_t addresses[] = {0x47, 0x46, 0x77};
		for (uint8_t addr : addresses) {
			if (baro.beginI2C(addr) == BMP5_OK) {
				baroAddress = addr;
				break;
			}
		}
	} else if (baro.beginI2C(baroAddress) != BMP5_OK) {
		baroAddress = 0;
	}

	if (baroAddress == 0) {
		print("未检测到 BMP581 气压计（定高功能不可用）\n");
		baroDetected = false;
		baroCalibrated = false;
		return;
	}

	baroDetected = true;
	baroConsecutiveErrors = 0;
	print("BMP581 已连接，地址 0x%02X，正在标定基准高度...\n", baroAddress);

	// 采集基准气压（最多约 2 秒）
	float sum = 0;
	int count = 0;
	unsigned long start = millis();
	while (count < 25 && millis() - start < 2000) {
		bmp5_sensor_data data = {0, 0};
		if (baro.getSensorData(&data) == BMP5_OK) {
			sum += data.pressure;
			baroTemperature = data.temperature;
			count++;
		}
		delay(20);
	}

	if (count > 0) {
		baroBaselineAltitude = altitudeFromPressure(sum / count);
		baroCalibrated = true;
		baroAltitude = 0;
		baroAbsoluteAltitude = baroBaselineAltitude;
		print("气压计标定完成（基准 %.2f m，温度 %.1f °C）\n", baroBaselineAltitude, baroTemperature);
	} else {
		print("气压计标定失败，将使用首次读数作为基准\n");
	}
}

void readBaro() {
	if (!baroDetected) return;

	static Rate rate(20); // 20Hz 读取
	if (!rate) return;

	bmp5_sensor_data data = {0, 0};
	if (baro.getSensorData(&data) != BMP5_OK) {
		baroConsecutiveErrors++;
		if (baroConsecutiveErrors > 20) {
			baroDetected = false;
			baroCalibrated = false;
			print("⚠️ 气压计读取失败，已标记为不可用\n");
		}
		return;
	}

	baroConsecutiveErrors = 0;
	baroPressure = data.pressure;
	baroTemperature = data.temperature;
	baroAbsoluteAltitude = altitudeFromPressure(baroPressure);

	if (!baroCalibrated) { // 标定失败时的兜底：以首次读数作为基准
		baroBaselineAltitude = baroAbsoluteAltitude;
		baroCalibrated = true;
	}

	baroAltitude = altitudeFilter.update(baroAbsoluteAltitude - baroBaselineAltitude);
}

#else // 未安装 SparkFun BMP581 库时的降级实现

void setupBaro() {
	print("未找到 SparkFun BMP581 Arduino Library（气压计功能不可用）\n");
	print("提示：Arduino 库管理器搜索安装，或复制到 flix/flix/libraries/ 后重新编译\n");
}

void readBaro() {}

#endif // BARO_HAVE_LIBRARY

bool isBaroReady() {
	return baroDetected && baroCalibrated && valid(baroAltitude);
}

float getAltitude() {
	return baroAltitude;
}

float getAbsoluteAltitude() {
	return baroAbsoluteAltitude;
}

float getBaroTemperature() {
	return baroTemperature;
}

// 重新标定：把当前位置作为零高度
void recalibrateBaro() {
	if (!baroDetected) {
		print("未检测到气压计\n");
		return;
	}
	baroBaselineAltitude = baroAbsoluteAltitude;
	altitudeFilter.reset();
	baroAltitude = 0;
	print("✓ 已重新标定，当前高度设为 0\n");
}

void printBaroStatus() {
	print("气压计：%s\n", baroDetected ? "已检测" : "未检测");
	if (baroDetected) {
		print("地址：0x%02X\n", baroAddress);
		print("状态：%s\n", isBaroReady() ? "就绪" : "未就绪");
		print("温度：%.1f °C\n", baroTemperature);
	}
	if (isBaroReady()) {
		print("相对高度：%.2f m\n", baroAltitude);
		print("绝对高度：%.2f m\n", baroAbsoluteAltitude);
		print("基准高度：%.2f m\n", baroBaselineAltitude);
		print("提示：用命令 baro reset 重新标定零高度\n");
	}
}

#endif // BARO_ENABLED
