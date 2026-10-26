#ifndef __SPL06_H
#define __SPL06_H
uint8_t SPL06_Init(uint8_t *State);

typedef struct{
	float Pressure;
	float Tempreture;
	float Height;
}SPL06_Data;
void SPL06_GetData(SPL06_Data *Data);


#endif

