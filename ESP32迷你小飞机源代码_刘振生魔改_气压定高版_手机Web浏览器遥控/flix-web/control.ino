// control.ino
// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Flight control - Web RC优化版 + 增强气压定高 魔改by @刘振生 QQ:2315960
#include "altitude_hold.h"
#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

#if WEB_RC_ENABLED
#include "web_rc_global.h"
#endif

// ==================== 气压定高系统变量 ====================
// 这些变量在altitude_hold.h中声明为extern
bool altitudeHoldEngaged = false;          // 定高是否激活
float altitudeHoldTarget = 0.0;            // 定高目标高度 (m)
float altitudeHoldThrottleCorrection = 0.0; // 油门修正量

// 气压定高PID参数
#define ALT_HOLD_P 0.8     // 比例增益
#define ALT_HOLD_I 0.1     // 积分增益
#define ALT_HOLD_D 0.2     // 微分增益
#define ALT_HOLD_I_LIMIT 0.3    // 积分限幅
#define ALT_HOLD_MAX_CORRECTION 0.2  // 最大油门修正量

// PID状态变量
float altitudeHoldIntegral = 0.0;
float altitudeHoldLastError = 0.0;
unsigned long altitudeHoldLastTime = 0;
// ==================== 气压定高状态函数 ====================
// 这些函数在altitude_hold.h中声明，在这里实现

bool isAltitudeHoldEngaged() {
    return altitudeHoldEngaged;
}

void enableAltitudeHold(float currentAltitude) {
    engageAltitudeHold();  // 使用现有的函数
}

void disableAltitudeHold() {
    disengageAltitudeHold();  // 使用现有的函数
}

float getTargetAltitude() {
    return altitudeHoldTarget;
}

void setAltitudeHoldTarget(float newTarget) {
    altitudeHoldTarget = newTarget;
    print("定高目标设置为: %.2fm\n", altitudeHoldTarget);
}
// PID参数
#define PITCHRATE_P 0.07
#define PITCHRATE_I 0.3
#define PITCHRATE_D 0.005
#define PITCHRATE_I_LIM 0.5
#define ROLLRATE_P PITCHRATE_P
#define ROLLRATE_I PITCHRATE_I
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.5
#define YAWRATE_I 0.0
#define YAWRATE_D 0.0
#define YAWRATE_I_LIM 0.5
#define ROLL_P 6
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3
#define PITCHRATE_MAX radians(30)
#define ROLLRATE_MAX radians(30)
#define YAWRATE_MAX radians(30)
#define RATES_D_LPF_ALPHA 0.2

// 飞行模式（使用数字常量，避免与flix.ino冲突）
int mode = 2;  // STAB = 2
bool armed = false;

#if WEB_RC_ENABLED
// 在control.ino中声明外部变量（在web_rc_core.ino中定义）
extern bool webRCEnabled;
extern bool useWebRC;
extern bool webRCUpdated;
extern float webRCRoll, webRCPitch, webRCYaw, webRCThrottle;
extern uint16_t webRCButtons;
extern unsigned long webRCLastUpdate;
extern float webRCThrottleScale;
extern float webRCStickScale;
extern float webRCYawScale;

// 声明Web RC函数
bool isWebRCEnabled();
bool isUsingWebRC();
#endif

// Web遥控器控制参数
#define WEB_RC_MAX_VALUE 100.0f
#define WEB_RC_TIMEOUT 2000  // 2秒超时

// 油门曲线参数
float throttleCurveLow = 0.7f;   // 低位曲线
float throttleCurveMid = 1.0f;   // 中位曲线
float throttleCurveHigh = 1.2f;  // 高位曲线
float throttleBreakpoint1 = 0.3f;
float throttleBreakpoint2 = 0.7f;

// 摇杆曲线参数
float stickExpo = 0.25f;        // 指数曲线
float stickSuperRate = 0.15f;   // 高阶曲线

// 控制参数
float tiltBase = radians(10);    // 基础倾角限制
float tiltScale = radians(20);   // 随油门增加的倾角限制

// 前馈控制增益
float rateFeedForward = 0.05f;   // 角速率前馈增益

// PID控制器
PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);

Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = radians(30);  // 初始值，运行时动态计算

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra;
Vector torqueTarget;
float thrustTarget;

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;

