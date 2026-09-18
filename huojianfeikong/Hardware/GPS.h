#ifndef __GPS_H
#define __GPS_H


#include <stdio.h>
extern char GPSSerial_RxPacket[];	
extern uint8_t GPSSerial_RxFlag;

typedef struct {
	uint8_t GPS_State;   //状态
	uint32_t GPS_Latitude1;  //纬度 度
	uint32_t GPS_Latitude2;  //纬度 分
	uint32_t GPS_Longitude1;   //经度 度
	uint32_t GPS_Longitude2;   //经度 分
	uint32_t GPS_Speed1;   //对地速度1
	uint32_t GPS_Speed2;   //对地速度2
	uint8_t GPS_Amount;  //卫星数量  
	uint32_t GPS_UTC;	   //UTC时间
}GPS_Information;


void GPSSerial_Init(void);
void GPSSerial_SendByte(uint8_t Byte);
void GPSSerial_SendArray(uint8_t *Array, uint16_t Length);
void GPSSerial_SendString(char *String);
void GPSSerial_SendNumber(uint32_t Number, uint8_t Length);
void GPSSerial_Printf(char *format, ...);
uint8_t Return(void);
uint8_t GPSSerial_GetRxFlag(void);
uint8_t GPSSerial_GetRxData(void);

void GPS_Init(GPS_Information *Gps);

#endif