#include "stm32f10x.h"
#include "PWM.h"
#include "PID.h"
#include "Delay.h"
#include "ICM42688.h"
#include "IMU.h"
#include "Servo.h"

// ==================== 舵机防抖低通滤波变量 ====================
static float servo_filt_angle[4] = {90.0f, 90.0f, 90.0f, 90.0f};

// 舵机角度低通滤波函数
static float Servo_LowPassFilter(uint8_t servo_id, float target_angle)
{
    servo_filt_angle[servo_id] = SERVO_LPF_ALPHA * target_angle + (1.0f - SERVO_LPF_ALPHA) * servo_filt_angle[servo_id];
    return servo_filt_angle[servo_id];
}

// ==================== 原有舵机初始化函数 ====================
void Servo_Init(void)
{
	PWM2_Init();
}

// ==================== 原有舵机角度设置函数（完全保留，不改动静态中立位） ====================
void Servo_SetAngle1(float Angle)
{
	PWM_SetCompare1(Angle / 180 * 2000 + 500);
}
void Servo_SetAngle2(float Angle)
{
	PWM_SetCompare2(Angle / 180 * 2000 + 500);
}
void Servo_SetAngle3(float Angle)
{
	PWM_SetCompare3(Angle / 180 * 2000 + 500);
}
void Servo_SetAngle4(float Angle)
{
	PWM_SetCompare4(Angle / 180 * 2000 + 500);
}

// ==================== PID控制器定义 ====================
PIDController Canard_PID;
PIDController Canard_PIDZ;

// ==================== 核心优化：PID初始化（提升倾斜修正力度） ====================
void Canard_Init(Angle *Canard_Angle,Angle *PID)
{
	float Pitch = Canard_Angle->Pitch;
	float Roll = Canard_Angle->Roll;
	float dt = 0.1;
	
	// 核心修改：Kp从0.25提升到0.6，同样倾斜角度，舵机转动幅度更大，修正力更强
	// 微调规则：拉不回垂直就往上加（0.6→0.8→1.0，最高不超1.5）；舵机抖就往下降
	PIDController_Init(&Canard_PID, 0.6f, 0.05f, 0.03f, 0);
	
	PID->Pitch = (int)PIDController_compute(&Canard_PID, Pitch, dt);
	PID->Roll  = (int)PIDController_compute(&Canard_PID, Roll, dt);
}

// ==================== 核心优化：鸭翼舵机控制（放开最大偏转限幅） ====================
void Canard_ON(Angle *Canard_Angle)
{
    float target_angle1, target_angle2, target_angle3, target_angle4;
    // 调用头文件配置的最大偏转，匹配舵机物理极限
    const int16_t MAX_DEFLECTION = MAX_SERVO_DEFLECTION;
	
	// Pitch轴舵机角度计算（限幅放开到最大物理行程）
	if(Canard_Angle->Pitch >= MAX_DEFLECTION)
        target_angle3 = 90 + MAX_DEFLECTION;
	else if(Canard_Angle->Pitch <= -MAX_DEFLECTION)
        target_angle3 = 90 - MAX_DEFLECTION;
	else 
        target_angle3 = 90 + (Canard_Angle->Pitch);
		
	if(Canard_Angle->Pitch >= MAX_DEFLECTION)
        target_angle1 = 90 - MAX_DEFLECTION;
	else if(Canard_Angle->Pitch <= -MAX_DEFLECTION)
        target_angle1 = 90 + MAX_DEFLECTION;
	else 
        target_angle1 = 90 - (Canard_Angle->Pitch);
	
	// Roll轴舵机角度计算（限幅放开到最大物理行程）
	if(Canard_Angle->Roll >= MAX_DEFLECTION)
        target_angle4 = 90 + MAX_DEFLECTION;
	else if(Canard_Angle->Roll <= -MAX_DEFLECTION)
        target_angle4 = 90 - MAX_DEFLECTION;
	else 
        target_angle4 = 90 + (Canard_Angle->Roll);
		
	if(Canard_Angle->Roll >= MAX_DEFLECTION)
        target_angle2 = 90 - MAX_DEFLECTION;
	else if(Canard_Angle->Roll <= -MAX_DEFLECTION)
        target_angle2 = 90 + MAX_DEFLECTION;
	else 
        target_angle2 = 90 - (Canard_Angle->Roll);

    // 舵机角度防抖滤波
    float final_angle1 = Servo_LowPassFilter(0, target_angle1);
    float final_angle2 = Servo_LowPassFilter(1, target_angle2);
    float final_angle3 = Servo_LowPassFilter(2, target_angle3);
    float final_angle4 = Servo_LowPassFilter(3, target_angle4);

    // 输出到舵机
    Servo_SetAngle1(final_angle1);
    Servo_SetAngle2(final_angle2);
    Servo_SetAngle3(final_angle3);
    Servo_SetAngle4(final_angle4);
}
