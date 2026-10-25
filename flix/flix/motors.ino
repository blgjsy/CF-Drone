// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 电机PWM控制

#include "util.h"

float motors[4]; // 归一化的电机推力，范围 [0..1]

int motorPins[4] = {-1, -1, -1, -1}; // 默认引脚号
int pwmFrequency = 78000;
int pwmResolution = 10;
int pwmStop = 0;
int pwmMin = 0;
int pwmMax = -1; // -1 表示占空比模式

const int MOT_RL = 0, MOT_RR = 1, MOT_FR = 2, MOT_FL = 3;

void setupMotors() {
	print("正在初始化电机\n");
	// 配置引脚
	for (int i = 0; i < 4; i++) {
		if (motorPins[i] < 0) continue; // 跳过未分配的电机
		ledcAttach(motorPins[i], pwmFrequency, pwmResolution);
		pwmFrequency = ledcChangeFrequency(motorPins[i], pwmFrequency, pwmResolution); // 重新配置时
	}
	sendMotors();
}

void sendMotors() {
	for (int i = 0; i < 4; i++) {
		if (motorPins[i] < 0) continue; // 跳过未分配的电机
		ledcWrite(motorPins[i], getDutyCycle(motors[i]));
	}
}

int getDutyCycle(float value) {
	value = constrain(value, 0, 1);

	if (pwmMax >= 0) { // PWM模式
		float pwm = mapf(value, 0, 1, pwmMin, pwmMax);
		if (value == 0) pwm = pwmStop;
		float duty = mapf(pwm, 0, 1000000 / pwmFrequency, 0, (1 << pwmResolution) - 1);
		return round(duty);
	} else { // 占空比模式
		return round(value * ((1 << pwmResolution) - 1));
	}
}

bool motorsActive() {
	return motors[0] != 0 || motors[1] != 0 || motors[2] != 0 || motors[3] != 0;
}

void testMotor(int n, float thrust) {
	print("正在测试电机 %d\n", n);
	motors[n] = thrust;
	delay(50); // ESP32可能需要等到当前周期结束才能改变占空比 https://github.com/espressif/arduino-esp32/issues/5306
	sendMotors();
	pause(3);
	motors[n] = 0;
	sendMotors();
	print("完成\n");
}
