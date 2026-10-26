#include "stm32f10x.h"                  // Device header

#include "Velocity.h"
#include <string.h>
#include <math.h>

void Kalman_Init(KalmanFilter *kf, float dt_init, float h0, float sigma_a, float sigma_h) {
    // 初始化结构体
    kf->dt = dt_init;
    kf->h = h0;
    kf->v = 0;
    kf->a_bias = 0;
    kf->R = sigma_h * sigma_h;
    
    // 初始化协方差矩阵
    memset(kf->P, 0, sizeof(kf->P));
    kf->P[0][0] = 0.5f;  // 高度初始方差
    kf->P[1][1] = 1.05f;  // 速度初始方差
    
    // 过程噪声矩阵Q（基于加速度噪声）
    float dt2 = kf->dt * kf->dt;
    float dt3 = dt2 * kf->dt;
    kf->Q[0][0] = 0.25 * dt3 * dt3 * sigma_a * sigma_a;
    kf->Q[0][1] = 0.5 * dt3 * sigma_a * sigma_a;
    kf->Q[1][0] = kf->Q[0][1];
    kf->Q[1][1] = dt2 * sigma_a * sigma_a;
}

void Kalman_Predict(KalmanFilter *kf, float a_z) {
    // 计算有效加速度（扣除重力）
    float a_vertical = a_z - 9.81f - kf->a_bias;
    
    // 状态预测（高度、速度）
    kf->h += kf->v * kf->dt + 0.5 * a_vertical * kf->dt * kf->dt;
    kf->v += a_vertical * kf->dt;
    
    // 协方差预测（手动展开矩阵运算）
    float F[2][2] = {{1, kf->dt}, {0, 1}};
    float FP[2][2] = {
        {F[0][0]*kf->P[0][0] + F[0][1]*kf->P[1][0], F[0][0]*kf->P[0][1] + F[0][1]*kf->P[1][1]},
        {F[1][0]*kf->P[0][0] + F[1][1]*kf->P[1][0], F[1][0]*kf->P[0][1] + F[1][1]*kf->P[1][1]}
    };
    kf->P[0][0] = FP[0][0] + kf->Q[0][0];
    kf->P[0][1] = FP[0][1] + kf->Q[0][1];
    kf->P[1][0] = FP[1][0] + kf->Q[1][0];
    kf->P[1][1] = FP[1][1] + kf->Q[1][1];
}

void Kalman_Update(KalmanFilter *kf, float h_baro) {
    // 观测矩阵H（仅观测高度）
    float H[2] = {1, 0};
    
    // 计算卡尔曼增益
    float S = H[0] * kf->P[0][0] * H[0] + kf->R;
    float K[2] = {
        (kf->P[0][0] * H[0]) / S,
        (kf->P[1][0] * H[0]) / S
    };
    
    // 状态更新
    float y = h_baro - kf->h;
    kf->h += K[0] * y;
    kf->v += K[1] * y;
    
    // 协方差更新
    float IKH[2][2] = {
        {1 - K[0]*H[0], -K[0]*H[1]},
        {-K[1]*H[0], 1 - K[1]*H[1]}
    };
    float P_new[2][2] = {
        {IKH[0][0]*kf->P[0][0] + IKH[0][1]*kf->P[1][0], IKH[0][0]*kf->P[0][1] + IKH[0][1]*kf->P[1][1]},
        {IKH[1][0]*kf->P[0][0] + IKH[1][1]*kf->P[1][0], IKH[1][0]*kf->P[0][1] + IKH[1][1]*kf->P[1][1]}
    };
    memcpy(kf->P, P_new, sizeof(P_new));
}