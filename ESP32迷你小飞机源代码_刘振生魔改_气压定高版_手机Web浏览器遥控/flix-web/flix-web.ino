// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <Wire.h>
// Main firmware file - 增强气压定高版  魔改by @刘振生 QQ:2315960
#include "altitude_hold.h"
#include "vector.h"
#include "quaternion.h"
#include "util.h"

#define SERIAL_BAUDRATE 115200
#define WIFI_ENABLED 1
#define WEB_RC_ENABLED 1  // 启用Web遥控器

// ====== 全局变量 ======
float t = NAN; // current step time, s
float dt; // time delta from previous step, s
float controlRoll, controlPitch, controlYaw, controlThrottle; // pilot's inputs, range [-1, 1]
float controlMode = NAN;
Vector gyro; // gyroscope data
Vector acc; // accelerometer data, m/s/s
Vector rates; // filtered angular rates, rad/s
Quaternion attitude; // estimated attitude
bool landed; // are we landed and stationary
float motors[4]; // normalized motors thrust in range [0..1]

// ====== BMP581相关变量声明 ======
extern bool bmp581Initialized;     // 传感器初始化状态

// ====== 外部函数声明 ======
extern void logDataToBuffer();      // 在log.ino中定义
extern void calculateTimeStep();    // 在time.ino中定义
extern void handleSerialInput();    // 在cli.ino中定义

// ====== 内部函数声明 ======
void setupLED();
void setLED(bool on);
void blinkLED();
void setupParameters();
void syncParameters();
void setupMotors();
void sendMotors();
bool motorsActive();

// WiFi和MAVLink函数
#if WIFI_ENABLED
void setupWiFi();
void processMavlink();
#endif

// IMU相关函数
void setupIMU();
void readIMU();

// RC接收器函数
void setupRC();
bool readRC();  // 返回bool类型表示是否接收到有效信号

// 估计和控制函数
void estimate();
void control();
void failsafe();

// 增强版气压传感器函数声明（在bmp581_enhanced.ino中定义）
void setupBMP581Enhanced();
void updateBMP581Enhanced();
bool isBMP581Ready();
float getCurrentAltitude();
float getAbsoluteAltitude();
float getTemperature();
float getBaselineAltitude();

// 新增功能接口（在bmp581_enhanced.ino中定义）
void restartBMP581();
uint8_t getFilterCoefficient();
bool setFilterCoefficient(uint8_t coeff);
void recalibrate();
String getSensorStatus();

// 控制相关变量声明（在control.ino中定义）
extern int mode;  // 飞行模式
extern bool armed; // 是否解锁
extern float thrustTarget; // 目标推力
extern const char* getModeName(); // 获取模式名称函数

// ====== Web RC相关声明 ======
#if WEB_RC_ENABLED
// Web RC函数声明
void setupWebRC();
void readWebRC();
void processWebRC();

// Web RC全局变量声明（在web_rc_core.ino中定义）
extern bool webRCEnabled;
extern bool useWebRC;
extern bool webRCUpdated;
extern float webRCRoll;
extern float webRCPitch;
extern float webRCYaw;
extern float webRCThrottle;
extern uint16_t webRCButtons;
extern unsigned long webRCLastUpdate;

// Web RC函数声明（在web_rc_core.ino中定义）
void setWebRCInput(float roll, float pitch, float yaw, float throttle, uint16_t buttons);
bool isWebRCEnabled();
bool isUsingWebRC();
#endif

// 系统工具函数
void disableBrownOut();

// 气压定高控制函数声明（在control.ino中定义）
extern bool altitudeHoldEngaged;  // 气压定高是否激活
extern float altitudeHoldTarget;  // 定高目标高度
extern float altitudeHoldThrottleCorrection; // 油门修正量

// 气压定高控制函数（在control.ino中定义）
void engageAltitudeHold();
void disengageAltitudeHold();
void updateAltitudeHoldControl();

// 气压定高状态函数（在control.ino中定义）
bool isAltitudeHoldEngaged();
float getTargetAltitude();

// ====== 新增调试函数声明 ======
void printSystemStatus();
void printDetailedSystemInfo();
void printAltitudeDebugInfo();
void scanI2CDevices();

