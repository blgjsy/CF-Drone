// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 命令行接口的实现

#include "pid.h"
#include "vector.h"
#include "util.h"
#include "filter.h"

extern const int MOT_RL, MOT_RR, MOT_FR, MOT_FL;
extern const int RAW, ACRO, STAB, AUTO;
extern const int W_AP, W_STA, W_ESPNOW;
extern float t, dt, loopRate;
extern uint16_t channels[16];
extern float controlTime;
extern int mode;
extern bool armed;
extern LowPassFilter<Vector> gyroBiasFilter;
extern float voltage;

const char* motd =
" _______  __       __  ___   ___\n"
"|   ____||  |     |  | \\  \\ /  /\n"
"|  |__   |  |     |  |  \\  V  /\n"
"|   __|  |  |     |  |   >   <\n"
"|  |     |  `----.|  |  /  .  \\\n"
"|__|     |_______||__| /__/ \\__\\\n\n"
"(C) Oleg Kalachev , Gemr\n"
"仓库：https://github.com/blgjsy/CF-Drone\n"
"原始仓库：https://github.com/okalachev/flix\n\n"
"命令:\n"
"help / motd - 显示帮助\n"
"p - 显示所有参数\n"
"p <name> - 显示指定参数。传入参数名\n"
"p <name> <value> - 设置参数。传入参数名和值\n"
"preset - 重置参数\n"
"time - 显示时间信息\n"
"imu - 显示 IMU 数据\n"
"ca - 校准加速度计\n"
"st - 显示状态估计\n"
"arm - 解锁无人机\n"
"disarm - 锁定无人机\n"
"raw/stab/acro/auto - 设置飞行模式：手动/稳定/动作/自动模式\n"
"rc - 显示遥控数据\n"
"cr - 校准遥控器\n"
"pw - 显示电源信息\n"
"wifi - 显示 Wi-Fi 信息\n"
"wifi ap/sta/espnow/off - 设置 Wi-Fi 模式：作为服务端/客户端/ESP-NOW/关闭\n"
"ap <ssid> <password> - 配置 Wi-Fi 接入点\n"
"sta <ssid> <password> - 配置 Wi-Fi 客户端模式\n"
"espnow <mac> [<key>] - 配置 ESP-NOW 对等设备\n"
"mot - 显示电机输出\n"
"log [dump] - 打印日志头 [和数据]\n"
"mfr/mfl/mrr/mrl [<thrust>] - 测试电机 (请移除螺旋桨)\n"
"sys - 显示系统信息\n"
"reset - 重置无人机状态\n"
"reboot - 重启无人机\n";

// 在ESP32-S3/C3中，串口默认输出到USB
#if SOC_USB_SERIAL_JTAG_SUPPORTED && ARDUINO_USB_MODE
#if !ARDUINO_USB_CDC_ON_BOOT
HWCDC HWCDCSerial;
#endif
#undef Serial
#define Serial HWCDCSerial
#endif

void setupConsole() {
	Serial.begin(115200);
#if SOC_USB_SERIAL_JTAG_SUPPORTED
	Serial.setTxTimeoutMs(0); // 不要阻塞USB写入
#endif
}

void print(const char* format, ...) {
	char buf[3000];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	Serial.print(buf);
	mavlinkPrint(buf);
}

void pause(float duration) {
	float start = t;
	while (t - start < duration) {
		step();
		handleConsole();
		processMavlink();
		delay(50);
	}
}

