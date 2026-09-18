#ifndef __TIMER_H
#define __TIMER_H

void TIM3_Configuration(void);
void TIM4_Configuration(void);
extern volatile uint8_t sample_flag3; // 全局变量定义
extern volatile uint8_t sample_flag4; // 全局变量定义
#endif