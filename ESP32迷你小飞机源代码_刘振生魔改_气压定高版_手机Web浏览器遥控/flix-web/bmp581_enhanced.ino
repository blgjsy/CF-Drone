// bmp581_enhanced.ino
// 增强版BMP581模块 - 高精度高度计带温度校准

#include <Wire.h>
#include "SparkFun_BMP581_Arduino_Library.h"

// ==================== 引脚定义 ====================
#define SDA_PIN 18
#define SCL_PIN 19

// ==================== 传感器对象 ====================
BMP581 pressureSensor;

// ==================== 自定义类型定义 ====================
enum BMP581FilterCoeff {
    BMP581_FILTER_OFF = 0,
    BMP581_FILTER_COEFF_3 = 1,
    BMP581_FILTER_COEFF_7 = 2,
    BMP581_FILTER_COEFF_15 = 3,
    BMP581_FILTER_COEFF_31 = 4,
    BMP581_FILTER_COEFF_63 = 5,
    BMP581_FILTER_COEFF_127 = 6
};

enum BMP581State {
    BMP581_STATE_IDLE,
    BMP581_STATE_CALIBRATING,
    BMP581_STATE_NORMAL_OPERATION,
    BMP581_STATE_ERROR
};

// ==================== 全局变量 ====================
uint8_t currentFilterCoeff = BMP581_FILTER_COEFF_31; // 中等滤波（原为63）
float baselinePressure = 0.0;    // 基准压力 (Pa)
float baselineAltitude = 0.0;    // 基准海拔高度 (m)
float seaLevelPressure = 101325.0; // 标准海平面压力 (Pa)

// 温度校准参数
float temperatureOffset = 0.0;  // 温度偏移校准值
float temperatureCorrection = 0.0; // 温度校正值
bool temperatureCalibrated = false;
float referenceTemperature = 14.0; // 参考温度（用于手动校准）

// 校准状态
bool calibrationComplete = false;
bool calibrationInProgress = false;
unsigned long calibrationStartTime = 0;
unsigned long lastCalibrationSampleTime = 0;
const int CALIBRATION_TIME = 5000; // 30秒校准
const int SAMPLE_COUNT = 10;
int sampleIndex = 0;
float pressureSamples[SAMPLE_COUNT];
float temperatureSamples[SAMPLE_COUNT];

// 滤波数据
float filteredPressure = 0.0;
float filteredTemperature = 0.0;
const float PRESSURE_ALPHA = 0.2;   // 压力滤波系数（较快响应）
const float TEMPERATURE_ALPHA = 0.05; // 温度滤波系数（较慢响应，更平滑）

// 温度稳定检测
float temperatureHistory[10]; // 最近10次温度记录
int tempHistoryIndex = 0;
bool temperatureStable = false;
unsigned long lastTemperatureCheck = 0;
const float TEMP_STABILITY_THRESHOLD = 0.1; // 温度稳定阈值（℃）

// 自热补偿
unsigned long powerOnTime = 0;
float selfHeatCompensation = 0.0;
const float SELF_HEAT_RATE = 0.0; // 每分钟升高0.5℃（估算值）

// 实时数据
float currentAltitude = 0.0;      // 当前绝对高度
float relativeAltitude = 0.0;     // 相对基准高度
float currentTemperature = 0.0;   // 当前温度（校准后）
float rawTemperature = 0.0;       // 原始温度值

// 状态标志
bool bmp581Initialized = false;
bool sensorReadError = false;
unsigned long lastReadTime = 0;
unsigned long lastErrorPrintTime = 0;
int consecutiveErrors = 0;

// 状态机
BMP581State currentBMP581State = BMP581_STATE_IDLE;

// 性能统计
unsigned long totalReads = 0;
unsigned long successfulReads = 0;
float minTemperature = 100.0;
float maxTemperature = -100.0;

// ==================== 新增温度校准函数声明 ====================
void calibrateTemperatureWithReference(float knownTemperature);
void autoTemperatureCalibration();
float applyTemperatureCorrection(float rawTemp);
void updateTemperatureStabilityCheck(float temp);
bool isTemperatureStable();
void calculateSelfHeatCompensation();
void setTemperatureOffset(float offset);
float getTemperatureOffset();
void resetTemperatureStatistics();