// ====== MAVLink 状态监控 ======
#if WIFI_ENABLED
// 声明 mavlinkSetup 函数（在 mavlink.ino 中定义）
void mavlinkSetup();
// 声明 mavlinkConnected 变量（在 mavlink.ino 中定义）
extern bool mavlinkConnected;
#endif

// ====== 主初始化函数 ======
void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    print("Initializing flix - 增强气压定高版\n");
    print("==========================================\n");
    
    disableBrownOut();
    setupParameters();
    setupLED();
    setupMotors();
    setLED(true);
    
#if WIFI_ENABLED
    setupWiFi();
    
    // 初始化 MAVLink 2.0
    mavlinkSetup();
#endif

#if WEB_RC_ENABLED
    setupWebRC();  // 初始化Web遥控器
#endif
    
    // ==== 快速I2C扫描 ====
    scanI2CDevices();
    
    // 初始化增强版气压传感器
    setupBMP581Enhanced();
    
    // ==== 检查气压传感器状态 ====
    print("检查气压传感器...\n");
    if (isBMP581Ready()) {
        print("✅ BMP581增强版初始化成功\n");
        print("  当前高度: %.2fm\n", getCurrentAltitude());
        print("  基准高度: %.2fm\n", getBaselineAltitude());
        print("  温度: %.1f°C\n", getTemperature());
    } else {
        print("⚠️ BMP581增强版初始化中...\n");
        print("  传感器正在校准（30秒）\n");
    }
    
    setupIMU();
    setupRC();
    setLED(false);
    
    print("Initialization complete\n");
    print("Web RC: http://192.168.4.1:8080\n");
    print("MAVLink: UDP 14550 (QGC)\n");
    print("气压定高: Web按钮5 (⛰️)\n");
    print("CLI命令: bmp status - 查看传感器状态\n");
    
    // ====== 新增：MAVLink初始化状态 ======
#if WIFI_ENABLED
    print("MAVLink状态:\n");
    print("  - 协议: MAVLink 2.0 强制模式\n");
    print("  - 系统ID: 1\n");  // 关键修改：从 2 改为 1
    print("  - 组件ID: 1\n");
    print("  - 端口: UDP 14550\n");
    print("  - 连接方式:\n");
    print("    1. QGroundControl -> 添加UDP连接\n");
    print("    2. 地址: 192.168.4.1\n");
    print("    3. 端口: 14550\n");
    print("    4. 开启串口监视器查看调试信息\n");
#endif
    
    print("==========================================\n");
}

// ====== 数据处理函数 ======
void logData() {
    logDataToBuffer();
}

// ====== 系统状态打印函数 ======
void printSystemStatus() {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint < 2000) return; // 0.5Hz限制（2秒一次）
    
    // 基本飞行状态
    Serial.print("M:");
    Serial.print(getModeName());
    Serial.print(" | ");
    
    Serial.print("A:");
    Serial.print(armed ? "ARM" : "DIS");
    Serial.print(" | ");
    
    Serial.print("T:");
    Serial.print(thrustTarget * 100, 0);
    Serial.print("% | ");
    
    // 显示姿态角
    Serial.print("P:");
    Serial.print(degrees(attitude.getPitch()), 1);
    Serial.print("° | ");
    
    Serial.print("R:");
    Serial.print(degrees(attitude.getRoll()), 1);
    Serial.print("°");
    
    // 高度信息
    if (isBMP581Ready()) {
        float currentAlt = getCurrentAltitude();
        Serial.print(" | Alt:");
        Serial.print(currentAlt, 2);
        Serial.print("m");
        
        if (altitudeHoldEngaged) {
            Serial.print(" | AH:ON");
            Serial.print(" T:");
            Serial.print(altitudeHoldTarget, 2);
            Serial.print("m");
        }
    }
    
    // 控制源显示
#if WEB_RC_ENABLED
    if (isUsingWebRC()) {
        Serial.print(" | Web RC");
    }
#endif
    
    // MAVLink状态
#if WIFI_ENABLED
    Serial.print(" | MAV:");
    Serial.print(mavlinkConnected ? "CON" : "---");
#endif
    
    Serial.println();
    lastPrint = millis();
}

// ====== 用户输入处理函数 ======
void handleInput() {
    handleSerialInput();
}

// ====== 时间步长计算函数 ======
void step() {
    calculateTimeStep();
}

