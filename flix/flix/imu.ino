// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// IMU传感器相关操作

#include <SPI.h>
#include <Wire.h>
#include <FlixPeriph.h>
#include "vector.h"
#include "filter.h"
#include "util.h"

IMU *imu;
int imuModel = -1; // 1 - MPU9250, 2 - ICM20948, 3 - MPU6050, 4 - ICM40609D
int imuBus = 0; // 0 - SPI, 1 - I2C
int imuSckPin = SCK, imuMisoPin = MISO, imuMosiPin = MOSI, imuCsPin = SS, imuIntPin = -1;
int imuSdaPin = SDA, imuSclPin = SCL;
Vector imuRotation(0, 0, PI / 2); // IMU方向，用欧拉角表示

Vector gyro; // 陀螺仪输出，弧度/秒
Vector gyroBias;

Vector acc; // 加速度计输出，米/秒²
Vector accBias;
Vector accScale(1, 1, 1);

LowPassFilter<Vector> gyroBiasFilter(0.001);

void setupIMU() {
	print("正在初始化IMU\n");
	free(imu);
	if (imuModel == 3) imuBus = 1; // MPU6050 仅支持I2C

	if (imuBus == 0) {
		// SPI连接
		SPI.begin(imuSckPin, imuMisoPin, imuMosiPin);
		imu = IMU::create(imuModel, SPI, imuCsPin, imuIntPin);
	} else {
		// I2C连接
		Wire.setPins(imuSdaPin, imuSclPin);
		imu = IMU::create(imuModel, Wire, imuIntPin);
	}

	imu->begin();
	configureIMU();
}

void configureIMU() {
	imu->setAccelRange(IMU::ACCEL_RANGE_4G);
	imu->setGyroRange(IMU::GYRO_RANGE_2000DPS);
	imu->setDLPF(IMU::DLPF_MAX);
	imu->setRate(IMU::RATE_1KHZ_APPROX);
	imu->setupInterrupt();
}

void readIMU() {
	imu->waitForData();
	imu->getGyro(gyro.x, gyro.y, gyro.z);
	imu->getAccel(acc.x, acc.y, acc.z);
	calibrateGyroOnce();

	// 应用缩放和零偏
	acc = (acc - accBias) / accScale;
	gyro = gyro - gyroBias;

	// 旋转到机体坐标系
	Quaternion rotation = Quaternion::fromEuler(imuRotation);
	acc = Quaternion::rotateVector(acc, rotation.inversed());
	gyro = Quaternion::rotateVector(gyro, rotation.inversed());
}

void calibrateGyroOnce() {
	static Delay landedDelay(2);
	if (!landedDelay.update(landed)) return; // 仅在确定静止时进行校准

	gyroBias = gyroBiasFilter.update(gyro);
}

void calibrateAccel() {
	print("正在校准加速度计\n");
	imu->setAccelRange(IMU::ACCEL_RANGE_2G); // 最灵敏的模式

	print("1/6 保持水平放置 [8秒]\n");
	pause(8);
	calibrateAccelOnce();
	print("2/6 机头朝上放置 [8秒]\n");
	pause(8);
	calibrateAccelOnce();
	print("3/6 机头朝下放置 [8秒]\n");
	pause(8);
	calibrateAccelOnce();
	print("4/6 右侧朝下放置 [8秒]\n");
	pause(8);
	calibrateAccelOnce();
	print("5/6 左侧朝下放置 [8秒]\n");
	pause(8);
	calibrateAccelOnce();
	print("6/6 倒置放置 [8秒]\n");
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print("✓ 校准完成！\n");
	configureIMU();
}

void calibrateAccelOnce() {
	const int samples = 1000;
	static Vector accMax(-INFINITY, -INFINITY, -INFINITY);
	static Vector accMin(INFINITY, INFINITY, INFINITY);

	// 计算加速度计读数的平均值
	acc = Vector(0, 0, 0);
	for (int i = 0; i < samples; i++) {
		imu->waitForData();
		Vector sample;
		imu->getAccel(sample.x, sample.y, sample.z);
		acc = acc + sample;
	}
	acc = acc / samples;

	// 更新最大值和最小值
	if (acc.x > accMax.x) accMax.x = acc.x;
	if (acc.y > accMax.y) accMax.y = acc.y;
	if (acc.z > accMax.z) accMax.z = acc.z;
	if (acc.x < accMin.x) accMin.x = acc.x;
	if (acc.y < accMin.y) accMin.y = acc.y;
	if (acc.z < accMin.z) accMin.z = acc.z;

	// 计算缩放和零偏
	accScale = (accMax - accMin) / 2 / ONE_G;
	accBias = (accMax + accMin) / 2;
}

void printIMUCalibration() {
	print("陀螺仪零偏：%f %f %f\n", gyroBias.x, gyroBias.y, gyroBias.z);
	print("加速度计零偏：%f %f %f\n", accBias.x, accBias.y, accBias.z);
	print("加速度计缩放：%f %f %f\n", accScale.x, accScale.y, accScale.z);
}

void printIMUInfo() {
	imu->status() ? print("状态：错误 %d\n", imu->status()) : print("状态：正常\n");
	print("型号：%s\n", imu->getModel());
	print("设备ID：0x%02X\n", imu->whoAmI());
	print("频率：%.0f\n", loopRate);
	print("中断模式：%s\n", imuIntPin != -1 ? "pin" : "timer");
	print("温度：%.1f °C\n", imu->getTemp());
	print("陀螺仪：%f %f %f\n", gyro.x, gyro.y, gyro.z);
	print("加速度：%f %f %f\n", acc.x, acc.y, acc.z);
	imu->waitForData();
	Vector rawGyro, rawAcc;
	imu->getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
	imu->getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
	print("原始陀螺仪：%f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
	print("原始加速度：%f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
}