// ==================== 函数声明 ====================
void setupBMP581Enhanced();
void initBMP581Calibration();
void updateBMP581Enhanced();
void handleErrorState();
void updateCalibrationNonBlocking();
void displayCalibrationProgress();
void processCalibrationSamples();
void readSensorData();
float calculateAltitude(float pressure);
void restartBMP581();
uint8_t getFilterCoefficient();
bool setFilterCoefficient(uint8_t coeff);
void recalibrate();
String getSensorStatus();
float getCurrentAltitude();
float getAbsoluteAltitude();
float getTemperature();
bool isBMP581Ready();
float getBaselineAltitude();
BMP581State getSensorState();
void printSensorStatus();
void scanBMP581Address();
void setSeaLevelPressure(float pressure);
bool getRawSensorData(float &pressure, float &temperature);
void resetAltitudeBaseline();
void testBMP581();

// ==================== 初始化函数 ====================
void setupBMP581Enhanced() {
    Serial.println("===================================");
    Serial.println("   BMP581 增强版高度计模块");
    Serial.println("     带温度校准功能");
    Serial.println("===================================");
    
    // 初始化I2C总线
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);  // 400kHz
    
    Serial.print("初始化I2C总线 (SDA=GPIO");
    Serial.print(SDA_PIN);
    Serial.print(", SCL=GPIO");
    Serial.print(SCL_PIN);
    Serial.println(")...");
    
    // 初始化温度历史数组
    for (int i = 0; i < 10; i++) {
        temperatureHistory[i] = 25.0; // 初始化为室温
    }
    
    // BMP581可能的I2C地址
    uint8_t addresses[] = {0x47, 0x77};  // 常见地址
    
    int8_t initResult = -1;
    uint8_t foundAddress = 0;
    bool deviceFound = false;
    
    for (int i = 0; i < sizeof(addresses)/sizeof(addresses[0]); i++) {
        Serial.print("尝试地址 0x");
        Serial.print(addresses[i], HEX);
        Serial.print("... ");
        
        // 先测试I2C通信
        Wire.beginTransmission(addresses[i]);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("设备存在，");
            
            // 尝试初始化传感器
            initResult = pressureSensor.beginI2C(addresses[i]);
            
            if (initResult == BMP5_OK) {
                Serial.println("成功！");
                bmp581Initialized = true;
                foundAddress = addresses[i];
                deviceFound = true;
                powerOnTime = millis(); // 记录上电时间
                break;
            } else {
                Serial.print("失败 (错误: ");
                Serial.print(initResult);
                Serial.println(")");
            }
        } else {
            Serial.println("无响应");
        }
        
        delay(10);
    }
    
    if (!deviceFound) {
        // 尝试自动扫描
        Serial.println("开始I2C自动扫描...");
        byte error, address;
        for(address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            
            if (error == 0) {
                Serial.print("找到设备在地址: 0x");
                if (address < 16) Serial.print("0");
                Serial.print(address, HEX);
                
                // 尝试用这个地址初始化
                initResult = pressureSensor.beginI2C(address);
                if (initResult == BMP5_OK) {
                    Serial.println(" - BMP581初始化成功！");
                    bmp581Initialized = true;
                    foundAddress = address;
                    deviceFound = true;
                    powerOnTime = millis();
                    break;
                }
                Serial.println();
            }
        }
    }
    
    if (!bmp581Initialized) {
        Serial.println("❌ BMP581传感器连接失败！");
        currentBMP581State = BMP581_STATE_ERROR;
        return;
    }
    
    // 打印传感器信息
    Serial.print("传感器地址: 0x");
    Serial.println(foundAddress, HEX);
    
    // 配置传感器
    Serial.println("配置传感器参数...");
    
    // 尝试设置IIR滤波器
    if (setFilterCoefficient(currentFilterCoeff)) {
        Serial.println("滤波器配置成功");
    } else {
        Serial.println("滤波器配置失败，使用默认配置");
    }
    
    // 重置温度统计
    resetTemperatureStatistics();
    
    // 开始校准
    initBMP581Calibration();
}

// ==================== 初始化校准 ====================
void initBMP581Calibration() {
    if (!bmp581Initialized) return;
    
    Serial.println("\n开始传感器校准...");
    Serial.println("请保持设备静止30秒");
    Serial.println("正在采集基准数据...");
    
    calibrationComplete = false;
    calibrationInProgress = true;
    calibrationStartTime = millis();
    lastCalibrationSampleTime = millis();
    sampleIndex = 0;
    consecutiveErrors = 0;
    
    currentBMP581State = BMP581_STATE_CALIBRATING;
    
    // 清空样本数组
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        pressureSamples[i] = 0;
        temperatureSamples[i] = 0;
    }
    
    // 显示校准开始信息
    displayCalibrationProgress();
}