// ==================== 气压传感器函数声明 ====================
// 这些函数在bmp581_enhanced.ino中实现
bool isBMP581Ready();
float getCurrentAltitude();
float getAbsoluteAltitude();
float getTemperature();
float getBaselineAltitude();

// ==================== 气压定高控制函数 ====================
void engageAltitudeHold() {
    if (!isBMP581Ready()) {
        print("❌ 气压传感器未就绪，无法启用定高\n");
        return;
    }
    
    if (!armed || mode != 2) { // 只在解锁且稳定模式下启用
        print("❌ 只能在解锁且稳定模式下启用气压定高\n");
        return;
    }
    
    altitudeHoldTarget = getCurrentAltitude();
    altitudeHoldEngaged = true;
    altitudeHoldIntegral = 0.0;
    altitudeHoldLastError = 0.0;
    altitudeHoldThrottleCorrection = 0.0;
    altitudeHoldLastTime = millis();
    
    print("✅ 气压定高已启用 | 目标高度: %.2fm\n", altitudeHoldTarget);
    
#if WEB_RC_ENABLED
    // 更新Web遥控器状态
    extern bool altitudeHoldActive;
    altitudeHoldActive = true;
#endif
}

void disengageAltitudeHold() {
    altitudeHoldEngaged = false;
    altitudeHoldThrottleCorrection = 0.0;
    
    print("✅ 气压定高已禁用\n");
    
#if WEB_RC_ENABLED
    // 更新Web遥控器状态
    extern bool altitudeHoldActive;
    altitudeHoldActive = false;
#endif
}

void updateAltitudeHoldControl() {
    if (!altitudeHoldEngaged) {
        return;
    }
    
    if (!isBMP581Ready()) {
        print("⚠️ 气压传感器异常，禁用定高\n");
        altitudeHoldEngaged = false;
        return;
    }
    
    unsigned long currentTime = millis();
    float dt = (currentTime - altitudeHoldLastTime) / 1000.0; // 转换为秒
    
    if (dt <= 0 || dt > 1.0) {
        // 时间间隔异常，重置
        altitudeHoldLastTime = currentTime;
        return;
    }
    
    // 获取当前高度
    float currentAltitude = getCurrentAltitude();
    
    // 计算高度误差
    float error = altitudeHoldTarget - currentAltitude;
    
    // PID计算
    // 比例项
    float proportional = ALT_HOLD_P * error;
    
    // 积分项（带抗饱和）
    altitudeHoldIntegral += error * dt;
    altitudeHoldIntegral = constrain(altitudeHoldIntegral, -ALT_HOLD_I_LIMIT, ALT_HOLD_I_LIMIT);
    float integral = ALT_HOLD_I * altitudeHoldIntegral;
    
    // 微分项
    float derivative = 0.0;
    if (dt > 0) {
        float errorDerivative = (error - altitudeHoldLastError) / dt;
        derivative = ALT_HOLD_D * errorDerivative;
    }
    
    // 计算总修正量
    float correction = proportional + integral + derivative;
    
    // 限幅
    correction = constrain(correction, -ALT_HOLD_MAX_CORRECTION, ALT_HOLD_MAX_CORRECTION);
    
    // 更新油门修正量
    altitudeHoldThrottleCorrection = correction;
    
    // 保存状态
    altitudeHoldLastError = error;
    altitudeHoldLastTime = currentTime;
    
    // 更新推力目标（在气压定高时）
    if (altitudeHoldEngaged) {
        // 基础油门保持为当前的thrustTarget，但加上修正
        thrustTarget += altitudeHoldThrottleCorrection;
        thrustTarget = constrain(thrustTarget, 0.05f, 1.0f);
    }
    
    // 调试输出（限频）
    static unsigned long lastDebugOutput = 0;
    if (currentTime - lastDebugOutput > 500) {
        print("AltHold: Error=%.2fm | Correction=%.1f%% | Thrust=%.1f%%\n",
              error, correction * 100, thrustTarget * 100);
        lastDebugOutput = currentTime;
    }
}