void doCommand(String str, bool echo = false) {
	// 解析命令
	String command, arg0, arg1;
	splitString(str, command, arg0, arg1);
	if (command.isEmpty()) return;

	// 显示命令
	if (echo) {
		print("> %s\n", str.c_str());
	}

	command.toLowerCase();

	// 执行命令
	if (command == "help" || command == "motd") {
		print("%s\n", motd);
	} else if (command == "p" && arg1 == "") {
		printParameters(arg0.c_str());
	} else if (command == "p") {
		bool success = setParameter(arg0.c_str(), arg1.toFloat());
		if (success) {
			print("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
		} else {
			print("未找到参数：%s\n", arg0.c_str());
		}
	} else if (command == "preset") {
		resetParameters();
	} else if (command == "time") {
		print("时间：%f\n", t);
		print("循环频率：%.0f\n", loopRate);
		print("时间步长 dt：%f\n", dt);
	} else if (command == "imu") {
		printIMUInfo();
		printIMUCalibration();
		print("已着陆：%d\n", landed);
	} else if (command == "st") {
		print("角速度：%g %g %g\n", rates.x, rates.y, rates.z);
		print("姿态：%g %g %g %g\n", attitude.w, attitude.x, attitude.y, attitude.z);
		print("横滚：%g° 俯仰：%g° 偏航：%g°\n", degrees(attitude.getRoll()), degrees(attitude.getPitch()), degrees(attitude.getYaw()));
		print("已着陆：%d\n", landed);
	} else if (command == "arm") {
		armed = true;
	} else if (command == "disarm") {
		armed = false;
	} else if (command == "raw") {
		mode = RAW;
	} else if (command == "stab") {
		mode = STAB;
	} else if (command == "acro") {
		mode = ACRO;
	} else if (command == "auto") {
		mode = AUTO;
	} else if (command == "rc") {
		print("通道：");
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print("\n横滚：%g 俯仰：%g 偏航：%g 油门：%g 模式：%g\n",
			controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print("时间：%.1f\n", controlTime);
		print("模式：%s\n", getModeName());
		print("已解锁：%d\n", armed);
	} else if (command == "pw") {
		print("电压：%.1f V\n", voltage);
	} else if (command == "wifi" && arg0 == "") {
		printWiFiInfo();
	} else if (command == "wifi") {
		setWiFiMode(arg0);
	} else if (command == "ap") {
		configWiFi(W_AP, arg0.c_str(), arg1.c_str());
	} else if (command == "sta") {
		configWiFi(W_STA, arg0.c_str(), arg1.c_str());
	} else if (command == "espnow") {
		configWiFi(W_ESPNOW, arg0.c_str(), arg1.c_str());
	} else if (command == "mot") {
		print("右前 %g 左前 %g 右后 %g 左后 %g\n",
			motors[MOT_FR], motors[MOT_FL], motors[MOT_RR], motors[MOT_RL]);
	} else if (command == "log") {
		printLogHeader();
		if (arg0 == "dump") printLogData();
	} else if (command == "cr") {
		calibrateRC();
	} else if (command == "ca") {
		calibrateAccel();
	} else if (command == "mfr") {
		testMotor(MOT_FR, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "mfl") {
		testMotor(MOT_FL, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "mrr") {
		testMotor(MOT_RR, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "mrl") {
		testMotor(MOT_RL, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "sys") {
#ifdef ESP32
		print("芯片：%s\n", ESP.getChipModel());
		print("温度：%.1f °C\n", temperatureRead());
		print("总内存：%d KB\n", ESP.getHeapSize() / 1024);
		print("空闲堆内存：%d KB\n", ESP.getFreeHeap() / 1024);
		print("固件：" __DATE__ " " __TIME__ "\n");
		// 打印任务表
		print("编号  任务                最小栈  优先级 核心  CPU%%\n");
		int taskCount = uxTaskGetNumberOfTasks();
		TaskStatus_t *systemState = new TaskStatus_t[taskCount];
		uint32_t totalRunTime;
		uxTaskGetSystemState(systemState, taskCount, &totalRunTime);
		for (int i = 0; i < taskCount; i++) {
			String core = systemState[i].xCoreID == tskNO_AFFINITY ? "*" : String(systemState[i].xCoreID);
			int cpuPercentage = systemState[i].ulRunTimeCounter / (totalRunTime / 100);
			print("%-5d%-20s%-7d%-6d%-6s%d\n",systemState[i].xTaskNumber, systemState[i].pcTaskName,
				systemState[i].usStackHighWaterMark, systemState[i].uxCurrentPriority, core.c_str(), cpuPercentage);
		}
		delete[] systemState;
#endif
	} else if (command == "reset") {
		attitude = Quaternion();
		gyroBiasFilter.reset();
	} else if (command == "reboot") {
		ESP.restart();
	} else {
		print("无效命令：%s\n", command.c_str());
	}
}

void handleConsole() {
	static bool showMotd = true;
	static String input;

	if (showMotd) {
		print("%s\n", motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n' || c == '\r') {
			doCommand(input);
			input.clear();
		} else {
			input += c;
		}
	}
}
