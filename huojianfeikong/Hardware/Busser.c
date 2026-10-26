#include "stm32f10x.h"                  // Device header

void Relay2_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//??GPIOB???
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);		//?PA1????????????	
	GPIO_SetBits(GPIOA,GPIO_Pin_12);
}



void Relay2_ON(void)
{
 
       GPIO_SetBits(GPIOA,GPIO_Pin_12);
}
void Relay2_OFF(void)
{
       GPIO_ResetBits(GPIOA,GPIO_Pin_12);
 
}