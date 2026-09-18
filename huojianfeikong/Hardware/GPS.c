#include "stm32f10x.h"                  // Device header
#include <stdio.h>
#include <stdarg.h>
#include "Serial.h"
#include "GPS.h"
#include "Math.h"
char GPSSerial_RxPacket[100];	
uint8_t GPSSerial_RxFlag = 0;
void GPS_Init(GPS_Information *Gps);
void GPSSerial_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;  //TX
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART2, &USART_InitStructure);
	
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
	
	USART_Cmd(USART2, ENABLE);
}


void GPSSerial_SendByte(uint8_t Byte)
{
	USART_SendData(USART2, Byte);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

void GPSSerial_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i ++)
	{
		GPSSerial_SendByte(Array[i]);
	}
}

void GPSSerial_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)
	{
		GPSSerial_SendByte(String[i]);
	}
}

uint32_t GPSSerial_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y --)
	{
		Result *= X;
	}
	return Result;
}

void GPSSerial_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)
	{
		GPSSerial_SendByte(Number / GPSSerial_Pow(10, Length - i - 1) % 10 + '0');
	}
}

int GPSfputc(int ch, FILE *f)
{
	GPSSerial_SendByte(ch);
	return ch;
}

void GPSSerial_Printf(char *format, ...)
{
	char String[100];
	va_list arg;
	va_start(arg, format);
	vsprintf(String, format, arg);
	va_end(arg);
	GPSSerial_SendString(String);
}

uint8_t GPSSerial_GetRxFlag(void)
{
	if (GPSSerial_RxFlag == 1)
	{
		GPSSerial_RxFlag = 0;
		return 1;
	}
	return 0;
}

void USART2_IRQHandler(void)
{

	static uint8_t RxState   = 0;	
    static uint8_t pRxPacket = 0;   //位置变量
	uint8_t RxData;
	
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
		{
			RxData = USART_ReceiveData(USART2);
			if (RxState == 0)
		{
			if (RxData == '$' && GPSSerial_RxFlag == 0)		
				{	
					RxState   = 1;			
					pRxPacket = 0;	
				}
		}
		else if (RxState == 1)
		{
			if (RxData == '\r')			
			{
				RxState = 2;	
			}
			else						
			{
				GPSSerial_RxPacket[pRxPacket] = RxData;		
				pRxPacket ++;			
			}
		}
	
		else if (RxState == 2)
		{
			if (RxData == '\n')			
			{
				RxState = 0;			
				GPSSerial_RxPacket[pRxPacket] = '\0';	//字符串以空字符\0结尾	
				GPSSerial_RxFlag = 1;		
			}
		}		

		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}	
}

