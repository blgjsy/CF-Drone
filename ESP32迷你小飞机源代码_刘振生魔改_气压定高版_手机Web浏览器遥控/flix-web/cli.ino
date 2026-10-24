// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Implementation of command line interface

#include "pid.h"
#include "vector.h"
#include "util.h"
#include "altitude_hold.h"  // 包含飞行模式定义
#if WIFI_ENABLED
// 声明MAVLink函数
extern void mavlinkPrint(const char* str);
extern void processMavlink();
#endif
extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;

// 删除或注释掉原来的常量定义
// extern const int ACRO, STAB, AUTO;
extern float t, dt, loopRate;
extern uint16_t channels[16];
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern int mode;
extern bool armed;

const char* motd =
"\nWelcome to\n"
" _______  __       __  ___   ___\n"
"|   ____||  |     |  | \\  \\ /  /\n"
"|  |__   |  |     |  |  \\  V  /\n"
"|   __|  |  |     |  |   >   <\n"
"|  |     |  `----.|  |  /  .  \\\n"
"|  |     |  `----.|  |  /  .  \\\n"
"|__|     |_______||__| /__/ \\__\\\n\n"
"Commands:\n\n"
"help - show help\n"
"p - show all parameters\n"
"p <name> - show parameter\n"
"p <name> <value> - set parameter\n"
"preset - reset parameters\n"
"time - show time info\n"
"ps - show pitch/roll/yaw\n"
"psq - show attitude quaternion\n"
"imu - show IMU data\n"
"arm - arm the drone\n"
"disarm - disarm the drone\n"
"stab/acro/auto - set mode\n"
"rc - show RC data\n"
"mot - show motor output\n"
"log - dump in-RAM log\n"
"cr - calibrate RC\n"
"ca - calibrate accel\n"
"mfr, mfl, mrr, mrl - test motor (remove props)\n"
"sys - show system info\n"
"reset - reset drone's state\n"
"reboot - reboot the drone\n"
"althold - altitude hold commands\n"
"bmp - BMP581传感器命令\n";

void print(const char* format, ...) {
	char buf[1000];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	Serial.print(buf);
#if WIFI_ENABLED
	mavlinkPrint(buf);
#endif
}

void pause(float duration) {
	float start = t;
	while (t - start < duration) {
		calculateTimeStep();
		handleSerialInput();
#if WIFI_ENABLED
		processMavlink();
#endif
		delay(50);
	}
}

// 电机测试函数（直接在cli.ino中实现）
void motorTestCLI(int motor) {
    if (motor < 0 || motor > 3) {
        print("Invalid motor number: %d\n", motor);
        return;
    }
    
    print("WARNING: REMOVE PROPELLERS BEFORE TEST!\n");
    print("Testing motor %d for 3 seconds...\n", motor);
    
    // 保存当前电机状态
    float savedMotors[4];
    for (int i = 0; i < 4; i++) {
        savedMotors[i] = motors[i];
        motors[i] = 0; // 先停止所有电机
    }
    
    // 设置测试电机为20%油门
    motors[motor] = 0.2;
    
    // 发送电机命令
    sendMotors();
    
    // 等待3秒
    unsigned long startTime = millis();
    while (millis() - startTime < 3000) {
        // 保持系统运行
        calculateTimeStep();
        handleSerialInput();
        delay(50);
    }
    
    // 恢复所有电机
    for (int i = 0; i < 4; i++) {
        motors[i] = savedMotors[i];
    }
    sendMotors();
    
    print("Motor test complete\n");
}

// BMP581传感器命令处理
void processBMP581Command(const String& arg0, const String& arg1) {
    if (arg0 == "status") {
        extern void printSensorStatus();
        printSensorStatus();
    } else if (arg0 == "restart") {
        extern void restartBMP581();
        restartBMP581();
    } else if (arg0 == "recalibrate") {
        extern void recalibrate();
        recalibrate();
    } else if (arg0 == "filter") {
        if (arg1 != "") {
            int coeff = arg1.toInt();
            extern bool setFilterCoefficient(uint8_t);
            if (setFilterCoefficient((uint8_t)coeff)) {
                print("滤波系数设置成功\n");
            } else {
                print("滤波系数设置失败\n");
            }
        } else {
            extern uint8_t getFilterCoefficient();
            print("当前滤波系数: %d\n", getFilterCoefficient());
        }
    } else {
        print("BMP581命令:\n");
        print("  status       - 显示传感器状态\n");
        print("  restart      - 重启传感器\n");
        print("  recalibrate  - 重新校准\n");
        print("  filter [0-6] - 设置/获取滤波系数\n");
        print("    0:无滤波 1:轻微 2:低 3:中 4:高 5:强 6:最强\n");
    }
}

