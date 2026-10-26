#include "stm32f10x.h"                  // Device header
#include "Serial.h"

void Vofa_SendData(float *Data,uint16_t length)
{

	for(int i = 0;i<length;i++)
	{
		Serial_SendByte(Data[i]);		//依次调用Serial_SendByte发送每个字节数据
	
	}
	uint8_t tail[4]  = {0x00, 0x00, 0x80, 0x7f};
	Serial_SendArray(tail,4);

}