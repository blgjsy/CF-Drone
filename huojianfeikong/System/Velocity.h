#ifndef __VELOCITY_H
#define __VELOCITY_H

#include "stm32f10x.h"

typedef struct {
    float dt;        // 采样周期（需在初始化时赋值）
    float h;         // 高度估计
    float v;         // 速度估计
    float a_bias;    // 加速度计零偏
    float P[2][2];   // 协方差矩阵（2x2）
    float Q[2][2];   // 过程噪声协方差
    float R;         // 观测噪声方差
} KalmanFilter;

void Kalman_Init(KalmanFilter *kf, float dt_init, float h0, float sigma_a, float sigma_h);
void Kalman_Predict(KalmanFilter *kf, float a_z);
void Kalman_Update(KalmanFilter *kf, float h_baro);

#endif