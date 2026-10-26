#include "stm32f10x.h"                  // Device header

/**
  * 函    数：PWM初始化
  * 参    数：无
  * 返 回 值：无
  */
void PWM2_Init(void)
{
    /* 开启时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);          // TIM2 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);         // GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);         // GPIOB 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);          // AFIO 时钟
    
    /* 配置部分重映射2 */
    GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM2, ENABLE);         // 重映射TIM2到PA0,PA1,PB10,PB11
    
    /* 初始化PA0和PA1 */
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;                  // 复用推挽输出
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;                // 50MHz速度
    
    // PA0 - TIM2_CH1
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // PA1 - TIM2_CH2
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // PB10 - TIM2_CH3
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // PB11 - TIM2_CH4
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* 配置时钟源 */
    TIM_InternalClockConfig(TIM2);                                // 使用内部时钟
    
    /* 配置时基单元 */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;   // 不分频
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;// 向上计数
    TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;             // ARR - 20ms周期
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;             // PSC - 72MHz/72=1MHz
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;          // 高级定时器专用
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);           // 初始化时基
    
    /* 配置输出比较/PWM模式 */
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);                       // 填充默认值
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             // PWM模式1
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     // 高电平有效
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能输出
    TIM_OCInitStructure.TIM_Pulse = 0;                            // 初始占空比0%
    
    // 初始化四个通道
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);                      // CH1 - PA0
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);                      // CH2 - PA1
    TIM_OC3Init(TIM2, &TIM_OCInitStructure);                      // CH3 - PB10
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);                      // CH4 - PB11
    
    /* 启用预装载寄存器 */
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);            // CH1预装载
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);            // CH2预装载
    TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);            // CH3预装载
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);            // CH4预装载
    
    TIM_ARRPreloadConfig(TIM2, ENABLE);                           // ARR预装载
    
    /* 启用定时器 */
    TIM_Cmd(TIM2, ENABLE);                                       // 启动TIM2
}


/**
  * 函    数：PWM设置CCR
  * 参    数：Compare 要写入的CCR的值，范围：0~100
  * 返 回 值：无
  * 注意事项：CCR和ARR共同决定占空比，此函数仅设置CCR的值，并不直接是占空比
  *           占空比Duty = CCR / (ARR + 1)
  */
void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM2, Compare);		//设置CCR1的值
}

void PWM_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare2(TIM2, Compare);		//设置CCR2的值
}

void PWM_SetCompare3(uint16_t Compare)
{
	TIM_SetCompare3(TIM2, Compare);		//设置CCR1的值
}

void PWM_SetCompare4(uint16_t Compare)
{
	TIM_SetCompare4(TIM2, Compare);		//设置CCR2的值

}

