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

// 气压定高状态
bool altHoldEngaged = false;    // 定高是否激活
float altHoldTarget = 0;        // 目标高度 (m)
float altHoldBaseThrottle = 0;  // 启用时的基准油门（修正量的基准）
float altHoldIntegral = 0;      // PID 积分项
float altHoldLastError = 0;     // 上次高度误差
float altHoldP = 0.8, altHoldI = 0.05, altHoldD = 0.1; // 高度PID增益
float altHoldIntegralLimit = 0.3;   // 积分限幅
float altHoldMaxCorrection = 0.25;  // 油门修正限幅（最大 ±25% 油门）
float altHoldExitThrottle = 0.2;    // 油门摇杆偏移超过此值自动退出定高

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

#if WEB_RC_ENABLED
extern uint16_t webRCButtons; // 网页遥控按钮位掩码（在 web_rc.ino 中定义）
#endif

void control() {
	interpretControls();
#if WEB_RC_ENABLED
	interpretWebRC();
#endif
	updateAltHold();
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

#if WEB_RC_ENABLED
	if (!isUsingWebRC()) { // 网页遥控激活时，实体遥控的解锁手势失效，避免互相干扰
#endif
	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true; // 解锁手势
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false; // 锁定手势
#if WEB_RC_ENABLED
	}
#endif

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

// ==================== 气压定高 ====================

// 检查定高方法是否受支持（是否检测到就绪的气压计）
bool altHoldSupported() {
#if BARO_ENABLED
	return isBaroReady();
#else
	return false;
#endif
}

// 启用定高。切换时检查是否支持：不支持则提醒并拒绝；force=true 可强制启用
bool engageAltHold(bool force) {
	if (altHoldEngaged) return true;

	if (!altHoldSupported()) {
		print("❌ 定高不可用：未检测到气压计（可用 baro 命令查看状态）\n");
		if (!force) {
			print("提示：可执行 althold force 强制启用（无气压计时定高不生效）\n");
			return false;
		}
		print("⚠️ 已强制启用定高：无气压计数据，定高将不生效\n");
	}
	if (!armed) {
		print("❌ 请先解锁再启用定高\n");
		return false;
	}
	if (mode != STAB) {
		print("❌ 定高仅在稳定模式(STAB)下可用，当前模式：%s\n", getModeName());
		return false;
	}

	altHoldTarget = 0;
#if BARO_ENABLED
	altHoldTarget = altHoldSupported() ? getAltitude() : 0;
#endif
	altHoldBaseThrottle = thrustTarget;
	altHoldIntegral = 0;
	altHoldLastError = 0;
	altHoldEngaged = true;
	print("✅ 定高已启用，目标高度 %.2f m，基准油门 %.0f%%\n", altHoldTarget, altHoldBaseThrottle * 100);
	return true;
}

void disengageAltHold(const char *reason) {
	if (!altHoldEngaged) return;
	altHoldEngaged = false;
	altHoldIntegral = 0;
	altHoldLastError = 0;
	if (reason) {
		print("定高已退出：%s\n", reason);
	} else {
		print("定高已关闭\n");
	}
}

// 定高控制：根据高度误差计算油门，覆盖手动油门
void updateAltHold() {
	if (!altHoldEngaged) return;
#if BARO_ENABLED
	if (!armed || mode != STAB) {
		disengageAltHold("未解锁或模式已改变");
		return;
	}
	if (!altHoldSupported()) {
		disengageAltHold("气压计不可用");
		return;
	}
	// 油门摇杆大幅移动时交还手动控制
	if (abs(controlThrottle - altHoldBaseThrottle) > altHoldExitThrottle) {
		disengageAltHold("油门摇杆大幅移动");
		return;
	}
	if (!(dt > 0)) return;

	float error = altHoldTarget - getAltitude();
	altHoldIntegral = constrain(altHoldIntegral + error * dt, -altHoldIntegralLimit, altHoldIntegralLimit);
	float derivative = (error - altHoldLastError) / dt;
	altHoldLastError = error;
	float correction = altHoldP * error + altHoldI * altHoldIntegral + altHoldD * derivative;
	correction = constrain(correction, -altHoldMaxCorrection, altHoldMaxCorrection);
	// 目标推力 = 基准推力 + 修正量（绝对值，不做累加，避免漂移）
	thrustTarget = constrain(altHoldBaseThrottle + correction, 0.05f, 1.0f);
#else
	disengageAltHold("未编译气压计支持");
#endif
}

const char* getAltHoldStatus() {
	if (!altHoldEngaged) return "关闭";
	if (!altHoldSupported()) return "强制启用(无气压计)";
	return "运行中";
}

#if WEB_RC_ENABLED
// 网页遥控器按钮处理（上升沿触发）
void interpretWebRC() {
	if (!isUsingWebRC()) return;

	static uint16_t lastWebRCButtons = 0;
	uint16_t risingEdge = webRCButtons & ~lastWebRCButtons;
	lastWebRCButtons = webRCButtons;

	// 按钮0：解锁
	if (risingEdge & 0x0001) {
		if (controlThrottle > 0.05) {
			setWebRCWarn("油门未归零，无法解锁");
		} else {
			armed = true;
		}
	}
	// 按钮1：上锁
	if (risingEdge & 0x0002) {
		armed = false;
		disengageAltHold(nullptr);
	}
	// 按钮2：急停
	if (risingEdge & 0x0004) {
		armed = false;
		thrustTarget = 0;
		disengageAltHold(nullptr);
		setWebRCWarn("已急停");
	}
	// 按钮3：模式循环 STAB ↔ ACRO
	if (risingEdge & 0x0008) {
		if (mode == STAB) {
			mode = ACRO;
			disengageAltHold("模式已切换到特技");
		} else {
			mode = STAB;
		}
	}
	// 按钮4：定高开关（切换时检查气压计支持，不支持时提醒）
	if (risingEdge & 0x0010) {
		if (altHoldEngaged) {
			disengageAltHold(nullptr);
		} else if (!engageAltHold(false)) {
			setWebRCWarn(altHoldSupported() ? "定高启用失败（请先解锁并切到自稳）" : "定高不可用：未检测到气压计");
		}
	}
}
#endif

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
