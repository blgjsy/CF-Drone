#include "stm32f10x.h"                  // Device header
#include <string.h>
#include "Delay.h"
#include "OLED.h"
#include "ICM42688.h"
#include "IMU.h"
#include "SPL06.h"
#include "Serial.h"
#include "GPS.h"
#include "Encounter.h"
#include "Show.h"
#include "Servo.h"
#include "Timer.h"
#include "Velocity.h"
#include "Busser.h"
#include "Key.h"
#include "Relay.h"

int main(void)
{
	ICM42688AccData AccData;
	ICM42688GyroData GyroData;
	Angle angle,anglePID;
	GPS_Information GPS;
	SPL06_Data Data_SPL06;
	KalmanFilter kf;

	int8_t Count = 0;
	uint8_t SPL06_State;
	/*模块初始化*/
	OLED_Init();		//OLED初始化
	uint8_t ICM42688_State;
	ICM42688_Init(&ICM42688_State);
	IMUInit(&GyroData);
	Serial_Init();
	GPSSerial_Init( );	
	Encoder_Init();	
	Relay2_Init();
	GPSSerial_SendString("$PCAS10,3*1F\r\n");Delay_ms(100);
	Servo_Init();
	Relay_Init();
	Key_Init();
	SPL06_Init(&SPL06_State);
	TIM3_Configuration();
	TIM4_Configuration(); uint8_t Relay = 0;
	Kalman_Init(&kf, 0.01f, Data_SPL06.Height, 1.5f, 0.001f);
	 Relay_OFF();	 
     Relay2_OFF();//继电器默认关闭
	while (1)      
	{
		if (sample_flag3) {
        sample_flag3 = 0;
		SPL06_GetData(&Data_SPL06);
		ICM42688_Data(&AccData,&GyroData);
//	Serial_Printf("%.2f,%.2f\r\n",angle.Pitch, AccData.AccZ*0.0004882887f*9.8);
			
		IMUupdate(&AccData,&GyroData,&angle);

		if(GPSSerial_RxFlag == 1)
		{
			GPS_Init(&GPS);
			GPSSerial_RxFlag = 0;	//
		}

		Count = Key_Count(Key_GetNum());	//
//		if(Count>4) {Count=0;}
//		    
//		else if(Count<0) {Count=4;}	
        
		float a_z = AccData.AccZ *0.0004882887; 
        // 读取气压高度
        float h_baro = Data_SPL06.Height;  
        
        // 卡尔曼滤波
        Kalman_Predict(&kf, a_z);
        Kalman_Update(&kf, h_baro);
		
//		else {Busser_Off();}	
	
		Canard_Init(&angle,&anglePID);
		Canard_ON(&anglePID);
//		Servo_SetAngle1(10);Servo_SetAngle2(10);Servo_SetAngle3(10);Servo_SetAngle4(10);
        // 输出结果
       
		OLED_Show(Count,&AccData,&GyroData,&angle,&GPS,&Data_SPL06,kf.v);	

		}
		if(Serial_GetRxData()=='?')
			Relay2_ON(); //发送?打开继电器2
		
		 if (sample_flag4) {
			 sample_flag4  = 0;
			 Relay++;	
			 if(Relay>=50)  //延时5s
			 {
				 if (kf.v<-10){  //检测到火箭下降,速度10m/s
					Relay_ON(); //开伞

				 }
			 }
//			 Serial_Printf("%d,%d\r\n",GPS.GPS_Latitude2/60,GPS.GPS_Longitude2/60);
			 Serial_Printf("%.2f,%.1f,%.1f,%.1f,%.1f,%d,%d,%d\r\n",Data_SPL06.Height,kf.v,angle.Pitch,angle.Roll,angle.Yaw,GPS.GPS_Latitude2/60,GPS.GPS_Longitude2/60,GPS.GPS_Speed1);
		 }
	}