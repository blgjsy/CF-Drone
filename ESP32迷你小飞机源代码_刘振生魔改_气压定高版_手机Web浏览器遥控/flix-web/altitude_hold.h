// altitude_hold.h
#ifndef ALTITUDE_HOLD_H
#define ALTITUDE_HOLD_H

// ==================== 基础函数声明 ====================
// 这些函数在bmp581_enhanced.ino中实现
bool isBMP581Ready();
float getCurrentAltitude();      // 相对高度
float getAbsoluteAltitude();     // 绝对高度
float getTemperature();
float getBaselineAltitude();

// ==================== 气压定高系统状态 ====================
// 这些变量在control.ino中定义
extern bool altitudeHoldEngaged;          // 定高是否激活
extern float altitudeHoldTarget;          // 定高目标高度
extern float altitudeHoldThrottleCorrection; // 油门修正量

// 这些变量在web_rc_core.ino中定义
extern bool altitudeHoldActive;           // Web RC定高激活状态

// ==================== 飞行模式定义 ====================
#ifndef FLIGHT_MODES_DEFINED
#define FLIGHT_MODES_DEFINED
enum FlightMode {
    MODE_MANUAL = 0,
    MODE_ACRO = 1,
    MODE_STAB = 2,
    MODE_AUTO = 3
};
#endif

// ==================== 新增功能接口 ====================
// 这些函数在bmp581_enhanced.ino中实现
void restartBMP581();
uint8_t getFilterCoefficient();
bool setFilterCoefficient(uint8_t coeff);
void recalibrate();
String getSensorStatus();

// ==================== 气压定高控制函数 ====================
// 这些函数在control.ino中实现
void engageAltitudeHold();
void disengageAltitudeHold();
void updateAltitudeHoldControl();
bool isAltitudeHoldEngaged();        // 检查定高是否激活
float getTargetAltitude();           // 获取目标高度
void setAltitudeHoldTarget(float newTarget); // 设置目标高度
void enableAltitudeHold(float currentAltitude);  // 启用定高（CLI兼容）
void disableAltitudeHold();          // 禁用定高（CLI兼容）

#endif // ALTITUDE_HOLD_H