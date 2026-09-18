#ifndef __IMU_H
#define __IMU_H

typedef struct{
	float Pitch;
	float Roll;
	float Yaw;
}Angle;
void IMUInit(ICM42688GyroData *DataGyro);
void IMUupdate(ICM42688AccData *DataAcc,ICM42688GyroData *DataGyro,Angle *angle) ;
#endif
