// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

#define RC_LOSS_TIMEOUT 1
#define WEB_RC_LOSS_TIMEOUT 2  // Web遥控器超时时间稍长
#define DESCEND_TIME 10

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

// ====== Web RC相关变量声明 ======
#if WEB_RC_ENABLED
// 使用extern声明，避免重复定义（这些变量在web_control.ino中定义）
extern bool webRCEnabled;
extern bool useWebRC;
extern unsigned long webRCLastUpdate;  // 修正：使用unsigned long而不是float
extern float webRCRoll, webRCPitch, webRCYaw, webRCThrottle;
#endif

// ====== 控制相关变量声明 ======
extern bool armed;
extern int mode;
extern float dt;
extern float thrustTarget;
extern float t;
extern Quaternion attitudeTarget;

void failsafe() {
    rcLossFailsafe();
    
#if WEB_RC_ENABLED
    webRCLossFailsafe();  // 新增：Web遥控器丢失保护
#endif
    
    autoFailsafe();
}

// RC loss failsafe
void rcLossFailsafe() {
    if (controlTime == 0) return;
    if (!armed) return;
    if (t - controlTime > RC_LOSS_TIMEOUT) {
        descend();
    }
}

// Web遥控器丢失保护
#if WEB_RC_ENABLED
void webRCLossFailsafe() {
    if (!webRCEnabled || !useWebRC) return;
    if (!armed) return;
    
    // 检查Web遥控器是否超时
    if (millis() / 1000.0f - webRCLastUpdate / 1000.0f > WEB_RC_LOSS_TIMEOUT) {
        print("Web RC lost, descending\n");
        descend();
        webRCEnabled = false;
        useWebRC = false;
    }
}
#endif

// Smooth descend on RC lost
void descend() {
    mode = 3;  // AUTO模式，根据control.ino中定义为3
    attitudeTarget = Quaternion();
    thrustTarget -= dt / DESCEND_TIME;
    if (thrustTarget < 0) {
        thrustTarget = 0;
        armed = false;
    }
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
    static float roll = 0, pitch = 0, yaw = 0, throttle = 0;
    
    // 检查传统RC输入是否有变化
    if (roll != controlRoll || pitch != controlPitch || 
        yaw != controlYaw || abs(throttle - controlThrottle) > 0.05) {
        if (mode == 3) mode = 2;  // AUTO转STAB，根据control.ino中定义
    }
    
    // 检查Web遥控器输入是否有变化
#if WEB_RC_ENABLED
    static float webRoll = 0, webPitch = 0, webYaw = 0, webThrottle = 0;
    if (useWebRC && webRCEnabled) {
        if (webRoll != webRCRoll || webPitch != webRCPitch || 
            webYaw != webRCYaw || abs(webThrottle - webRCThrottle) > 5) {
            if (mode == 3) mode = 2;  // AUTO转STAB
        }
        webRoll = webRCRoll;
        webPitch = webRCPitch;
        webYaw = webRCYaw;
        webThrottle = webRCThrottle;
    }
#endif
    
    roll = controlRoll;
    pitch = controlPitch;
    yaw = controlYaw;
    throttle = controlThrottle;
}

/*======================== 修改说明 ========================
1. 删除了冲突的变量定义（webRCLastUpdate等），改为使用extern声明
2. 修正了webRCLastUpdate的类型为unsigned long
3. 添加了WEB_RC_ENABLED条件编译
4. 修正了webRCLossFailsafe中的时间计算（毫秒转秒）
5. 修正了模式值（使用数字而非名称）
6. 添加了必要的extern变量声明
*/