#ifndef __ICM42688_H
#define __ICM42688_H
// ==================== 寄存器地址定义（完全保留原有定义） ====================
#define ICM42688_DEVICE_CONFIG             0x11
#define ICM42688_DRIVE_CONFIG              0x13
#define ICM42688_INT_CONFIG                0x14
#define ICM42688_FIFO_CONFIG               0x16
#define ICM42688_TEMP_DATA1                0x1D
#define ICM42688_TEMP_DATA0                0x1E
#define ICM42688_ACCEL_DATA_X1             0x1F
#define ICM42688_ACCEL_DATA_X0             0x20
#define ICM42688_ACCEL_DATA_Y1             0x21
#define ICM42688_ACCEL_DATA_Y0             0x22
#define ICM42688_ACCEL_DATA_Z1             0x23
#define ICM42688_ACCEL_DATA_Z0             0x24
#define ICM42688_GYRO_DATA_X1              0x25
#define ICM42688_GYRO_DATA_X0              0x26
#define ICM42688_GYRO_DATA_Y1              0x27
#define ICM42688_GYRO_DATA_Y0              0x28
#define ICM42688_GYRO_DATA_Z1              0x29
#define ICM42688_GYRO_DATA_Z0              0x2A
#define ICM42688_TMST_FSYNCH               0x2B
#define ICM42688_TMST_FSYNCL               0x2C
#define ICM42688_INT_STATUS                0x2D
#define ICM42688_FIFO_COUNTH               0x2E
#define ICM42688_FIFO_COUNTL               0x2F
#define ICM42688_FIFO_DATA                 0x30
#define ICM42688_APEX_DATA0                0x31
#define ICM42688_APEX_DATA1                0x32
#define ICM42688_APEX_DATA2                0x33
#define ICM42688_APEX_DATA3                0x34
#define ICM42688_APEX_DATA4                0x35
#define ICM42688_APEX_DATA5                0x36
#define ICM42688_INT_STATUS2               0x37
#define ICM42688_INT_STATUS3               0x38
#define ICM42688_SIGNAL_PATH_RESET         0x4B
#define ICM42688_INTF_CONFIG0              0x4C
#define ICM42688_INTF_CONFIG1              0x4D
#define ICM42688_PWR_MGMT0                 0x4E
#define ICM42688_GYRO_CONFIG0              0x4F
#define ICM42688_ACCEL_CONFIG0             0x50
#define ICM42688_GYRO_CONFIG1              0x51
#define ICM42688_GYRO_ACCEL_CONFIG0        0x52
#define ICM42688_ACCEL_CONFIG1             0x53
#define ICM42688_TMST_CONFIG               0x54
#define ICM42688_APEX_CONFIG0              0x56
#define ICM42688_SMD_CONFIG                0x57
#define ICM42688_FIFO_CONFIG1              0x5F
#define ICM42688_FIFO_CONFIG2              0x60
#define ICM42688_FIFO_CONFIG3              0x61
#define ICM42688_FSYNC_CONFIG              0x62
#define ICM42688_INT_CONFIG0               0x63
#define ICM42688_INT_CONFIG1               0x64
#define ICM42688_INT_SOURCE0               0x65
#define ICM42688_INT_SOURCE1               0x66
#define ICM42688_INT_SOURCE3               0x68
#define ICM42688_INT_SOURCE4               0x69
#define ICM42688_FIFO_LOST_PKT0            0x6C
#define ICM42688_FIFO_LOST_PKT1            0x6D
#define ICM42688_SELF_TEST_CONFIG          0x70
#define ICM42688_WHO_AM_I                  0x75
#define ICM42688_REG_BANK_SEL              0x76
// ==================== 适配200N火箭的核心参数配置 ====================
// 加速度计量程：±16G，完全覆盖火箭最大11.7G加速度
#define AFS_2G  0x03
#define AFS_4G  0x02
#define AFS_8G  0x01
#define AFS_16G 0x00
#define AFS_SEL AFS_16G
// 陀螺仪量程：±1000DPS，足够火箭姿态控制，噪声更小
#define GFS_2000DPS   0x00
#define GFS_1000DPS   0x01
#define GFS_500DPS    0x02
#define GFS_250DPS    0x03
#define GFS_SEL GFS_1000DPS
// 输出频率ODR：1KHz，平衡响应速度和噪声
#define AODR_1000Hz   0x06
#define GODR_1000Hz   0x06
#define AODR_SEL AODR_1000Hz
#define GODR_SEL GODR_1000Hz
// 防抖滤波与校准配置
#define IMU_FILTER_WINDOW_SIZE  5    // 滑动平均窗口，平衡平滑与响应
#define CALIB_SAMPLE_COUNT      500  // 零偏校准采样次数
// ==================== 结构体定义（完全保留原有） ====================
typedef struct{
	int16_t AccX;
	int16_t AccY;
	int16_t AccZ;
}ICM42688AccData;  
typedef struct{
	float gx;
	float gy;
	float gz;
}AccG;
typedef struct{
	int16_t GyroX;
	int16_t GyroY;
	int16_t GyroZ;
}ICM42688GyroData;  
// ==================== 函数声明 ====================
void ICM42688_Init (uint8_t *State);
void ICM42688_Read_Reg(uint8_t reg,uint8_t *Data);
void ICM42688_Write_Reg(uint8_t reg,uint8_t Data);
void ICM42688_Data(ICM42688AccData *DataAcc,ICM42688GyroData *DataGyro);
void ACCData_Convert(ICM42688AccData *DataAcc,AccG *Data);
void ICM42688_Calibrate(void);
// ==================== 对外全局变量声明 ====================
extern ICM42688AccData ICM42688_AccBias;   // 加速度计零偏
extern ICM42688GyroData ICM42688_GyroBias; // 陀螺仪零偏
extern ICM42688AccData ICM42688_AccFilt;   // 滤波后加速度数据
extern ICM42688GyroData ICM42688_GyroFilt; // 滤波后陀螺仪数据
#endif