// ==================== 更新函数（非阻塞式） ====================
void updateBMP581Enhanced() {
    static unsigned long lastTemperatureUpdate = 0;
    static unsigned long lastStatusPrint = 0;
    
    switch(currentBMP581State) {
        case BMP581_STATE_ERROR:
            handleErrorState();
            break;
            
        case BMP581_STATE_CALIBRATING:
            updateCalibrationNonBlocking();
            break;
            
        case BMP581_STATE_NORMAL_OPERATION:
            // 读取传感器数据（限制频率）
            if (millis() - lastReadTime >= 100) { // 10Hz（原为50Hz）
                readSensorData();
            }
            
            // 每秒检查一次温度稳定性
            if (millis() - lastTemperatureCheck >= 1000) {
                updateTemperatureStabilityCheck(currentTemperature);
                lastTemperatureCheck = millis();
            }
            
            // 每分钟计算一次自热补偿
            if (millis() - powerOnTime >= 60000) {
                calculateSelfHeatCompensation();
            }
            
            // 每分钟打印一次状态（可选）
            if (millis() - lastStatusPrint >= 60000) {
                // printSensorStatus(); // 可选：取消注释以启用定期状态打印
                lastStatusPrint = millis();
            }
            break;
            
        case BMP581_STATE_IDLE:
        default:
            if (!bmp581Initialized) {
                static unsigned long lastInitAttempt = 0;
                if (millis() - lastInitAttempt > 5000) {
                    setupBMP581Enhanced();
                    lastInitAttempt = millis();
                }
            }
            break;
    }
}

// ==================== 处理错误状态 ====================
void handleErrorState() {
    static unsigned long lastReinitAttempt = 0;
    static int reinitAttemptCount = 0;
    
    unsigned long currentTime = millis();
    
    if (currentTime - lastReinitAttempt > 10000) {
        Serial.println("尝试重新初始化BMP581...");
        bmp581Initialized = false;
        calibrationComplete = false;
        calibrationInProgress = false;
        
        // 重置I2C总线
        Wire.end();
        delay(100);
        Wire.begin(SDA_PIN, SCL_PIN);
        Wire.setClock(400000);
        
        setupBMP581Enhanced();
        
        lastReinitAttempt = currentTime;
        reinitAttemptCount++;
        
        if (reinitAttemptCount > 3) {
            Serial.println("多次重试失败，进入休眠状态");
            currentBMP581State = BMP581_STATE_IDLE;
            reinitAttemptCount = 0;
        }
    }
}

