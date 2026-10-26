//#ifndef __OLED_H
//#define __OLED_H

//void OLED_Init(void);
//void OLED_Clear(void);
//void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
//void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
//void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
//void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
//void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
//void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);



//#endif

#ifndef __OLED_H
#define __OLED_H

#define OLED_PAGE_NUM 8
#define OLED_WIDTH 128

extern uint8_t OLED_DisplayBuffer[OLED_PAGE_NUM][OLED_WIDTH]; // 完整维度声明
extern const uint8_t OLED_F8x16[][16];

/*引脚初始化*/
void OLED_I2C_Init(void);
void OLED_I2C_Start(void);

void OLED_I2C_Stop(void);

void OLED_I2C_SendByte(uint8_t Byte);

void OLED_WriteCommand(uint8_t Command);

void OLED_WriteData(uint8_t Data);

void OLED_SetCursor(uint8_t Y, uint8_t X);

void OLED_Refresh(void);

void OLED_Clear(void);

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);

uint32_t OLED_Pow(uint32_t X, uint32_t Y);

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);


void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);


void OLED_Init(void);


void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);




void ComputeRotationMatrix(float roll, float pitch, float yaw, float mat[3][3]);


void OLED_DrawAttitudeCube(float roll, float pitch, float yaw);
#endif