// ==================== 数据验证和修复函数 ====================
float validateAndFixThrottle(float rawThrottle) {
    if (isnan(rawThrottle) || isinf(rawThrottle)) {
        return 0.0f;
    }
    
    float throttle = rawThrottle;
    
    if (throttle > 200.0f) {
        throttle = throttle / 2000.0f * 100.0f;
    } else if (throttle > 100.0f) {
        throttle = throttle / 2.0f;
    } else if (throttle < 0.0f) {
        throttle = 0.0f;
    }
    
    throttle = constrain(throttle, 0.0f, 100.0f);
    
    return throttle;
}

float validateAndFixStick(float rawStick) {
    if (isnan(rawStick) || isinf(rawStick)) {
        return 0.0f;
    }
    
    float stick = rawStick;
    
    if (fabs(stick) > 150.0f) {
        stick = stick / 10.0f;
    }
    
    stick = constrain(stick, -100.0f, 100.0f);
    
    return stick;
}

// ==================== 油门曲线函数 ====================
float applyThrottleCurve(float throttle) {
    throttle = constrain(throttle, 0.0f, 1.0f);
    
    if (throttle < throttleBreakpoint1) {
        return throttle * throttleCurveLow;
    } else if (throttle < throttleBreakpoint2) {
        return throttle * throttleCurveMid;
    } else {
        float t = (throttle - throttleBreakpoint2) / (1.0f - throttleBreakpoint2);
        return throttleBreakpoint2 * throttleCurveMid + 
               (throttle - throttleBreakpoint2) * throttleCurveHigh;
    }
}

// ==================== 摇杆曲线函数 ====================
float applyStickCurve(float value) {
    const float deadzone = 0.02f;
    if (fabs(value) < deadzone) return 0.0f;
    
    float sign = value >= 0 ? 1.0f : -1.0f;
    float absVal = fabs(value);
    
    float normalized = (absVal - deadzone) / (1.0f - deadzone);
    normalized = constrain(normalized, 0.0f, 1.0f);
    
    float curved = normalized * (1.0f - stickExpo) + normalized * normalized * stickExpo;
    curved = curved * (1.0f - stickSuperRate) + curved * curved * curved * stickSuperRate;
    
    return curved * sign;
}

// ==================== 主要控制循环 ====================
void control() {
    interpretControls();
    
#if WEB_RC_ENABLED
    interpretWebRC();
    combineInputs();
#endif
    
    // 动态计算倾角限制
    tiltMax = tiltBase + thrustTarget * tiltScale;
    
    // 更新气压定高控制（关键：在这里调用）
    if (altitudeHoldEngaged) {
        updateAltitudeHoldControl();
    }
    
    failsafe();
    controlAttitude();
    controlRates();
    controlTorque();
}

void interpretControls() {
    // 如果Web遥控器可用且正在使用，跳过传统RC解析
#if WEB_RC_ENABLED
    if (isUsingWebRC()) {
        return;
    }
#endif
    
    // 传统RC控制解析
    if (controlMode < 0.25) mode = 2; // STAB
    if (controlMode < 0.75) mode = 2; // STAB
    if (controlMode > 0.75) mode = 2; // STAB

    if (mode == 3) return; // AUTO

    if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true;
    if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false;
}

#if WEB_RC_ENABLED
void interpretWebRC() {
    // 检查Web遥控器状态
    if (!isWebRCEnabled()) {
        useWebRC = false;
        webRCEnabled = false;
        // 气压定高失活时也禁用
        if (altitudeHoldEngaged) {
            disengageAltitudeHold();
        }
        return;
    }
    
    webRCEnabled = true;
    useWebRC = true;
    
    // 检查Web RC超时
    if (millis() - webRCLastUpdate > WEB_RC_TIMEOUT) {
        useWebRC = false;
        thrustTarget = 0.0f;
        armed = false;
        // 超时也禁用气压定高
        if (altitudeHoldEngaged) {
            disengageAltitudeHold();
        }
        print("Web RC Timeout → FAILSAFE\n");
        return;
    }
    
    // 处理解锁/上锁按钮
    static bool lastArmedState = false;
    if (armed != lastArmedState) {
        print(armed ? "Web RC: ARMED\n" : "Web RC: DISARMED\n");
        lastArmedState = armed;
    }
    
    // ARM按钮（按钮0）
    if (webRCButtons & 0x0001) {
        float throttle = validateAndFixThrottle(webRCThrottle);
        if (throttle < 5.0) {
            armed = true;
        }
    }
    
    // DISARM按钮（按钮1）
    if (webRCButtons & 0x0002) {
        armed = false;
        if (altitudeHoldEngaged) {
            disengageAltitudeHold();
        }
    }
    
    // 处理模式按钮
    static int lastMode = 2; // STAB
    if (mode != lastMode) {
        print("Web RC: Mode %s\n", getModeName());
        lastMode = mode;
    }
    
    // STAB模式（按钮6）
    if (webRCButtons & 0x0040) {
        mode = 2; // STAB
    }
    
    // ACRO模式（按钮7）
    if (webRCButtons & 0x0080) {
        mode = 1; // ACRO
        if (altitudeHoldEngaged) {
            disengageAltitudeHold();
        }
    }
}

