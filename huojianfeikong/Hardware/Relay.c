#include "stm32f10x.h"                  // Device header


void Relay_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);		
	GPIO_ResetBits(GPIOB,GPIO_Pin_5);

}

void Relay_ON(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_5);
	
}
void Relay_OFF(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_5);

	
}
