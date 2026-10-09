// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// PID控制器实现

#pragma once

#include "filter.h"

class PID {
public:
	float p, i, d;
	float windup;
	float dtMax;

	float derivative = 0;
	float integral = 0;

	LowPassFilter<float> lpf; // 微分项的低通滤波器

	PID(float p, float i = 0, float d = 0, float windup = INFINITY, float dAlpha = 1, float dtMax = 0.1) :
		p(p), i(i), d(d), windup(windup), lpf(dAlpha), dtMax(dtMax) {}

	float update(float error) {
		float dt = t - prevTime;

		if (dt > 0 && dt < dtMax) {
			integral += error * dt;
			derivative = lpf.update((error - prevError) / dt); // 计算微分并应用低通滤波
		} else {
			integral = 0;
			derivative = 0;
		}

		prevError = error;
		prevTime = t;

		return p * error + constrain(i * integral, -windup, windup) + d * derivative; // PID计算
	}

	void reset() {
		prevError = NAN;
		prevTime = NAN;
		integral = 0;
		derivative = 0;
		lpf.reset();
	}

private:
	float prevError = NAN;
	float prevTime = NAN;
};
