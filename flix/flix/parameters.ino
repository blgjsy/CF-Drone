// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 参数在闪存中的存储

#include <Preferences.h>
#include "util.h"

extern int channelZero[16], channelMax[16];
extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;
extern int rcRxPin, voltagePin;
extern int wifiMode, wifiLongRange, wifiBroadcast, udpLocalPort, udpRemotePort, espnowChannel;
extern float rcLossTimeout, descendTime, disarmTilt;
extern float voltageScale;
extern LowPassFilter<float> voltageFilter;

#include "config.h"

Preferences storage;

struct Parameter {
	const char *name; // 最大长度为15
	bool integer;
	union { float *f; int *i; }; // 指向变量的指针
	float initial; // 默认值
	float cache; // 闪存中存储的值
	void (*callback)(); // 参数修改后调用
	Parameter(const char *name, float *variable, void (*callback)() = nullptr) : name(name), integer(false), f(variable), callback(callback) {};
	Parameter(const char *name, int *variable, void (*callback)() = nullptr) : name(name), integer(true), i(variable), callback(callback) {};
	float getValue() const { return integer ? *i : *f; };
	void setValue(const float value) { if (integer) *i = value; else *f = value; };
};

Parameter parameters[] = {
	// 控制
	{"CTL_RATE_R_P", &rollRatePID.p},
	{"CTL_RATE_R_I", &rollRatePID.i},
	{"CTL_RATE_R_D", &rollRatePID.d},
	{"CTL_RATE_R_WU", &rollRatePID.windup},
	{"CTL_RATE_R_D_A", &rollRatePID.lpf.alpha},
	{"CTL_RATE_P_P", &pitchRatePID.p},
	{"CTL_RATE_P_I", &pitchRatePID.i},
	{"CTL_RATE_P_D", &pitchRatePID.d},
	{"CTL_RATE_P_WU", &pitchRatePID.windup},
	{"CTL_RATE_P_D_A", &pitchRatePID.lpf.alpha},
	{"CTL_RATE_Y_P", &yawRatePID.p},
	{"CTL_RATE_Y_I", &yawRatePID.i},
	{"CTL_RATE_Y_D", &yawRatePID.d},
	{"CTL_RATE_Y_WU", &yawRatePID.windup},
	{"CTL_RATE_Y_D_A", &yawRatePID.lpf.alpha},
	{"CTL_RATE_P_MAX", &maxRate.y},
	{"CTL_RATE_R_MAX", &maxRate.x},
	{"CTL_RATE_Y_MAX", &maxRate.z},
	{"CTL_ATT_R_P", &rollPID.p},
	{"CTL_ATT_P_P", &pitchPID.p},
	{"CTL_ATT_Y_P", &yawPID.p},
	{"CTL_ATT_MAX", &tiltMax},
	{"CTL_FLT_MODE_0", &flightModes[0]},
	{"CTL_FLT_MODE_1", &flightModes[1]},
	{"CTL_FLT_MODE_2", &flightModes[2]},
	// IMU
	{"IMU_MODEL", &imuModel},
	{"IMU_BUS", &imuBus},
	{"IMU_PIN_SCK", &imuSckPin},
	{"IMU_PIN_MISO", &imuMisoPin},
	{"IMU_PIN_MOSI", &imuMosiPin},
	{"IMU_PIN_CS", &imuCsPin},
	{"IMU_PIN_SDA", &imuSdaPin},
	{"IMU_PIN_SCL", &imuSclPin},
	{"IMU_PIN_INT", &imuIntPin},
	{"IMU_ROT_ROLL", &imuRotation.x},
	{"IMU_ROT_PITCH", &imuRotation.y},
	{"IMU_ROT_YAW", &imuRotation.z},
	{"IMU_ACC_BIAS_X", &accBias.x},
	{"IMU_ACC_BIAS_Y", &accBias.y},
	{"IMU_ACC_BIAS_Z", &accBias.z},
	{"IMU_ACC_SCALE_X", &accScale.x},
	{"IMU_ACC_SCALE_Y", &accScale.y},
	{"IMU_ACC_SCALE_Z", &accScale.z},
	{"IMU_GYRO_BIAS_A", &gyroBiasFilter.alpha},
	// 估计
	{"EST_ACC_WEIGHT", &accWeight},
	{"EST_LVL_WEIGHT", &levelWeight},
	{"EST_RATES_LPF_A", &ratesFilter.alpha},
	// 电机
	{"MOT_PIN_FL", &motorPins[MOT_FL], setupMotors},
	{"MOT_PIN_FR", &motorPins[MOT_FR], setupMotors},
	{"MOT_PIN_RL", &motorPins[MOT_RL], setupMotors},
	{"MOT_PIN_RR", &motorPins[MOT_RR], setupMotors},
	{"MOT_PWM_FREQ", &pwmFrequency, setupMotors},
	{"MOT_PWM_RES", &pwmResolution, setupMotors},
	{"MOT_PWM_STOP", &pwmStop},
	{"MOT_PWM_MIN", &pwmMin},
	{"MOT_PWM_MAX", &pwmMax},
	// 遥控器
	{"RC_RX_PIN", &rcRxPin, setupRC},
	{"RC_ZERO_0", &channelZero[0]},
	{"RC_ZERO_1", &channelZero[1]},
	{"RC_ZERO_2", &channelZero[2]},
	{"RC_ZERO_3", &channelZero[3]},
	{"RC_ZERO_4", &channelZero[4]},
	{"RC_ZERO_5", &channelZero[5]},
	{"RC_ZERO_6", &channelZero[6]},
	{"RC_ZERO_7", &channelZero[7]},
	{"RC_MAX_0", &channelMax[0]},
	{"RC_MAX_1", &channelMax[1]},
	{"RC_MAX_2", &channelMax[2]},
	{"RC_MAX_3", &channelMax[3]},
	{"RC_MAX_4", &channelMax[4]},
	{"RC_MAX_5", &channelMax[5]},
	{"RC_MAX_6", &channelMax[6]},
	{"RC_MAX_7", &channelMax[7]},
	{"RC_ROLL", &rollChannel},
	{"RC_PITCH", &pitchChannel},
	{"RC_THROTTLE", &throttleChannel},
	{"RC_YAW", &yawChannel},
	{"RC_MODE", &modeChannel},
	// WiFi
	{"WIFI_MODE", &wifiMode},
	{"WIFI_PORT_LOC", &udpLocalPort},
	{"WIFI_PORT_REM", &udpRemotePort},
	{"WIFI_LONG_RANGE", &wifiLongRange},
	{"WIFI_BROADCAST", &wifiBroadcast},
	// ESP-NOW
	{"ESPNOW_CHANNEL", &espnowChannel},
	// MAVLink
	{"MAV_SYS_ID", &mavlinkSysId},
	{"MAV_RATE_SLOW", &telemetrySlow.rate},
	{"MAV_RATE_ATT", &telemetryAttitude.rate},
	{"MAV_RATE_RC", &telemetryRC.rate},
	{"MAV_RATE_MOT", &telemetryMotors.rate},
	{"MAV_RATE_IMU", &telemetryIMU.rate},
	// 电源
	{"PWR_VOLT_PIN", &voltagePin, setupPower},
	{"PWR_VOLT_SCALE", &voltageScale},
	{"PWR_VOLT_LPF_A", &voltageFilter.alpha},
	// 安全
	{"SF_RC_LOSS_TIME", &rcLossTimeout},
	{"SF_DESCEND_TIME", &descendTime},
	{"SF_DISARM_TILT", &disarmTilt},
};

