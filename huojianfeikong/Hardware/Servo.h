#ifndef __SERVO_H
#define __SERVO_H
#include "IMU.h"

// 舵机防抖低通滤波系数，平衡响应与防抖
#define SERVO_LPF_ALPHA  0.35f

// ==================== 舵机最大偏转角度 ====================
#define MAX_SERVO_DEFLECTION  75

// 函数声明
void Servo_Init(void);
void Canard_Init(Angle *Canard_Angle,Angle *PID);
void Servo_SetAngle1(float Angle);
void Servo_SetAngle2(float Angle);
void Servo_SetAngle3(float Angle);
void Servo_SetAngle4(float Angle);
void Canard_ON(Angle *Canard_Angle);
#endif
