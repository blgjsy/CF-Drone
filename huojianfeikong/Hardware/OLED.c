#include "stm32f10x.h"
#include "OLED_Font.h"
#include "math.h"
#include <string.h>  // 解决memset警告
#include <stdlib.h>  // 解决abs警告

#include "OLED.h"
/*引脚配置*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_8, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_9, (BitAction)(x))
#define OLED_HEIGHT   64
uint8_t OLED_DisplayBuffer[OLED_PAGE_NUM][OLED_WIDTH]; // 完整维度声明
/*引脚初始化*/
void OLED_I2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  * @param  无
  * @retval 无
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C停止
  * @param  无
  * @retval 无
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节
  * @param  Byte 要发送的一个字节
  * @retval 无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(Byte & (0x80 >> i));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}
void OLED_Refresh(void)
{
    for(uint8_t page=0; page<OLED_PAGE_NUM; page++){
        OLED_SetCursor(page, 0);
        for(uint16_t col=0; col<OLED_WIDTH; col++){
            OLED_WriteData(OLED_DisplayBuffer[page][col]);
        }
    }
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{  
    memset(OLED_DisplayBuffer, 0, sizeof(OLED_DisplayBuffer));
    OLED_Refresh();
}

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	}
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)			//上电延时
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			//端口初始化
	
	OLED_WriteCommand(0xAE);	//关闭显示
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	//设置显示开始行
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/倒转显示

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//开启显示
		
	OLED_Clear();				//OLED清屏
}



void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    
    if(color) {
        OLED_DisplayBuffer[page][x] |=  (1 << bit);
    } else {
        OLED_DisplayBuffer[page][x] &= ~(1 << bit);
    }
}




// 立方体参数
typedef struct {
    float vertices[8][3];    // 立方体顶点
    uint8_t edges[12][2];    // 边连接关系
    float scale;             // 缩放系数
    int16_t offsetX;         // X偏移
    int16_t offsetY;         // Y偏移
} Cube_TypeDef;

// 初始化立方体结构体
Cube_TypeDef Cube = {
    .vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    },
    .edges = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    },
    .scale = 20.0f,
    .offsetX = OLED_WIDTH/2,
    .offsetY = OLED_HEIGHT/2
};


void ComputeRotationMatrix(float roll, float pitch, float yaw, float mat[3][3])
{
    float phi = roll * 0.0174533f;   // X轴旋转
    float theta = pitch * 0.0174533f;// Y轴旋转 
    float psi = yaw * 0.0174533f;    // Z轴旋转

    float cph = cosf(phi), sph = sinf(phi);
    float cth = cosf(theta), sth = sinf(theta);
    float cps = cosf(psi), sps = sinf(psi);

    mat[0][0] = cth * cps;
    mat[0][1] = sph * sth * cps - cph * sps;
    mat[0][2] = cph * sth * cps + sph * sps;
    
    mat[1][0] = cth * sps;
    mat[1][1] = sph * sth * sps + cph * cps;
    mat[1][2] = cph * sth * sps - sph * cps;
    
    mat[2][0] = -sth;
    mat[2][1] = sph * cth;
    mat[2][2] = cph * cth;
}


void OLED_DrawAttitudeCube(float roll, float pitch, float yaw)
{
    float rotMat[3][3];
    int16_t projX[8], projY[8];
    
    // 计算旋转矩阵
    ComputeRotationMatrix(roll, pitch, yaw, rotMat);

    // 顶点投影
    for(uint8_t i=0; i<8; i++){
        float x = Cube.vertices[i][0];
        float y = Cube.vertices[i][1];
        float z = Cube.vertices[i][2];
        
        // 应用旋转
        float xr = rotMat[0][0]*x + rotMat[0][1]*y + rotMat[0][2]*z;
        float yr = rotMat[1][0]*x + rotMat[1][1]*y + rotMat[1][2]*z;
        float zr = rotMat[2][0]*x + rotMat[2][1]*y + rotMat[2][2]*z;
        
        // 正交投影
        projX[i] = Cube.offsetX + (int16_t)(xr * Cube.scale);
        projY[i] = Cube.offsetY - (int16_t)(yr * Cube.scale); // Y轴翻转
    }

    // Bresenham画线算法
    for(uint8_t e=0; e<12; e++){
        uint8_t start = Cube.edges[e][0];
        uint8_t end = Cube.edges[e][1];
        
        int16_t x0 = projX[start], y0 = projY[start];
        int16_t x1 = projX[end], y1 = projY[end];
        
        int16_t dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
        int16_t dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; 
        int16_t err = (dx>dy ? dx : -dy)/2, e2;
        
        while(1){
            OLED_DrawPoint(x0, y0, 1);
            if(x0==x1 && y0==y1) break;
            e2 = err;
            if(e2 >-dx) { err -= dy; x0 += sx; }
            if(e2 < dy) { err += dx; y0 += sy; }
        }
    }
}
