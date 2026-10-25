// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 使用陀螺仪和加速度计的姿态估计

#include "quaternion.h"
#include "vector.h"
#include "filter.h"
#include "util.h"

Vector rates; // 估计的角速度，弧度/秒
Quaternion attitude; // 估计的姿态
bool landed;

float accWeight = 0.003;
float levelWeight = 0.0002;
LowPassFilter<Vector> ratesFilter(0.2); // 截止频率约 40 Hz

void estimate() {
	applyGyro();
	applyAcc();
	applyLevel();
}

void applyGyro() {
	// 对陀螺仪数据滤波以获得角速度
	rates = ratesFilter.update(gyro);

	// 将角速度应用到姿态
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(rates * dt));
}

void applyAcc() {
	// 判断是否应用加速度计重力校正
	landed = !motorsActive() && abs(acc.norm() - ONE_G) < ONE_G * 0.1f;

	if (!landed) return;

	// 计算加速度计校正量
	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	Vector correction = Vector::rotationVectorBetween(acc, up) * accWeight;

	// 应用校正
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}

void applyLevel() {
	if (landed) return;
	if (thrustTarget < 0.1) return; // 怠速推力时跳过

	// 假设飞手在飞行中大致保持无人机水平
	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	Vector correction = Vector::rotationVectorBetween(Vector(0, 0, 1), up) * levelWeight;
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}