void GPS_Init(GPS_Information *Gps)
{
	
		uint8_t  GPSFlag = 0;
		uint8_t  k = 0;  //CommaCount
		uint16_t len = 0;
	
	if (GPSSerial_RxPacket[0] == 'G' && GPSSerial_RxPacket[1] == 'N' && GPSSerial_RxPacket[2] == 'R' && GPSSerial_RxPacket[3] == 'M' && GPSSerial_RxPacket[4] == 'C')
		{
			while(GPSSerial_RxPacket[len] != '\0')
			{
				if (GPSSerial_RxPacket[len]==',')
				{
					k++;
					
					switch(k)
						{
						case 2:
							if(GPSSerial_RxPacket[len+1] =='A')
								{
									Gps->GPS_State = 1;
									GPSFlag = 1;			
								}
								else
								{
									Gps->GPS_State = 0;
								}
						break;
								
						case 3:
							if(GPSFlag == 1 && GPSSerial_RxPacket[len+1] != ',' && GPSSerial_RxPacket[len+1] != '.')
							{
									Gps->GPS_Latitude1 = (int)(GPSSerial_RxPacket[len+1] - '0')*10 +(int)(GPSSerial_RxPacket[len+2] - '0');
									Gps->GPS_Latitude2 = (int)(GPSSerial_RxPacket[len+3] - '0')*1000000+(int)(GPSSerial_RxPacket[len+4] - '0')*100000+(int)(GPSSerial_RxPacket[len+6] - '0')*10000+(int)(GPSSerial_RxPacket[len+7] - '0')*1000+(int)(GPSSerial_RxPacket[len+8] - '0')*100
										               + (int)(GPSSerial_RxPacket[len+9] - '0')*10+(int)(GPSSerial_RxPacket[len+10] - '0');                                                
							}
						break;
							
						case 5:
							if(GPSFlag == 1 && GPSSerial_RxPacket[len+1] != ',' && GPSSerial_RxPacket[len+1] != '.')
							{
									Gps->GPS_Longitude1 = (int)(GPSSerial_RxPacket[len+1] - '0')*100 +(int)(GPSSerial_RxPacket[len+2] - '0')*10+(int)(GPSSerial_RxPacket[len+3] - '0');
									Gps->GPS_Longitude2 = (int)(GPSSerial_RxPacket[len+4] - '0')*1000000 +(int)(GPSSerial_RxPacket[len+5] - '0')*100000+(int)(GPSSerial_RxPacket[len+7] - '0')*10000+(int)(GPSSerial_RxPacket[len+8] - '0')*1000+(int)(GPSSerial_RxPacket[len+9] - '0')*100
										               + (int)(GPSSerial_RxPacket[len+10] - '0')*10+(int)(GPSSerial_RxPacket[len+11] - '0');    								
							}
						break;
						
						case 7:
							if(GPSFlag == 1 && GPSSerial_RxPacket[len+1] != ',')
							{	
								Gps->GPS_Speed1 = 1.852*(int)(GPSSerial_RxPacket[len+1] - '0');
							
							}
							else if (GPSFlag == 1 && GPSSerial_RxPacket[len+1] != ',' && GPSSerial_RxPacket[len+3] == '.')
							{
								Gps->GPS_Speed1 = 1.852*((int)(GPSSerial_RxPacket[len+1] - '0')*10 + (int)(GPSSerial_RxPacket[len+2] - '0'));
							
							}
							else if(GPSFlag == 1 && GPSSerial_RxPacket[len+1] != ',' && GPSSerial_RxPacket[len+4] == '.')
							{
								Gps->GPS_Speed1 = 1.852*((int)(GPSSerial_RxPacket[len+1] - '0')*100 + (int)(GPSSerial_RxPacket[len+2] - '0')*10 + (int)(GPSSerial_RxPacket[len+3] - '0'));
							
							}
						break;
								
										
						
						}
				}
				len++;	
			}
		}
		
		

		
		else if (GPSSerial_RxPacket[0] == 'G' && GPSSerial_RxPacket[1] == 'N' && GPSSerial_RxPacket[2] == 'G' && GPSSerial_RxPacket[3] == 'G' && GPSSerial_RxPacket[4] == 'A')
		{
			while(GPSSerial_RxPacket[len] != '\0')
			{
				if (GPSSerial_RxPacket[len]==',')
				{
					k++;
					
					switch(k)
						{
						case 1:
							if(GPSSerial_RxPacket[len+1] != ',')
							{
									if(((int)(GPSSerial_RxPacket[len+1] - '0')*100000 + (int)(GPSSerial_RxPacket[len+2] - '0')*10000 + 80000) >= 240000 ){
					
									Gps -> GPS_UTC = (int)(GPSSerial_RxPacket[len+1] - '0')*100000 + (int)(GPSSerial_RxPacket[len+2] - '0')*10000 + (int)(GPSSerial_RxPacket[len+3] - '0')*1000
												   + (int)(GPSSerial_RxPacket[len+4] - '0')*100 +(int)(GPSSerial_RxPacket[len+5] - '0')*10 +(int)(GPSSerial_RxPacket[len+6] - '0') - 160000;       //转换为北京时间      
									}	
									else{
									Gps -> GPS_UTC = (int)(GPSSerial_RxPacket[len+1] - '0')*100000 + (int)(GPSSerial_RxPacket[len+2] - '0')*10000 + (int)(GPSSerial_RxPacket[len+3] - '0')*1000
												   + (int)(GPSSerial_RxPacket[len+4] - '0')*100 +(int)(GPSSerial_RxPacket[len+5] - '0')*10 +(int)(GPSSerial_RxPacket[len+6] - '0') + 80000;
									}
									
							}
						break;
						case 7:
							if(GPSSerial_RxPacket[len+1] != ',' && GPSSerial_RxPacket[len+1] != '.')
							{
									Gps -> GPS_Amount = (int)(GPSSerial_RxPacket[len+1] - '0')*10 + (int)(GPSSerial_RxPacket[len+2] - '0');                                           
							}
						break;
					
						}
				}
				len++;	
			}
		}
		
	
		
		
	
}		

