// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 故障保护功能

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

float rcLossTimeout = 1;
float descendTime = 10;
float disarmTilt = radians(120);

void failsafe() {
	rcLossFailsafe();
	autoFailsafe();
	tiltFailsafe();
}

// 遥控器信号丢失保护
void rcLossFailsafe() {
	if (!armed) return;
	if (t - controlTime > rcLossTimeout) {
		descend();
	}
}

// 遥控器信号丢失时平稳下降
void descend() {
	mode = AUTO;
	attitudeTarget = Quaternion();
	thrustTarget -= dt / descendTime;
	if (thrustTarget < 0) {
		thrustTarget = 0;
		armed = false;
	}
}

// 允许飞手中断自动飞行
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	if (abs(roll - controlRoll) > 0.05 || abs(pitch - controlPitch) > 0.05 || abs(yaw - controlYaw) > 0.05 || abs(throttle - controlThrottle) > 0.05) {
		// 控制量发生变化且未配置模式开关
		if (mode == AUTO && invalid(controlMode)) mode = STAB; // 由飞手重新接管控制
	}
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}

// 倾斜过大时锁定
void tiltFailsafe() {
	if (!armed) return;
	if (mode != STAB) return;

	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	float tilt = acos(up.z);
	if (disarmTilt && tilt > disarmTilt) {
		armed = false;
	}
}