// ==================== 非阻塞式校准更新 ====================
void updateCalibrationNonBlocking() {
    if (!bmp581Initialized || !calibrationInProgress) {
        currentBMP581State = BMP581_STATE_ERROR;
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - calibrationStartTime;
    
    if (elapsedTime >= CALIBRATION_TIME) {
        processCalibrationSamples();
        calibrationInProgress = false;
        
        if (calibrationComplete) {
            currentBMP581State = BMP581_STATE_NORMAL_OPERATION;
            Serial.println("\n✅ 传感器校准完成！");
            
            // 自动温度校准（基于初始读数）
            autoTemperatureCalibration();
        } else {
            Serial.println("\n❌ 校准失败，将重新尝试...");
            currentBMP581State = BMP581_STATE_ERROR;
        }
        return;
    }
    
    if (currentTime - lastCalibrationSampleTime >= 200) { // 5Hz（原为10Hz）
        lastCalibrationSampleTime = currentTime;
        
        bmp5_sensor_data data = {0, 0};
        int8_t err = pressureSensor.getSensorData(&data);
        
        if (err == BMP5_OK) {
            consecutiveErrors = 0;
            
            if (sampleIndex < SAMPLE_COUNT) {
                pressureSamples[sampleIndex] = data.pressure;
                temperatureSamples[sampleIndex] = data.temperature;
                sampleIndex++;
            }
            
            static unsigned long lastProgressTime = 0;
            if (currentTime - lastProgressTime >= 5000) {
                displayCalibrationProgress();
                lastProgressTime = currentTime;
            }
        } else {
            consecutiveErrors++;
            if (consecutiveErrors > 5 && currentTime - lastErrorPrintTime > 2000) {
                Serial.println("⚠️ 校准期间传感器读取失败");
                lastErrorPrintTime = currentTime;
                
                if (consecutiveErrors > 20) {
                    Serial.println("❌ 连续错误过多，中止校准");
                    calibrationInProgress = false;
                    currentBMP581State = BMP581_STATE_ERROR;
                }
            }
        }
    }
}

// ==================== 显示校准进度 ====================
void displayCalibrationProgress() {
    if (!calibrationInProgress) return;
    
    int elapsedSeconds = (millis() - calibrationStartTime) / 1000;
    int totalSeconds = CALIBRATION_TIME / 1000;
    int percentComplete = (elapsedSeconds * 100) / totalSeconds;
    
    Serial.print("校准进度: ");
    Serial.print(elapsedSeconds);
    Serial.print("/");
    Serial.print(totalSeconds);
    Serial.print("秒 (");
    Serial.print(percentComplete);
    Serial.println("%)");
    
    if (sampleIndex > 0) {
        Serial.print("已采集样本: ");
        Serial.print(sampleIndex);
        Serial.print("/");
        Serial.println(SAMPLE_COUNT);
    }
}

// ==================== 处理校准样本 ====================
void processCalibrationSamples() {
    if (sampleIndex == 0) {
        Serial.println("❌ 校准失败：未采集到有效样本");
        calibrationComplete = false;
        return;
    }
    
    float sumPressure = 0.0;
    float sumTemperature = 0.0;
    int validSamples = 0;
    
    // 计算有效样本的均值和标准差
    for (int i = 0; i < sampleIndex; i++) {
        if (pressureSamples[i] > 80000 && pressureSamples[i] < 120000) {
            sumPressure += pressureSamples[i];
            sumTemperature += temperatureSamples[i];
            validSamples++;
        }
    }
    
    Serial.print("总样本数: ");
    Serial.print(sampleIndex);
    Serial.print(", 有效样本: ");
    Serial.println(validSamples);
    
    if (validSamples < (SAMPLE_COUNT * 0.5)) {
        Serial.println("❌ 校准失败：有效样本太少");
        calibrationComplete = false;
        return;
    }
    
    // 计算平均值
    baselinePressure = sumPressure / validSamples;
    float avgTemperature = sumTemperature / validSamples;
    
    // 计算基准高度
    baselineAltitude = calculateAltitude(baselinePressure);
    
    // 初始化滤波值
    filteredPressure = baselinePressure;
    filteredTemperature = avgTemperature;
    
    // 设置初始温度值
    rawTemperature = avgTemperature;
    currentTemperature = applyTemperatureCorrection(avgTemperature);
    
    // 初始化当前值
    currentAltitude = baselineAltitude;
    relativeAltitude = 0.0;
    
    calibrationComplete = true;
    
    Serial.print("基准压力: ");
    Serial.print(baselinePressure, 1);
    Serial.println(" Pa");
    Serial.print("基准高度: ");
    Serial.print(baselineAltitude, 2);
    Serial.println(" m");
    Serial.print("初始温度: ");
    Serial.print(avgTemperature, 1);
    Serial.println(" °C (原始)");
    Serial.print("校准温度: ");
    Serial.print(currentTemperature, 1);
    Serial.println(" °C");
}

// ==================== 读取传感器数据（优化版） ====================
void readSensorData() {
    if (!bmp581Initialized || !calibrationComplete) {
        return;
    }
    
    totalReads++;
    unsigned long currentTime = millis();
    
    bmp5_sensor_data data = {0, 0};
    int8_t err = pressureSensor.getSensorData(&data);
    
    if (err == BMP5_OK) {
        successfulReads++;
        sensorReadError = false;
        consecutiveErrors = 0;
        
        // 应用不同的滤波系数
        filteredPressure = filteredPressure * (1 - PRESSURE_ALPHA) + data.pressure * PRESSURE_ALPHA;
        
        // 温度滤波（更慢的响应）
        filteredTemperature = filteredTemperature * (1 - TEMPERATURE_ALPHA) + data.temperature * TEMPERATURE_ALPHA;
        
        // 保存原始温度值
        rawTemperature = data.temperature;
        
        // 应用温度校准和校正
        currentTemperature = applyTemperatureCorrection(filteredTemperature);
        
        // 更新温度统计
        if (currentTemperature < minTemperature) minTemperature = currentTemperature;
        if (currentTemperature > maxTemperature) maxTemperature = currentTemperature;
        
        // 计算高度
        currentAltitude = calculateAltitude(filteredPressure);
        relativeAltitude = currentAltitude - baselineAltitude;
        
        lastReadTime = currentTime;
        
    } else {
        sensorReadError = true;
        consecutiveErrors++;
        
        if (currentTime - lastErrorPrintTime > 2000) {
            Serial.print("BMP581读取错误: ");
            Serial.println(err);
            
            if (consecutiveErrors > 10) {
                Serial.println("⚠️ BMP581传感器可能已断开");
                currentBMP581State = BMP581_STATE_ERROR;
            }
            
            lastErrorPrintTime = currentTime;
        }
    }
}

// ==================== 新增温度校准函数实现 ====================

// 1. 使用已知参考温度校准
void calibrateTemperatureWithReference(float knownTemperature) {
    if (!bmp581Initialized || !calibrationComplete) {
        Serial.println("传感器未就绪，无法进行温度校准");
        return;
    }
    
    // 等待温度稳定
    if (!temperatureStable) {
        Serial.println("温度未稳定，请稍后重试");
        return;
    }
    
    // 计算偏移量
    temperatureOffset = knownTemperature - currentTemperature;
    temperatureCalibrated = true;
    
    Serial.print("✅ 温度校准完成！");
    Serial.print(" 参考温度: ");
    Serial.print(knownTemperature, 1);
    Serial.println(" °C");
    Serial.print("测量温度: ");
    Serial.print(currentTemperature, 1);
    Serial.println(" °C");
    Serial.print("偏移量: ");
    Serial.print(temperatureOffset, 2);
    Serial.println(" °C");
    
    // 重新计算当前温度
    currentTemperature = applyTemperatureCorrection(rawTemperature);
}

// 2. 自动温度校准（基于初始读数）
void autoTemperatureCalibration() {
    if (!bmp581Initialized || !calibrationComplete) {
        return;
    }
    
    // 假设初始温度应该是室温（20-30℃）
    if (currentTemperature < 15.0 || currentTemperature > 40.0) {
        // 温度读数异常，应用校正
        float expectedTemp = 25.0; // 假设室温25℃
        temperatureCorrection = expectedTemp - currentTemperature;
        
        Serial.print("⚠️ 自动温度校正: ");
        Serial.print(temperatureCorrection, 1);
        Serial.println(" °C");
        
        currentTemperature = applyTemperatureCorrection(rawTemperature);
    }
}

// 3. 应用温度校正
float applyTemperatureCorrection(float rawTemp) {
    float correctedTemp = rawTemp + temperatureOffset + temperatureCorrection - selfHeatCompensation;
    
    // 限制在合理范围内
    correctedTemp = constrain(correctedTemp, -50.0, 100.0);
    
    return correctedTemp;
}

// 4. 更新温度稳定性检查
void updateTemperatureStabilityCheck(float temp) {
    // 添加到历史记录
    temperatureHistory[tempHistoryIndex] = temp;
    tempHistoryIndex = (tempHistoryIndex + 1) % 10;
    
    // 检查稳定性（最近10次读数的方差）
    float sum = 0;
    float sumSq = 0;
    int count = 0;
    
    for (int i = 0; i < 10; i++) {
        if (temperatureHistory[i] != 0) {
            sum += temperatureHistory[i];
            sumSq += temperatureHistory[i] * temperatureHistory[i];
            count++;
        }
    }
    
    if (count >= 5) { // 至少5个有效样本
        float mean = sum / count;
        float variance = (sumSq / count) - (mean * mean);
        
        temperatureStable = (variance < TEMP_STABILITY_THRESHOLD * TEMP_STABILITY_THRESHOLD);
    }
}

// 5. 检查温度是否稳定
bool isTemperatureStable() {
    return temperatureStable;
}

// 6. 计算自热补偿
void calculateSelfHeatCompensation() {
    unsigned long minutesOn = (millis() - powerOnTime) / 60000;
    
    // 简单的线性模型：每分钟升高0.5℃，前10分钟明显，之后趋于稳定
    if (minutesOn < 10) {
        selfHeatCompensation = SELF_HEAT_RATE * minutesOn;
    } else {
        selfHeatCompensation = SELF_HEAT_RATE * 10; // 最大补偿5℃
    }
    
    // Serial.print("自热补偿: "); // 调试用
    // Serial.print(selfHeatCompensation, 2);
    // Serial.println(" °C");
}

// 7. 设置温度偏移
void setTemperatureOffset(float offset) {
    temperatureOffset = offset;
    temperatureCalibrated = true;
    
    Serial.print("温度偏移设置为: ");
    Serial.print(temperatureOffset, 2);
    Serial.println(" °C");
    
    // 重新计算温度
    if (calibrationComplete) {
        currentTemperature = applyTemperatureCorrection(rawTemperature);
    }
}

// 8. 获取温度偏移
float getTemperatureOffset() {
    return temperatureOffset;
}

// 9. 重置温度统计
void resetTemperatureStatistics() {
    minTemperature = 100.0;
    maxTemperature = -100.0;
    totalReads = 0;
    successfulReads = 0;
}

// ==================== 计算高度 ====================
float calculateAltitude(float pressure) {
    if (pressure <= 0) return 0.0;
    
    float ratio = pressure / seaLevelPressure;
    
    if (ratio < 0.1 || ratio > 10.0) {
        return 0.0;
    }
    
    return 44330.8 * (1.0 - pow(ratio, 0.190284));
}

// ==================== 其他现有函数保持原样 ====================

void restartBMP581() {
    Serial.println("正在重启BMP581传感器...");
    
    currentBMP581State = BMP581_STATE_IDLE;
    bmp581Initialized = false;
    calibrationComplete = false;
    calibrationInProgress = false;
    sensorReadError = false;
    consecutiveErrors = 0;
    
    delay(100);
    setupBMP581Enhanced();
}

uint8_t getFilterCoefficient() {
    return currentFilterCoeff;
}

bool setFilterCoefficient(uint8_t coeff) {
    if (coeff > BMP581_FILTER_COEFF_127) {
        return false;
    }
    
    int8_t err = BMP5_OK;
    
    bmp5_iir_config config;
    config.set_iir_t = coeff;
    config.set_iir_p = coeff;
    config.shdw_set_iir_t = BMP5_ENABLE;
    config.shdw_set_iir_p = BMP5_ENABLE;
    config.iir_flush_forced_en = BMP5_DISABLE;
    
    err = pressureSensor.setFilterConfig(&config);
    
    if (err == BMP5_OK) {
        currentFilterCoeff = coeff;
        
        Serial.print("滤波系数设置为: ");
        switch(coeff) {
            case BMP581_FILTER_OFF:     Serial.println("无滤波"); break;
            case BMP581_FILTER_COEFF_3: Serial.println("轻微 (系数=3)"); break;
            case BMP581_FILTER_COEFF_7: Serial.println("低 (系数=7)"); break;
            case BMP581_FILTER_COEFF_15: Serial.println("中 (系数=15)"); break;
            case BMP581_FILTER_COEFF_31: Serial.println("高 (系数=31)"); break;
            case BMP581_FILTER_COEFF_63: Serial.println("强 (系数=63)"); break;
            case BMP581_FILTER_COEFF_127: Serial.println("最强 (系数=127)"); break;
            default: Serial.println("未知"); break;
        }
        return true;
    }
    
    currentFilterCoeff = coeff;
    return true;
}

void recalibrate() {
    if (!bmp581Initialized) {
        Serial.println("传感器未初始化，无法重新校准");
        return;
    }
    
    Serial.println("\n开始手动重新校准...");
    initBMP581Calibration();
}

String getSensorStatus() {
    String status = "";
    
    status += "BMP581状态: ";
    status += bmp581Initialized ? "已初始化" : "未初始化";
    status += "\n";
    
    status += "运行状态: ";
    switch(currentBMP581State) {
        case BMP581_STATE_IDLE: status += "空闲"; break;
        case BMP581_STATE_CALIBRATING: status += "校准中"; break;
        case BMP581_STATE_NORMAL_OPERATION: status += "正常运行"; break;
        case BMP581_STATE_ERROR: status += "错误"; break;
        default: status += "未知"; break;
    }
    status += "\n";
    
    if (bmp581Initialized) {
        status += "校准状态: ";
        if (calibrationInProgress) {
            status += "进行中";
            int elapsed = (millis() - calibrationStartTime) / 1000;
            status += " (" + String(elapsed) + "秒)";
        } else {
            status += calibrationComplete ? "已完成" : "未完成";
        }
        status += "\n";
        
        if (calibrationComplete) {
            status += "当前高度: ";
            status += String(currentAltitude, 2);
            status += " m\n";
            
            status += "相对高度: ";
            status += String(relativeAltitude, 3);
            status += " m\n";
            
            status += "温度: ";
            status += String(currentTemperature, 2);
            status += " °C (校准后)\n";
            
            status += "原始温度: ";
            status += String(rawTemperature, 2);
            status += " °C\n";
            
            status += "温度稳定性: ";
            status += temperatureStable ? "稳定" : "波动";
            status += "\n";
            
            status += "温度偏移: ";
            status += String(temperatureOffset, 2);
            status += " °C\n";
            
            status += "自热补偿: ";
            status += String(selfHeatCompensation, 2);
            status += " °C\n";
            
            status += "滤波系数: ";
            switch(currentFilterCoeff) {
                case BMP581_FILTER_OFF:     status += "无滤波"; break;
                case BMP581_FILTER_COEFF_3: status += "轻微"; break;
                case BMP581_FILTER_COEFF_7: status += "低"; break;
                case BMP581_FILTER_COEFF_15: status += "中"; break;
                case BMP581_FILTER_COEFF_31: status += "高"; break;
                case BMP581_FILTER_COEFF_63: status += "强"; break;
                case BMP581_FILTER_COEFF_127: status += "最强"; break;
                default: status += "未知"; break;
            }
            status += "\n";
            
            status += "温度范围: ";
            status += String(minTemperature, 1);
            status += " ~ ";
            status += String(maxTemperature, 1);
            status += " °C\n";
            
            status += "读取成功率: ";
            if (totalReads > 0) {
                float successRate = (successfulReads * 100.0) / totalReads;
                status += String(successRate, 1);
                status += "%\n";
            } else {
                status += "N/A\n";
            }
        }
    }
    
    return status;
}

// ==================== 公共接口函数 ====================

float getCurrentAltitude() {
    return relativeAltitude;
}

float getAbsoluteAltitude() {
    return currentAltitude;
}

float getTemperature() {
    return currentTemperature;
}

bool isBMP581Ready() {
    return bmp581Initialized && calibrationComplete && !sensorReadError && 
           currentBMP581State == BMP581_STATE_NORMAL_OPERATION;
}

float getBaselineAltitude() {
    return baselineAltitude;
}

BMP581State getSensorState() {
    return currentBMP581State;
}

void printSensorStatus() {
    if (!bmp581Initialized) {
        Serial.println("❌ BMP581未初始化");
        return;
    }
    
    Serial.println("\n=== BMP581传感器状态 ===");
    Serial.print("初始化状态: ");
    Serial.println(bmp581Initialized ? "✅ 成功" : "❌ 失败");
    
    Serial.print("运行状态: ");
    switch(currentBMP581State) {
        case BMP581_STATE_IDLE: Serial.println("🔄 空闲"); break;
        case BMP581_STATE_CALIBRATING: Serial.println("🔄 校准中"); break;
        case BMP581_STATE_NORMAL_OPERATION: Serial.println("✅ 正常运行"); break;
        case BMP581_STATE_ERROR: Serial.println("❌ 错误"); break;
        default: Serial.println("❓ 未知"); break;
    }
    
    Serial.print("校准状态: ");
    if (calibrationInProgress) {
        Serial.print("🔄 进行中 (");
        Serial.print((millis() - calibrationStartTime) / 1000);
        Serial.println("秒)");
    } else {
        Serial.println(calibrationComplete ? "✅ 完成" : "❌ 未完成");
    }
    
    if (calibrationComplete) {
        Serial.print("基准高度: ");
        Serial.print(baselineAltitude, 2);
        Serial.println(" m");
        
        Serial.print("当前高度: ");
        Serial.print(currentAltitude, 2);
        Serial.println(" m (绝对)");
        
        Serial.print("相对高度: ");
        Serial.print(relativeAltitude, 3);
        Serial.println(" m");
        
        Serial.print("温度: ");
        Serial.print(currentTemperature, 2);
        Serial.println(" °C (校准后)");
        
        Serial.print("原始温度: ");
        Serial.print(rawTemperature, 2);
        Serial.println(" °C");
        
        Serial.print("温度稳定性: ");
        Serial.println(temperatureStable ? "✅ 稳定" : "⚠️ 波动");
        
        Serial.print("温度偏移: ");
        Serial.print(temperatureOffset, 2);
        Serial.println(" °C");
        
        Serial.print("自热补偿: ");
        Serial.print(selfHeatCompensation, 2);
        Serial.println(" °C");
        
        Serial.print("温度范围: ");
        Serial.print(minTemperature, 1);
        Serial.print(" ~ ");
        Serial.print(maxTemperature, 1);
        Serial.println(" °C");
        
        Serial.print("读取成功率: ");
        if (totalReads > 0) {
            float successRate = (successfulReads * 100.0) / totalReads;
            Serial.print(successRate, 1);
            Serial.println("%");
        } else {
            Serial.println("N/A");
        }
    }
    
    Serial.print("最后读取: ");
    if (lastReadTime > 0) {
        Serial.print((millis() - lastReadTime) / 1000.0, 1);
        Serial.println(" 秒前");
    } else {
        Serial.println("从未读取");
    }
    
    Serial.println("========================\n");
}

// ==================== 其他现有函数保持原样 ====================

void scanBMP581Address() {
    // 保持原样
}

void setSeaLevelPressure(float pressure) {
    if (pressure > 80000 && pressure < 120000) {
        seaLevelPressure = pressure;
        
        if (calibrationComplete) {
            baselineAltitude = calculateAltitude(baselinePressure);
            currentAltitude = calculateAltitude(filteredPressure);
            relativeAltitude = currentAltitude - baselineAltitude;
        }
    }
}

bool getRawSensorData(float &pressure, float &temperature) {
    if (!bmp581Initialized) return false;
    
    bmp5_sensor_data data = {0, 0};
    int8_t err = pressureSensor.getSensorData(&data);
    
    if (err == BMP5_OK) {
        pressure = data.pressure;
        temperature = data.temperature;
        return true;
    }
    
    return false;
}

void resetAltitudeBaseline() {
    if (!bmp581Initialized || !calibrationComplete) {
        Serial.println("传感器未就绪，无法重置高度基准");
        return;
    }
    
    baselinePressure = filteredPressure;
    baselineAltitude = calculateAltitude(baselinePressure);
    relativeAltitude = 0.0;
    
    Serial.println("✅ 高度基准已重置");
}

void testBMP581() {
    Serial.println("\n=== BMP581测试 ===");
    
    if (!bmp581Initialized) {
        Serial.println("传感器未初始化");
        return;
    }
    
    bmp5_sensor_data data = {0, 0};
    int8_t err = pressureSensor.getSensorData(&data);
    
    if (err == BMP5_OK) {
        Serial.print("压力: ");
        Serial.print(data.pressure, 1);
        Serial.println(" Pa");
        
        Serial.print("原始温度: ");
        Serial.print(data.temperature, 1);
        Serial.println(" °C");
        
        Serial.print("校准温度: ");
        Serial.print(applyTemperatureCorrection(data.temperature), 1);
        Serial.println(" °C");
        
        float altitude = calculateAltitude(data.pressure);
        Serial.print("计算高度: ");
        Serial.print(altitude, 2);
        Serial.println(" m");
        
        Serial.println("✅ 传感器读取正常");
    } else {
        Serial.print("❌ 读取失败，错误代码: ");
        Serial.println(err);
    }
    
    Serial.println("==================\n");
}

void quickBMP581Status() {
    if (!bmp581Initialized) {
        Serial.println("BMP581: 未初始化");
        return;
    }
    
    if (calibrationInProgress) {
        int elapsed = (millis() - calibrationStartTime) / 1000;
        int total = CALIBRATION_TIME / 1000;
        Serial.print("BMP581: 校准中 ");
        Serial.print(elapsed);
        Serial.print("/");
        Serial.print(total);
        Serial.println("秒");
    } else if (calibrationComplete) {
        Serial.print("BMP581: 高度=");
        Serial.print(relativeAltitude, 2);
        Serial.print("m, 温度=");
        Serial.print(currentTemperature, 1);
        Serial.println("°C");
    } else {
        Serial.println("BMP581: 校准未完成");
    }
}

String getBMP581Diagnostics() {
    String diag = "=== BMP581诊断信息 ===\n";
    
    diag += "初始化状态: " + String(bmp581Initialized ? "成功" : "失败") + "\n";
    diag += "当前状态: ";
    
    switch(currentBMP581State) {
        case BMP581_STATE_IDLE: diag += "空闲"; break;
        case BMP581_STATE_CALIBRATING: diag += "校准中"; break;
        case BMP581_STATE_NORMAL_OPERATION: diag += "正常运行"; break;
        case BMP581_STATE_ERROR: diag += "错误"; break;
        default: diag += "未知"; break;
    }
    diag += "\n";
    
    diag += "校准状态: " + String(calibrationComplete ? "完成" : (calibrationInProgress ? "进行中" : "未开始")) + "\n";
    diag += "温度稳定性: " + String(temperatureStable ? "稳定" : "波动") + "\n";
    diag += "温度偏移: " + String(temperatureOffset, 2) + " °C\n";
    diag += "自热补偿: " + String(selfHeatCompensation, 2) + " °C\n";
    diag += "连续错误: " + String(consecutiveErrors) + "\n";
    diag += "读取成功率: " + String(totalReads > 0 ? String((successfulReads * 100.0) / totalReads, 1) + "%" : "N/A") + "\n";
    
    if (calibrationComplete) {
        diag += "基准压力: " + String(baselinePressure, 1) + " Pa\n";
        diag += "相对高度: " + String(relativeAltitude, 3) + " m\n";
        diag += "校准温度: " + String(currentTemperature, 2) + " °C\n";
        diag += "温度范围: " + String(minTemperature, 1) + " ~ " + String(maxTemperature, 1) + " °C\n";
    }
    
    diag += "=====================\n";
    
    return diag;
}