// ====== I2C扫描函数 ======
void scanI2CDevices() {
    print("扫描I2C设备...\n");
    
    #define SDA_PIN 18
    #define SCL_PIN 19
    
    Wire.begin(SDA_PIN, SCL_PIN);
    
    byte error, address;
    int nDevices = 0;
    
    print("I2C总线地址范围: 0x08 ~ 0x77\n");
    
    for(address = 8; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
            print("找到设备: 0x");
            if (address < 16) print("0");
            Serial.print(address, HEX);
            
            switch(address) {
                case 0x68: print(" (MPU6050)"); break;
                case 0x77: print(" (BMP581)"); break;
                case 0x46: print(" (BMP581备用)"); break;
                case 0x76: print(" (BME280)"); break;
                case 0x1E: print(" (HMC5883L)"); break;
                default: print(" (未知)");
            }
            print("\n");
            nDevices++;
        }
    }
    
    if (nDevices == 0) {
        print("❌ 未找到I2C设备！\n");
    } else {
        print("✅ 找到 %d 个设备\n", nDevices);
    }
    print("------------------------------------------\n");
}

// ====== 气压传感器调试函数 ======
void printAltitudeDebugInfo() {
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 30000) { // 30秒一次
        print("=== 气压传感器 ===\n");
        
        if (isBMP581Ready()) {
            print("状态: 正常\n");
            print("高度: %.2fm\n", getCurrentAltitude());
            print("温度: %.1f°C\n", getTemperature());
            print("定高: %s\n", altitudeHoldEngaged ? "运行中" : "待机");
            
            if (altitudeHoldEngaged) {
                print("目标: %.2fm\n", altitudeHoldTarget);
            }
        } else {
            print("状态: 异常\n");
        }
        
        print("===================\n");
        lastDebugPrint = millis();
    }
}

// ====== 主循环函数 ======
void loop() {
    step();             // 更新时间步长
    readIMU();          // 读取IMU数据
    bool rcValid = readRC();  // 读取传统遥控器输入
    
#if WEB_RC_ENABLED
    readWebRC();        // 读取Web遥控器输入
#endif
    
    // 更新增强版气压传感器数据
    updateBMP581Enhanced();
    
    estimate();         // 姿态估计
    
    // ====== 气压定高按钮处理 ======
#if WEB_RC_ENABLED
    static bool lastAltHoldButtonState = false;
    bool currentAltHoldButtonState = (webRCButtons & 0x0020) != 0; // 按钮5
    
    // 按钮按下（上升沿）触发
    if (currentAltHoldButtonState && !lastAltHoldButtonState) {
        if (armed && mode == 2) { // 只有在解锁且稳定模式下才能启用定高
            if (isBMP581Ready()) {
                if (altitudeHoldEngaged) {
                    disengageAltitudeHold();
                } else {
                    engageAltitudeHold();
                }
            } else {
                print("❌ 气压传感器未就绪\n");
            }
        }
    }
    
    lastAltHoldButtonState = currentAltHoldButtonState;
#endif
    
    control();          // 控制计算（这里会调用updateAltitudeHoldControl()）
    
    failsafe();         // 安全保护
    sendMotors();       // 发送电机命令
    handleInput();      // 处理用户输入
    
#if WIFI_ENABLED
    processMavlink();   // 处理MAVLink通信
    
    // ====== 新增：MAVLink连接状态监控 ======
    static unsigned long lastMavlinkStatusCheck = 0;
    if (millis() - lastMavlinkStatusCheck > 3000) { // 3秒检查一次
        static bool lastConnected = false;
        if (mavlinkConnected != lastConnected) {
            if (mavlinkConnected) {
                print("✅ MAVLink已连接到地面站\n");
                print("  地面站地址: 192.168.4.2:14550\n");
                print("  协议: MAVLink 2.0\n");
                print("  系统ID: 1, 组件ID: 1\n");
            } else {
                print("⚠️ MAVLink连接断开\n");
            }
            lastConnected = mavlinkConnected;
        }
        lastMavlinkStatusCheck = millis();
    }
#endif

#if WEB_RC_ENABLED
    processWebRC();     // 处理Web遥控器
#endif
    
    logData();          // 记录数据
    syncParameters();   // 同步参数
    
    // LED闪烁（自动模式）
    blinkLED();
    
    // 系统状态打印（限频）
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 2000) { // 2秒一次（0.5Hz）
        printSystemStatus();
        lastStatusPrint = millis();
    }
    
    // 气压传感器状态检查
    static unsigned long lastAltSensorCheck = 0;
    if (millis() - lastAltSensorCheck > 5000) { // 5秒检查一次
        if (!isBMP581Ready() && altitudeHoldEngaged) {
            print("⚠️ 气压传感器不可用，禁用定高\n");
            altitudeHoldEngaged = false;
        }
        
        // 显示高度信息
        static unsigned long lastAltDisplay = 0;
        if (millis() - lastAltDisplay > 3000) { // 3秒显示一次
            if (isBMP581Ready()) {
                float currentAlt = getCurrentAltitude();
                float targetAlt = getTargetAltitude();
                
                if (altitudeHoldEngaged) {
                    print("AH: %.2fm -> %.2fm\n", currentAlt, targetAlt);
                }
            }
            lastAltDisplay = millis();
        }
        lastAltSensorCheck = millis();
    }
    
    // 调试信息打印（30秒一次）
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 30000) {
        printAltitudeDebugInfo();
        lastDebugPrint = millis();
    }
    
    // ====== 新增：MAVLink数据发送状态监控 ======
