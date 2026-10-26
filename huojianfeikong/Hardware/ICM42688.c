#include "stm32f10x.h"
#include "MySPI.h"
#include "ICM42688.h"
#include "OLED.h"
#include "Delay.h"

// ==================== 全局变量 ====================
// 原有变量
ICM42688AccData *DataAcc;
ICM42688GyroData *DataGyro;

// 零偏校准变量
ICM42688AccData ICM42688_AccBias;
ICM42688GyroData ICM42688_GyroBias;

// 软件滤波缓冲区
static int16_t accx_filter_buf[IMU_FILTER_WINDOW_SIZE] = {0};
static int16_t accy_filter_buf[IMU_FILTER_WINDOW_SIZE] = {0};
static int16_t accz_filter_buf[IMU_FILTER_WINDOW_SIZE] = {0};
static int16_t gyrox_filter_buf[IMU_FILTER_WINDOW_SIZE] = {0};
static int16_t gyroy_filter_buf[IMU_FILTER_WINDOW_SIZE] = {0};
static int16_t gyroz_filter_buf[IMU_FILTER_WINDOW_SIZE] = {0};
static uint8_t filter_buf_idx = 0;

// 滤波后最终输出数据
ICM42688AccData ICM42688_AccFilt;
ICM42688GyroData ICM42688_GyroFilt;

// ==================== 内部功能函数 ====================
// 滑动平均滤波核心函数
static int16_t IMU_SlideAvgFilter(int16_t raw_data, int16_t *buf)
{
    buf[filter_buf_idx] = raw_data;
    int32_t sum = 0;
    for (uint8_t i = 0; i < IMU_FILTER_WINDOW_SIZE; i++)
    {
        sum += buf[i];
    }
    return (int16_t)(sum / IMU_FILTER_WINDOW_SIZE);
}

// 传感器零偏校准函数（解决静止数据跳变）
void ICM42688_Calibrate(void)
{
    int32_t accx_sum = 0, accy_sum = 0, accz_sum = 0;
    int32_t gyrox_sum = 0, gyroy_sum = 0, gyroz_sum = 0;
    ICM42688AccData acc_temp;
    ICM42688GyroData gyro_temp;

    // 屏幕提示校准
    OLED_Clear();
    OLED_ShowString(1,1,"IMU Calibrating");
    OLED_ShowString(2,1,"Keep Rocket Still!");

    // 多次采样求平均零偏
    for(uint16_t i = 0; i < CALIB_SAMPLE_COUNT; i++)
    {
        ICM42688_Data(&acc_temp, &gyro_temp);
        accx_sum += acc_temp.AccX;
        accy_sum += acc_temp.AccY;
        accz_sum += acc_temp.AccZ;
        gyrox_sum += gyro_temp.GyroX;
        gyroy_sum += gyro_temp.GyroY;
        gyroz_sum += gyro_temp.GyroZ;
        Delay_ms(2);
    }

    // 计算零偏值
    ICM42688_AccBias.AccX = accx_sum / CALIB_SAMPLE_COUNT;
    ICM42688_AccBias.AccY = accy_sum / CALIB_SAMPLE_COUNT;
    ICM42688_AccBias.AccZ = accz_sum / CALIB_SAMPLE_COUNT;
    ICM42688_GyroBias.GyroX = gyrox_sum / CALIB_SAMPLE_COUNT;
    ICM42688_GyroBias.GyroY = gyroy_sum / CALIB_SAMPLE_COUNT;
    ICM42688_GyroBias.GyroZ = gyroz_sum / CALIB_SAMPLE_COUNT;

    // 校准完成提示
    OLED_Clear();
    OLED_ShowString(1,1,"Calib Done!");
    OLED_ShowString(2,1,"Rocket Ready!");
    Delay_ms(1000);
}

// ==================== 原有寄存器操作函数（完全保留） ====================
void ICM42688_Read_Reg(uint8_t reg,uint8_t *Data)
{
	*Data = 0;
	MySPI_ICM42688_Start();
	reg |= 0x80;
	MySPI_ICM42688_SwapByte(reg);
	*Data = MySPI_ICM42688_SwapByte(0xFF);
	MySPI_ICM42688_Stop();
}

void ICM42688_Write_Reg(uint8_t reg,uint8_t Data)
{
	MySPI_ICM42688_Start();
	MySPI_ICM42688_SwapByte(reg);
	MySPI_ICM42688_SwapByte(Data);
	MySPI_ICM42688_Stop();
}

