// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 固件主文件

#include "vector.h"
#include "quaternion.h"
#include "util.h"

extern float t, dt;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;
extern Vector gyro, acc;
extern Vector rates;
extern Quaternion attitude;
extern bool landed;
extern float motors[4];

void setup() {
	setupConsole();                         // 初始化串口控制台
	print("开始初始化\n");
	setupParameters();                     // 初始化参数存储
	setupPower();                          // 初始化电源管理
	setupLED();                            // 初始化LED指示灯
	setLED(true);                          // 打开LED指示灯，表示正在初始化
	setupMotors();                         // 初始化电机控制
	setupMavlink();                        // 初始化MAVLink通信
	setupWiFi();                           // 初始化WiFi连接
	setupIMU();                            // 初始化IMU传感器
	setupRC();                             // 初始化遥控接收机
	setLED(false);                         // 关闭LED指示灯，表示初始化完成
	print("初始化完成\n");
}

void loop() {
	readIMU();                             // 读取IMU传感器数据
	step();                                // 执行控制循环
	readRC();                              // 读取遥控器输入
	estimate();                            // 执行状态估计
	control();                             // 执行控制算法
	sendMotors();                          // 发送电机控制信号
	handleConsole();                       // 处理串口控制台输入
	processMavlink();                      // 处理MAVLink通信
	readVoltage();                         // 读取电池电压
	logData();                             // 记录数据
	syncParameters();                      // 同步参数存储
}
