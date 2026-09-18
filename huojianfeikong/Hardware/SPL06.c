#include "stm32f10x.h"
#include "MySPI.h"
#include "SPL06.h"
#include "Delay.h"
#include "Serial.h"
#include "math.h"

void SPL06_Read_Reg(uint8_t reg, uint8_t *Data) {
    *Data = 0;
    MySPI_SPL06_Start();
    reg |= 0x80; // 设置读标志位
    MySPI_SPL06_SwapByte(reg);
    *Data = MySPI_SPL06_SwapByte(0xFF);
    MySPI_SPL06_Stop();
}

void SPL06_Write_Reg(uint8_t reg, uint8_t Data) {
    MySPI_SPL06_Start();
    MySPI_SPL06_SwapByte(reg); // 写标志位
    MySPI_SPL06_SwapByte(Data);
    MySPI_SPL06_Stop();
}

uint8_t SPL06_Init(uint8_t *State) {
    uint8_t ID;
    MySPI_SPL06_Init();
	SPL06_Write_Reg(0x0C, 0x09);
	Delay_ms(200);
    SPL06_Read_Reg(0x0D, &ID);
    if (ID != 0x10) {
        *State = 0;
    }
	else
		*State = 1;
 
    // 配置传感器
    SPL06_Write_Reg(0x08, 0x07); // 连续测量模式
	SPL06_Write_Reg(0x06, 0x76); // 压力配置
    SPL06_Write_Reg(0x07, 0xF6); // 温度配置,改完还要改下面的系数
    SPL06_Write_Reg(0x09, 0x0C); // FIFO禁用，压力移位

    return ID;
}

void SPL06_GetData(SPL06_Data *Data) {
    uint8_t  buffer[6];
	int32_t Ori_Pressure,Ori_Tempreture;
    MySPI_SPL06_Start();
    MySPI_SPL06_SwapByte(0x00 | 0x80); // 读数据寄存器，地址自增
    //依次读取数据寄存器
    for (int i = 0; i < 6; i++) {
        buffer[i] = MySPI_SPL06_SwapByte(0xFF);
    }
    MySPI_SPL06_Stop();
    
	
	//组合原始数据
	Ori_Pressure   = (int32_t)((buffer[0] << 16) | (buffer[1] << 8) | buffer[2]); 
	Ori_Tempreture = (int32_t)((buffer[3] << 16) | (buffer[4] << 8) | buffer[5]);
	Ori_Pressure = (Ori_Pressure << 8) >> 8;   // 保留符
	Ori_Tempreture = (Ori_Tempreture << 8) >> 8;	
	//读取校准系数
	int16_t Temp_Config0,Temp_Config1;
	int32_t Pressure_Config00,Pressure_Config10;int16_t Pressure_Config20,
			Pressure_Config30,Pressure_Config01,Pressure_Config11,Pressure_Config21;	
	uint8_t data[18];
	
	MySPI_SPL06_Start();
	MySPI_SPL06_SwapByte(0x10 | 0x80);
	for(int j = 0;j < 18;j++)
	{
		data[j]= MySPI_SPL06_SwapByte(0xFF);
	}
	MySPI_SPL06_Stop();
	
	//温度校准
	Temp_Config0 = (int16_t)((data[0] << 4) | ((data[1] & 0xF0) >> 4));
	Temp_Config0 = (Temp_Config0&0x0800)?(0xF000|Temp_Config0):Temp_Config0;   //补码转换
	Temp_Config1 = (int16_t)((data[2])      | ((data[1] & 0x0F) << 8));	
	Temp_Config1 = (Temp_Config1&0x0800)?(0xF000|Temp_Config1):Temp_Config1;
	
	float Traw_sc = Ori_Tempreture / 1040384.0f;
	Data->Tempreture = Temp_Config0 * 0.5 + Temp_Config1 * Traw_sc;
	
	//压力校准
	
	Pressure_Config00 =  (int32_t)((data[3] <<  12) | ((data[4] << 4) | ((data[5] & 0xF0) >> 4 )));
		Pressure_Config00 = (Pressure_Config00&0x080000)?(0xFFF00000|Pressure_Config00):Pressure_Config00;   //补码转换(C00,C10是16位,在16-20位判断负数)
	Pressure_Config10 =  (int32_t)((data[7]       ) | ((data[6] << 8) | ((data[5] & 0x0F) << 16)));
		Pressure_Config10 = (Pressure_Config10&0x080000)?(0xFFF00000|Pressure_Config10):Pressure_Config10;   //补码转换
	Pressure_Config01 =  (int16_t)((data[8]  << 8 ) | ((data[9])));
		Pressure_Config01 = (Pressure_Config01&0x08000)?(0xFFF00000|Pressure_Config01):Pressure_Config01;   //补码转换
	Pressure_Config11 =  (int16_t)((data[10] << 8 ) | ((data[11])));
		Pressure_Config11 = (Pressure_Config11&0x08000)?(0xFFF00000|Pressure_Config11):Pressure_Config11;   //补码转换	
	Pressure_Config20 =  (int16_t)((data[12] << 8 ) | ((data[13])));
		Pressure_Config20 = (Pressure_Config20&0x08000)?(0xFFF00000|Pressure_Config20):Pressure_Config20;   //补码转换
	Pressure_Config21 =  (int16_t)((data[14] << 8 ) | ((data[15])));
		Pressure_Config21 = (Pressure_Config21&0x08000)?(0xFFF00000|Pressure_Config21):Pressure_Config21;   //补码转换	
	Pressure_Config30 =  (int16_t)((data[16] << 8 ) | ((data[17])));
		Pressure_Config30 = (Pressure_Config30&0x08000)?(0xFFF00000|Pressure_Config30):Pressure_Config30;   //补码转换
	

	float Praw_sc =  Ori_Pressure / 1040384.0f;//改这里的系数
	Data->Pressure = Pressure_Config00 + Praw_sc*(Pressure_Config10 + Praw_sc*(Pressure_Config20 + Praw_sc * Pressure_Config30))+ Traw_sc * Pressure_Config01 + Traw_sc *Praw_sc*(Pressure_Config11+Praw_sc*Pressure_Config21);	
	

	Data->Height = 43300*(1-pow((Data->Pressure/101325),1/5.255));
	
}