void doCommand(String str, bool echo = false) {
	// parse command
	String command, arg0, arg1;
	splitString(str, command, arg0, arg1);
	if (command.isEmpty()) return;

	// echo command
	if (echo) {
		print("> %s\n", str.c_str());
	}

	command.toLowerCase();

	// execute command
	if (command == "help" || command == "motd") {
		print("%s\n", motd);
	} else if (command == "p" && arg0 == "") {
		printParameters();
	} else if (command == "p" && arg0 != "" && arg1 == "") {
		print("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
	} else if (command == "p") {
		bool success = setParameter(arg0.c_str(), arg1.toFloat());
		if (success) {
			print("%s = %g\n", arg0.c_str(), arg1.toFloat());
		} else {
			print("Parameter not found: %s\n", arg0.c_str());
		}
	} else if (command == "preset") {
		resetParameters();
	} else if (command == "time") {
		print("Time: %f\n", t);
		print("Loop rate: %.0f\n", loopRate);
		print("dt: %f\n", dt);
	} else if (command == "ps") {
		Vector a = attitude.toEuler();
		print("roll: %f pitch: %f yaw: %f\n", degrees(a.x), degrees(a.y), degrees(a.z));
	} else if (command == "psq") {
		print("qw: %f qx: %f qy: %f qz: %f\n", attitude.w, attitude.x, attitude.y, attitude.z);
	} else if (command == "imu") {
		printIMUInfo();
		printIMUCalibration();
		print("landed: %d\n", landed);
	} else if (command == "arm") {
		armed = true;
		print("Drone ARMED\n");
	} else if (command == "disarm") {
		armed = false;
		print("Drone DISARMED\n");
	} else if (command == "stab") {
		mode = MODE_STAB;  // 使用枚举值
		print("Mode set to STAB\n");
	} else if (command == "acro") {
		mode = MODE_ACRO;  // 使用枚举值
		print("Mode set to ACRO\n");
	} else if (command == "auto") {
		mode = MODE_AUTO;  // 使用枚举值
		print("Mode set to AUTO\n");
	} else if (command == "rc") {
		print("channels: ");
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print("\nroll: %g pitch: %g yaw: %g throttle: %g mode: %g\n",
			controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print("mode: %s\n", getModeName());
		print("armed: %d\n", armed);
	} else if (command == "mot") {
		print("front-right %g front-left %g rear-right %g rear-left %g\n",
			motors[MOTOR_FRONT_RIGHT], motors[MOTOR_FRONT_LEFT], motors[MOTOR_REAR_RIGHT], motors[MOTOR_REAR_LEFT]);
	} else if (command == "log") {
		dumpLog();
	} else if (command == "cr") {
		calibrateRC();
	} else if (command == "ca") {
		calibrateAccel();
	} else if (command == "mfr") {
		motorTestCLI(MOTOR_FRONT_RIGHT);
	} else if (command == "mfl") {
		motorTestCLI(MOTOR_FRONT_LEFT);
	} else if (command == "mrr") {
		motorTestCLI(MOTOR_REAR_RIGHT);
	} else if (command == "mrl") {
		motorTestCLI(MOTOR_REAR_LEFT);
	} else if (command == "sys") {
#ifdef ESP32
		print("Chip: %s\n", ESP.getChipModel());
		print("Temperature: %.1f °C\n", temperatureRead());
		print("Free heap: %d\n", ESP.getFreeHeap());
		// Print tasks table
		print("Num  Task                Stack  Prio  Core  CPU%%\n");
		int taskCount = uxTaskGetNumberOfTasks();
		TaskStatus_t *systemState = new TaskStatus_t[taskCount];
		uint32_t totalRunTime;
		uxTaskGetSystemState(systemState, taskCount, &totalRunTime);
		for (int i = 0; i < taskCount; i++) {
			String core = systemState[i].xCoreID == tskNO_AFFINITY ? "*" : String(systemState[i].xCoreID);
			int cpuPercentage = systemState[i].ulRunTimeCounter / (totalRunTime / 100);
			print("%-5d%-20s%-7d%-6d%-6s%d\n",systemState[i].xTaskNumber, systemState[i].pcTaskName,
				systemState[i].usStackHighWaterMark, systemState[i].uxCurrentPriority, core, cpuPercentage);
		}
		delete[] systemState;
#endif
	} else if (command == "reset") {
		attitude = Quaternion();
		print("Drone state reset\n");
	} else if (command == "reboot") {
		print("Rebooting...\n");
		ESP.restart();
	} else if (command == "althold" || command == "ah") {
		// 气压定高相关命令 - 使用新的函数名
		if (arg0 == "enable" || arg0 == "on") {
			// 使用新的函数名
			extern void engageAltitudeHold();
			engageAltitudeHold();
			print("Altitude Hold ENABLED\n");
		} else if (arg0 == "disable" || arg0 == "off") {
			// 使用新的函数名
			extern void disengageAltitudeHold();
			disengageAltitudeHold();
			print("Altitude Hold DISABLED\n");
		} else if (arg0 == "status" || arg0 == "info") {
			// 使用新的函数名
			extern bool altitudeHoldEngaged;
			extern float altitudeHoldTarget;
			print("Altitude Hold: %s\n", altitudeHoldEngaged ? "ENABLED" : "DISABLED");
			print("Current Alt: %.2fm\n", getCurrentAltitude());
			print("Target Alt: %.2fm\n", altitudeHoldTarget);
		} else if (arg0 == "set" && arg1 != "") {
			// 使用新的函数名
			extern void setAltitudeHoldTarget(float);
			float target = arg1.toFloat();
			setAltitudeHoldTarget(target);
			print("Altitude target set to: %.2fm\n", target);
		} else {
			print("Usage: althold <command>\n");
			print("Commands: enable/disable/status/set <value>\n");
		}
	} else if (command == "bmp" || command == "alt") {
		// BMP581传感器命令
		processBMP581Command(arg0, arg1);
	} else {
		print("Invalid command: %s\n", command.c_str());
	}
}

void handleSerialInput() {
	static bool showMotd = true;
	static String input;

	if (showMotd) {
		print("%s\n", motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n') {
			doCommand(input);
			input.clear();
		} else {
			input += c;
		}
	}
}