void combineInputs() {
    if (isUsingWebRC()) {
        // 验证和修复油门数据
        float rawThrottle = webRCThrottle;
        float throttlePercent = validateAndFixThrottle(rawThrottle);
        
        // 验证和修复摇杆数据
        float rawRoll = webRCRoll;
        float rawPitch = webRCPitch;
        float rawYaw = webRCYaw;
        
        float roll = validateAndFixStick(rawRoll);
        float pitch = validateAndFixStick(rawPitch);
        float yaw = validateAndFixStick(rawYaw);
        
        // 调试输出
        static int debugCount = 0;
        if (debugCount++ % 100 == 0) {
            print("Web RC原始: T=%.1f R=%.1f P=%.1f Y=%.1f | ",
                  rawThrottle, rawRoll, rawPitch, rawYaw);
            print("修复后: T=%.1f%% R=%.1f P=%.1f Y=%.1f\n",
                  throttlePercent, roll, pitch, yaw);
        }
        
        // 转换为0.0-1.0范围
        float throttle = throttlePercent / 100.0f;
        
        // 如果不处于气压定高模式，正常处理油门
        if (!altitudeHoldEngaged) {
            throttle = applyThrottleCurve(throttle);
            throttle *= webRCThrottleScale;
            throttle = constrain(throttle, 0.0f, 1.0f);
            thrustTarget = throttle;
        }
        
        // 摇杆输入处理（不受气压定高影响）
        float rollNorm = roll / 100.0f;
        float pitchNorm = pitch / 100.0f;
        float yawNorm = yaw / 100.0f;
        
        rollNorm = applyStickCurve(rollNorm);
        pitchNorm = applyStickCurve(pitchNorm);
        yawNorm = applyStickCurve(yawNorm);
        
        rollNorm *= webRCStickScale;
        pitchNorm *= webRCStickScale;
        yawNorm *= webRCYawScale;
        
        rollNorm = constrain(rollNorm, -1.0f, 1.0f);
        pitchNorm = constrain(pitchNorm, -1.0f, 1.0f);
        yawNorm = constrain(yawNorm, -1.0f, 1.0f);
        
        if (mode == 2) { // STAB
            float yawTarget = attitudeTarget.getYaw();
            if (!armed || invalid(yawTarget) || fabs(yawNorm) > 0.01) {
                float currentYaw = attitude.getYaw();
                if (armed) {
                    float yawErr = wrapAngle(currentYaw - yawTarget);
                    yawTarget += yawErr * 0.02f;
                } else {
                    yawTarget = currentYaw;
                }
            }
            
            attitudeTarget = Quaternion::fromEuler(Vector(
                rollNorm * tiltMax,
                pitchNorm * tiltMax,
                yawTarget
            ));
            
            ratesExtra = Vector(0, 0, -yawNorm * maxRate.z);
            
        } else if (mode == 1) { // ACRO
            attitudeTarget.invalidate();
            ratesTarget.x = rollNorm * maxRate.x * (1.0f + rateFeedForward);
            ratesTarget.y = pitchNorm * maxRate.y * (1.0f + rateFeedForward);
            ratesTarget.z = -yawNorm * maxRate.z;
        }
        
    } else {
        // 传统RC输入处理
        thrustTarget = controlThrottle;
        
        if (mode == 2) { // STAB
            float yawTarget = attitudeTarget.getYaw();
            if (!armed || invalid(yawTarget) || controlYaw != 0) {
                float currentYaw = attitude.getYaw();
                if (armed) {
                    float yawErr = wrapAngle(currentYaw - yawTarget);
                    yawTarget += yawErr * 0.02f;
                } else {
                    yawTarget = currentYaw;
                }
            }
            attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
            ratesExtra = Vector(0, 0, -controlYaw * maxRate.z);
        }
        
        if (mode == 1) { // ACRO
            attitudeTarget.invalidate();
            ratesTarget.x = controlRoll * maxRate.x * (1.0f + rateFeedForward);
            ratesTarget.y = controlPitch * maxRate.y * (1.0f + rateFeedForward);
            ratesTarget.z = -controlYaw * maxRate.z;
        }
    }
}
#endif // WEB_RC_ENABLED