void setupParameters() {
	print("正在初始化参数\n");
	setDefaults();
	storage.begin("flix");
	// 从存储中读取参数
	for (auto &parameter : parameters) {
		parameter.initial = parameter.getValue();
		if (storage.isKey(parameter.name)) {
			parameter.setValue(storage.getFloat(parameter.name));
		}
		parameter.cache = parameter.getValue();
	}
}

int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

const char *getParameterName(int index) {
	if (index < 0 || index >= parametersCount()) return "";
	return parameters[index].name;
}

float getParameter(int index) {
	if (index < 0 || index >= parametersCount()) return NAN;
	return parameters[index].getValue();
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			return parameter.getValue();
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			if (parameter.integer && !isfinite(value)) return false; // 不能将整数设置为NaN或Inf
			parameter.setValue(value);
			if (parameter.callback) parameter.callback();
			return true;
		}
	}
	return false;
}

void syncParameters() {
	static Rate rate(1);
	if (!rate) return; // 每秒同步一次
	if (motorsActive()) return; // 飞行中不使用闪存，以免造成延迟

	for (auto &parameter : parameters) {
		if (floatEquals(parameter.getValue(), parameter.cache)) continue; // 没有变化

		storage.putFloat(parameter.name, parameter.getValue());
		parameter.cache = parameter.getValue(); // 更新缓存
	}
}

void printParameters(const char *filter) {
	print("参数名            值             [默认值]\n");
	for (auto &parameter : parameters) {
		if (strncasecmp(parameter.name, filter, strlen(filter))) continue;

		if (floatEquals(parameter.getValue(), parameter.initial)) { // 参数已更改
			print("%-15s  %-13g\n", parameter.name, parameter.getValue());
		} else {
			print("%-15s  %-13g  [%g]\n", parameter.name, parameter.getValue(), parameter.initial);
		}
	}
}

void resetParameters() {
	storage.clear();
	ESP.restart();
}
