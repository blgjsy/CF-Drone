// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 飞行控制

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "filter.h"
#include "util.h"

const int RAW = 0, ACRO = 1, STAB = 2, AUTO = 3; // 飞行模式
int mode = STAB;
bool armed = false;

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; // 前馈角速度
Vector torqueTarget;
float thrustTarget;

PID rollRatePID(0.05, 0.2, 0.001, 0.3, 0.2);
PID pitchRatePID(0.05, 0.2, 0.001, 0.3, 0.2);
PID yawRatePID(0.3, 0, 0, 0.3);
PID rollPID(6);
PID pitchPID(6);
PID yawPID(3);
Vector maxRate(radians(360), radians(360), radians(360));
float tiltMax = radians(30);
int flightModes[] = {STAB, STAB, STAB}; // 遥控器模式开关映射

extern const int MOT_RL, MOT_RR, MOT_FR, MOT_FL;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;

void control() {
	interpretControls();
	failsafe();
	controlAttitude();
	controlRates();
	controlTorque();
}

void interpretControls() {
	if (controlMode < 0.25) mode = flightModes[0];
	else if (controlMode <= 0.75) mode = flightModes[1];
	else if (controlMode > 0.75) mode = flightModes[2];

	if (mode == AUTO) return; // 在自动模式下，飞手输入不生效

	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true; // 解锁手势
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false; // 锁定手势

	if (abs(controlYaw) < 0.1) controlYaw = 0; // 偏航死区

	thrustTarget = controlThrottle;

	if (mode == STAB) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw(); // 重置偏航目标
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z); // 正偏航摇杆表示在FLU坐标系中顺时针旋转
	}

	if (mode == ACRO) {
		attitudeTarget.invalidate(); // 跳过姿态控制
		ratesTarget.x = controlRoll * maxRate.x;
		ratesTarget.y = controlPitch * maxRate.y;
		ratesTarget.z = -controlYaw * maxRate.z; // 正偏航摇杆表示在FLU坐标系中顺时针旋转
	}

	if (mode == RAW) { // 直接力矩控制
		attitudeTarget.invalidate(); // 跳过姿态控制
		ratesTarget.invalidate(); // 跳过角速度控制
		torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1;
	}
}

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.1) return; // 跳过姿态控制

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
	ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;

	float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
	ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}


void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.1) return; // 跳过角速度控制

	Vector error = ratesTarget - rates;

	// 计算期望力矩，其中 0 - 无力矩，1 - 最大可能力矩
	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
	if (!torqueTarget.valid()) return; // 跳过力矩控制

	if (!armed) {
		for (float& m : motors) m = 0; // 锁定时停止电机
		return;
	}

	if (thrustTarget < 0.1) {
		for (float& m : motors) m = 0.1; // 怠速推力
		return;
	}

	motors[MOT_FL] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOT_FR] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOT_RL] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOT_RR] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	desaturate(); // 优先保证角度控制，其次才满足推力控制
}

void desaturate() {
	float max_ = -INFINITY, min_ = INFINITY;
	float correction = 0;

	// 找出最大和最小推力
	for (float m : motors) {
		if (m > max_) max_ = m;
		if (m < min_) min_ = m;
	}

	if (max_ > 1) { // 推力超过最大值
		correction = max_ - 1;
	} else if (min_ < 0) { // 推力低于最小值
		correction = min_;
	}

	for (float& m : motors) {
		m -= correction;
	}
}

const char* getModeName() {
	switch (mode) {
		case RAW: return "RAW";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		case AUTO: return "AUTO";
		default: return "UNKNOWN";
	}
}
