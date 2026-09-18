#ifndef __MYSPI_H
#define __MYSPI_H

#include "stm32f10x.h"                  // Device header
void MySPI_ICM42688_W_SS(uint8_t BitValue);
void MySPI_ICM42688_Init(void);
void MySPI_ICM42688_Start(void);
void MySPI_ICM42688_Stop(void);
uint8_t MySPI_ICM42688_SwapByte(uint8_t ByteSend);



void MySPI_SPL06_W_SS(uint8_t BitValue);
void MySPI_SPL06_Init(void);
void MySPI_SPL06_Start(void);
void MySPI_SPL06_Stop(void);
uint8_t MySPI_SPL06_SwapByte(uint8_t ByteSend);
#endif