// ==================== 传感器初始化函数（适配火箭优化） ====================
void ICM42688_Init(uint8_t *State)
{
	MySPI_ICM42688_Init();
	
	// 检测传感器是否连接
	uint8_t Time=100,ID = 0;    
	ICM42688_Read_Reg(ICM42688_WHO_AM_I,&ID);
	while(ID != 0x47 && Time>0)
    {
		ICM42688_Read_Reg(ICM42688_WHO_AM_I,&ID);
		Time--;
        Delay_ms(1);
	}
    *State = (ID == 0x47) ? 1 : 0;
	
	if(*State == 1)
	{
		// 软复位
		ICM42688_Write_Reg(ICM42688_DEVICE_CONFIG,0x01);
		Delay_ms(100);

		// 电源配置：开启加速度计+陀螺仪，低噪声模式
		ICM42688_Write_Reg(ICM42688_PWR_MGMT0,0x0F);
		Delay_ms(10);

		// 加速度计配置：±16G量程，1KHz输出
		ICM42688_Write_Reg(ICM42688_ACCEL_CONFIG0, (AFS_SEL << 5) | AODR_SEL);
		// 加速度计硬件滤波：带宽50Hz，滤除发动机高频振动
		ICM42688_Write_Reg(ICM42688_ACCEL_CONFIG1, 0x04);

		// 陀螺仪配置：±1000DPS量程，1KHz输出
		ICM42688_Write_Reg(ICM42688_GYRO_CONFIG0, (GFS_SEL << 5) | GODR_SEL);
		// 陀螺仪硬件滤波：带宽50Hz，减少噪声
		ICM42688_Write_Reg(ICM42688_GYRO_CONFIG1, 0x04);

		Delay_ms(50);
	}
}

// ==================== 数据读取函数（新增零偏扣除+软件滤波） ====================
void ICM42688_Data(ICM42688AccData *DataAcc,ICM42688GyroData *DataGyro)
{
	uint8_t buffer[12];
    
    // 连续读取12字节原始数据
    MySPI_ICM42688_Start();
    MySPI_ICM42688_SwapByte(ICM42688_ACCEL_DATA_X1 | 0x80);
    for(int i=0; i<12; i++) {
        buffer[i] = MySPI_ICM42688_SwapByte(0xFF);
    }
    MySPI_ICM42688_Stop();

    // 解析原始数据
    DataAcc->AccX = (int16_t)((buffer[0] << 8) | buffer[1]);
    DataAcc->AccY = (int16_t)((buffer[2] << 8) | buffer[3]);
    DataAcc->AccZ = (int16_t)((buffer[4] << 8) | buffer[5]);
    DataGyro->GyroX = (int16_t)((buffer[6] << 8) | buffer[7]);
    DataGyro->GyroY = (int16_t)((buffer[8] << 8) | buffer[9]);
    DataGyro->GyroZ = (int16_t)((buffer[10] << 8) | buffer[11]);

    // 扣除零偏
    DataAcc->AccX -= ICM42688_AccBias.AccX;
    DataAcc->AccY -= ICM42688_AccBias.AccY;
    DataAcc->AccZ -= ICM42688_AccBias.AccZ;
    DataGyro->GyroX -= ICM42688_GyroBias.GyroX;
    DataGyro->GyroY -= ICM42688_GyroBias.GyroY;
    DataGyro->GyroZ -= ICM42688_GyroBias.GyroZ;

    // 软件滑动平均滤波
    ICM42688_AccFilt.AccX = IMU_SlideAvgFilter(DataAcc->AccX, accx_filter_buf);
    ICM42688_AccFilt.AccY = IMU_SlideAvgFilter(DataAcc->AccY, accy_filter_buf);
    ICM42688_AccFilt.AccZ = IMU_SlideAvgFilter(DataAcc->AccZ, accz_filter_buf);
    ICM42688_GyroFilt.GyroX = IMU_SlideAvgFilter(DataGyro->GyroX, gyrox_filter_buf);
    ICM42688_GyroFilt.GyroY = IMU_SlideAvgFilter(DataGyro->GyroY, gyroy_filter_buf);
    ICM42688_GyroFilt.GyroZ = IMU_SlideAvgFilter(DataGyro->GyroZ, gyroz_filter_buf);
    
    // 更新缓冲区索引
    filter_buf_idx = (filter_buf_idx + 1) % IMU_FILTER_WINDOW_SIZE;
}

// 原有加速度转换函数（完全保留）
void ACCData_Convert(ICM42688AccData *DataAcc,AccG *Data)
{
    float acc_sensitivity = 2048.0f; // ±16G量程对应的灵敏度：2048 LSB/g
    Data->gx = (float)DataAcc->AccX / acc_sensitivity;
    Data->gy = (float)DataAcc->AccY / acc_sensitivity;
    Data->gz = (float)DataAcc->AccZ / acc_sensitivity;
}

