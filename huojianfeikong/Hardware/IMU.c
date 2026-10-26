#include "stm32f10x.h"                  // Device header
#include "ICM42688.h"
#include "IMU.h"
#include "math.h"
#include "Delay.h"
//#define Acc_Gain    0.0001220f   // 加速度转换为m/s2 (满量程±4g，LSB=8g/65535)
#define Acc_Gain    0.0004882887f //满量程±16G
#define Gyro_Gr     0.06120325f   // 角速度转弧度/s (2000dps量程对应LSB)
#define RadtoDeg    57.324841f   // 弧度转角度
#define G           9.80665f     // 重力加速度
#define Kp_IMU      400.8f        // 比例增益
#define Ki_IMU      0.009f       // 积分增益
#define halfT       0.0005f        // 采样周期的一半（对应1000HZ采样周期）

float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f; // 四元数
float exInt = 0.0f, eyInt = 0.0f, ezInt = 0.0f;   // 积分误差
void IMUInit(ICM42688GyroData *DataGyro)
{
	// 开机静止校准示例（持续2秒）
	float gyro_bias[3] = {0};
	for(int i=0; i<200; i++){
		gyro_bias[0] += DataGyro->GyroX;
		gyro_bias[1] += DataGyro->GyroY;
		gyro_bias[2] += DataGyro->GyroZ;
		Delay_ms(1);
	}
	gyro_bias[0] /= 200; // 获取零偏均值

}

void IMUupdate(ICM42688AccData *DataAcc, ICM42688GyroData *DataGyro, Angle *angle) 
{
    // 原始数据读取
    float ax = (float)DataAcc->AccX * Acc_Gain * G;
    float ay = (float)DataAcc->AccY * Acc_Gain * G;
    float az = (float)DataAcc->AccZ * Acc_Gain * G;
    
    float gx = (float)DataGyro->GyroX * Gyro_Gr;
    float gy = (float)DataGyro->GyroY * Gyro_Gr;
    float gz = (float)DataGyro->GyroZ * Gyro_Gr;

    // 加速度计归一化
    float norm = sqrtf(ax*ax + ay*ay + az*az);
    if (norm < 1e-6f) return;  // 防止零除
    ax /= norm; ay /= norm; az /= norm;

    // 估计重力方向
    float vx = 2.0f*(q1*q3 - q0*q2);
    float vy = 2.0f*(q0*q1 + q2*q3);
    float vz = q0*q0 - q1*q1 - q2*q2 + q3*q3;

    // 计算叉积误差
    float ex = (ay*vz - az*vy);
    float ey = (az*vx - ax*vz);
    float ez = (ax*vy - ay*vx);

    // 积分误差
    exInt += ex * Ki_IMU;
    eyInt += ey * Ki_IMU;
    ezInt += ez * Ki_IMU;

    // 补偿陀螺仪偏差
    gx += Kp_IMU*ex + exInt;
    gy += Kp_IMU*ey + eyInt;
    gz += Kp_IMU*ez + ezInt;

    // 四元数更新（一阶龙格库塔）
    float q0t = q0 + (-q1*gx - q2*gy - q3*gz)*halfT;
    float q1t = q1 + ( q0*gx + q2*gz - q3*gy)*halfT;
    float q2t = q2 + ( q0*gy - q1*gz + q3*gx)*halfT;
    float q3t = q3 + ( q0*gz + q1*gy - q2*gx)*halfT;
//    
    // 四元数归一化
    norm = sqrtf(q0t*q0t + q1t*q1t + q2t*q2t + q3t*q3t);
    q0 = q0t / norm;
    q1 = q1t / norm;
    q2 = q2t / norm;
    q3 = q3t / norm;

    // 转换为欧拉角

    angle->Roll  = -asinf(2.0f*(q1*q3 - q0*q2)) * RadtoDeg; // Roll
    angle->Pitch =  atan2f(2.0f*(q2*q3 + q0*q1), q0*q0 - q1*q1 - q2*q2 + q3*q3) * RadtoDeg; // Pitch
	   if((DataGyro->GyroZ > 20.0f) || (DataGyro->GyroZ < -20.0f))
	{
	  	angle->Yaw -= DataGyro->GyroZ * RadtoDeg * 0.0000185294f;

		   if (angle->Yaw > 180) {      //修正连续自旋时,Yaw跳变的问题

			   angle->Yaw -= 360;

		}  else if (angle->Yaw < -180) {

		       angle->Yaw += 360;
	// Yaw
	   }  
   }
}   
