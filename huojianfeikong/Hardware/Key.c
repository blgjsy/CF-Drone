#include "stm32f10x.h"                  // Device header
#include "Delay.h"
	uint8_t count = 0;
void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
}

uint8_t Key_GetNum(void)
{
		uint8_t KeyNum = 0;	

	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == 0)			
	{	
		while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == 0);
		Delay_ms(20);
		KeyNum = 1;												
	}
	
	return KeyNum;	

}

uint8_t Key_Count(uint8_t Num)
{

	if(Num == 1)
	{
		count ++;
	}
	if(count > 4)
	{
		count = 0;
	}
	return count;
}