#if WIFI_ENABLED
    static unsigned long lastMavlinkDataLog = 0;
    if (mavlinkConnected && millis() - lastMavlinkDataLog > 10000) { // 10秒一次
        print("📡 MAVLink数据流状态:\n");
        print("  - 心跳包: 1Hz\n");
        print("  - 姿态数据: 10Hz\n");
        print("  - 系统状态: 1Hz\n");
        print("  - IMU数据: 10Hz\n");
        if (isBMP581Ready()) {
            print("  - 高度数据: 10Hz\n");
        }
        print("  - RC通道: 10Hz\n");
        print("  - 电机状态: 10Hz\n");
        print("  - 系统ID: 1, 组件ID: 1\n");
        lastMavlinkDataLog = millis();
    }
#endif
}

// ====== 辅助函数 ======

// 获取飞行模式名称的辅助函数
const char* getModeNameFromInt(int mode) {
    switch (mode) {
        case 0: return "MANUAL";
        case 1: return "ACRO";
        case 2: return "STAB";
        case 3: return "AUTO";
        default: return "UNKNOWN";
    }
}

// 获取气压定高状态的字符串
const char* getAltitudeHoldStatusString() {
    if (!isBMP581Ready()) {
        return "传感器未就绪";
    }
    return altitudeHoldEngaged ? "运行中" : "待机";
}

// 打印详细的系统信息
void printDetailedSystemInfo() {
    print("=== 系统信息 ===\n");
    print("飞行模式: %s\n", getModeName());
    print("解锁状态: %s\n", armed ? "已解锁" : "已锁定");
    print("姿态: R=%.1f° P=%.1f° Y=%.1f°\n", 
          degrees(attitude.getRoll()), 
          degrees(attitude.getPitch()), 
          degrees(attitude.getYaw()));
    print("油门: %.1f%%\n", thrustTarget * 100);
    
    if (isBMP581Ready()) {
        print("气压传感器: 正常\n");
        print("当前高度: %.2fm\n", getCurrentAltitude());
        print("定高状态: %s\n", getAltitudeHoldStatusString());
        print("温度: %.1f°C\n", getTemperature());
        
        if (altitudeHoldEngaged) {
            print("目标高度: %.2fm\n", altitudeHoldTarget);
        }
    } else {
        print("气压传感器: ❌ 异常\n");
    }
    
    print("电机输出: FL=%.2f FR=%.2f RL=%.2f RR=%.2f\n",
          motors[2], motors[3], motors[0], motors[1]);
    
#if WEB_RC_ENABLED
    print("Web遥控器: %s\n", isUsingWebRC() ? "使用中" : "未使用");
#endif
    
#if WIFI_ENABLED
    print("MAVLink: %s\n", mavlinkConnected ? "已连接" : "未连接");
    if (mavlinkConnected) {
        print("  - 地面站: 192.168.4.2:14550\n");
        print("  - 协议: MAVLink 2.0\n");
        print("  - 系统ID: 1, 组件ID: 1\n");
    }
#endif
    
    print("================\n");
}