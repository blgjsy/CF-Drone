#include "stm32f10x.h"                  // Device header

volatile uint8_t sample_flag3 = 0; // 全局变量定义
volatile uint8_t sample_flag4 = 0; // 全局变量定义

void TIM3_Configuration(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. 使能 TIM3 时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // 2. 配置定时器为 100Hz 中断
    // 计算公式：中断频率 = 72MHz / (Prescaler + 1) / (Period + 1)
    TIM_TimeBaseStruct.TIM_Prescaler = 1000-1;  
    TIM_TimeBaseStruct.TIM_Period = 720-1;       
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStruct);

    // 3. 使能更新中断
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    // 4. 配置 NVIC（中断优先级）
    NVIC_InitStruct.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    // 5. 启动定时器
    TIM_Cmd(TIM3, ENABLE);
}

void TIM3_IRQHandler(void) {
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        sample_flag3 = 1;  // 设置标志位
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}



void TIM4_Configuration(void) {  // 修改函数名
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. 使能 TIM4 时钟（原TIM3）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);  // 修改时钟源[4,7](@ref)

    // 2. 定时器参数配置（保持100Hz中断）
    TIM_TimeBaseStruct.TIM_Prescaler = 7200-1;  // 分频系数
    TIM_TimeBaseStruct.TIM_Period = 1000-1;      // 自动重载值
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;  // 向上计数模式
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;       // 无时钟分割
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStruct);  // 定时器对象为TIM4

    // 3. 使能更新中断
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);  // 修改中断源

    // 4. 配置NVIC（修改中断通道）
    NVIC_InitStruct.NVIC_IRQChannel = TIM4_IRQn;  // TIM4中断通道[4,7](@ref)
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;          // 响应优先级
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    // 5. 启动定时器
    TIM_Cmd(TIM4, ENABLE);  // 使能TIM4
}

void TIM4_IRQHandler(void) {  // 修改中断服务函数名
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {  // 检查TIM4中断标志
        sample_flag4 = 1;  // 更新标志变量名
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);  // 清除中断标志[5](@ref)
    }
}