void controlAttitude() {
    if (!armed || attitudeTarget.invalid() || thrustTarget < 0.05) return;

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
    if (!armed || ratesTarget.invalid() || thrustTarget < 0.05) return;

    Vector error = ratesTarget - rates;

    torqueTarget.x = rollRatePID.update(error.x);
    torqueTarget.y = pitchRatePID.update(error.y);
    torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
    if (!torqueTarget.valid()) return;

    if (!armed) {
        memset(motors, 0, sizeof(motors));
        return;
    }

    if (thrustTarget < 0.05) {
        motors[0] = 0.05;
        motors[1] = 0.05;
        motors[2] = 0.05;
        motors[3] = 0.05;
        return;
    }

    // 标准四轴混控
    motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
    motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
    motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
    motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

    // 电机输出归一化
    float maxMotor = motors[0];
    float minMotor = motors[0];
    for (int i = 1; i < 4; i++) {
        if (motors[i] > maxMotor) maxMotor = motors[i];
        if (motors[i] < minMotor) minMotor = motors[i];
    }
    
    if (minMotor < 0) {
        float offset = -minMotor + 0.01f;
        for (int i = 0; i < 4; i++) {
            motors[i] += offset;
        }
        maxMotor = motors[0];
        for (int i = 1; i < 4; i++) {
            if (motors[i] > maxMotor) maxMotor = motors[i];
        }
    }
    
    if (maxMotor > 1.0f || maxMotor < 0.2f) {
        if (maxMotor < 0.001f) maxMotor = 0.001f;
        for (int i = 0; i < 4; i++) {
            motors[i] /= maxMotor;
        }
    }
    
    for (int i = 0; i < 4; i++) {
        motors[i] = constrain(motors[i], 0.05f, 1.0f);
    }
    
    static int motorDebugCount = 0;
    if (motorDebugCount++ % 200 == 0) {
        print("电机输出: FL=%.3f FR=%.3f RL=%.3f RR=%.3f | ",
              motors[2], motors[3], motors[0], motors[1]);
        print("油门=%.1f%% 姿态: R=%.1f P=%.1f Y=%.1f\n",
              thrustTarget * 100, 
              degrees(attitude.getRoll()),
              degrees(attitude.getPitch()),
              degrees(attitude.getYaw()));
    }
}

/*======================== 辅助函数 ========================*/
const char* getModeName() {
    switch (mode) {
        case 0: return "MANUAL";
        case 1: return "ACRO";
        case 2: return "STAB";
        case 3: return "AUTO";
        default: return "UNKNOWN";
    }
}

#if WEB_RC_ENABLED
// 获取当前状态
void getWebRCStatus(char* buffer, int size) {
    // 使用修复后的值显示
    float throttle = validateAndFixThrottle(webRCThrottle);
    float roll = validateAndFixStick(webRCRoll);
    float pitch = validateAndFixStick(webRCPitch);
    float yaw = validateAndFixStick(webRCYaw);
    
    snprintf(buffer, size,
             "WebRC: %s | Throttle: %.1f%% | Roll: %.1f | Pitch: %.1f | Yaw: %.1f | Mode: %s | Armed: %d | AltHold: %s | Alt: %.2fm",
             isUsingWebRC() ? "ACTIVE" : "INACTIVE",
             throttle, roll, pitch, yaw,
             getModeName(),
             armed ? 1 : 0,
             altitudeHoldEngaged ? "ON" : "OFF",
             getCurrentAltitude());
}
#endif