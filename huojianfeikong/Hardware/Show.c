#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "ICM42688.h"
#include "IMU.h"
#include "SPL06.h"
#include "Serial.h"
#include "GPS.h"
#include <string.h>


void OLED_Show(uint8_t Count,ICM42688AccData *DataAcc,ICM42688GyroData *DataGyro,Angle *angle,GPS_Information *Gps,SPL06_Data *Data,float Vsp)
{
static uint8_t lastCount = 0xFF; // 初始值设为不可能的值（假设Count范围为0-255）

    // 检查Count是否变化
    if (Count != lastCount) {
        OLED_Clear();       // 变化则清屏
        lastCount = Count;  // 更新为当前的Count
    }
	
		
	if(Count == 0){
			
		memset(OLED_DisplayBuffer, 0, sizeof(OLED_DisplayBuffer));
		// 绘制立方体
        OLED_DrawAttitudeCube( angle->Pitch,angle->Roll, 0);
        
        // 刷新显示
        OLED_Refresh();
		
	}
	else if (Count == 1){
	OLED_ShowString(1, 1, "GPS:");
	OLED_ShowNum(1, 5,Gps->GPS_Amount,2);	
	OLED_ShowString(1, 8, "BJT");		
	OLED_ShowNum(1, 11,Gps->GPS_UTC,6);		
	OLED_ShowNum(2, 2,Gps->GPS_Latitude1,2);
	OLED_ShowChar(2, 4 , '.');
	OLED_ShowChar(2, 11 , 'N');
	OLED_ShowNum(2, 5,Gps->GPS_Latitude2/60,5);//转换为十进制
	OLED_ShowNum(3, 1,Gps->GPS_Longitude1,3);
	OLED_ShowChar(3, 4 , '.');
	OLED_ShowChar(3, 11 , 'E');
	OLED_ShowNum(3, 5,Gps->GPS_Longitude2/60,5);
	OLED_ShowString(4, 1, "Speed:");
	OLED_ShowNum(4, 8,Gps->GPS_Speed1,3);
	OLED_ShowString(4, 12, "km/h");
		
	}
	else if(Count == 2){
	
	OLED_ShowString(1, 2, "Acc");
	OLED_ShowString(1, 10, "Gyro");
	OLED_ShowChar(2, 1 , 'X');		
	OLED_ShowChar(3, 1 , 'Y');		
	OLED_ShowChar(4, 1 , 'Z');		
	OLED_ShowChar(2, 9 , 'X');		
	OLED_ShowChar(3, 9 , 'Y');		
	OLED_ShowChar(4, 9 , 'Z');			
	OLED_ShowSignedNum(2, 3, (DataAcc->AccX *0.0004882887)*100, 4);			
	OLED_ShowSignedNum(3, 3, (DataAcc->AccY *0.0004882887)*100, 4);			
	OLED_ShowSignedNum(4, 3, (DataAcc->AccZ *0.0004882887)*100, 4);			
	OLED_ShowSignedNum(2, 10, DataGyro->GyroX *0.06120325f, 4);			
	OLED_ShowSignedNum(3, 10, DataGyro->GyroY *0.06120325f, 4);	
	OLED_ShowSignedNum(4, 10, DataGyro->GyroZ *0.06120325f, 4);	
		
	}
	
	else if(Count == 3){
	OLED_ShowString(1, 2, "BJT");		
	OLED_ShowNum(1, 6,Gps->GPS_UTC,6);		
	OLED_ShowString(2, 1, "Pitch:");
	OLED_ShowSignedNum(2, 8, angle->Pitch, 3);					
	OLED_ShowString(3, 1, "Roll:");
	OLED_ShowSignedNum(3, 8, angle->Roll, 3);					
	OLED_ShowString(4, 1, "yaw:");
	OLED_ShowSignedNum(4, 8, angle->Yaw ,3 );				

	}
	else
	{
	OLED_ShowString(1, 1, "Pre:");	OLED_ShowString(1, 12, "Pa");
	OLED_ShowString(2, 1, "Tem:");	
	OLED_ShowString(3, 1, "Height:");	
	OLED_ShowNum(1, 6,Data->Pressure , 6);			
	OLED_ShowSignedNum(2, 6,Data->Tempreture , 3);	
	OLED_ShowNum(3, 10,Data->Height ,3);	
	OLED_ShowNum(3, 14,(int)(Data->Height *10)%10 ,1);	
	OLED_ShowChar(3, 13 , '.');	
	OLED_ShowChar(3, 15 , 'm');	
	OLED_ShowString(4, 1 , "Vsp:");	
	OLED_ShowSignedNum(4, 5,Vsp,1);	
	OLED_ShowChar(4, 8 , '.');	
	OLED_ShowNum(4, 9,(int)(Vsp*10)%10,1);		
	OLED_ShowString(4, 11 , "m/s");		
	}

	
}