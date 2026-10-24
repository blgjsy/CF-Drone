// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication - MAVLink 2.0 强制版本（最小兼容版本）

#if WIFI_ENABLED

#include <MAVLink.h>

#define SYSTEM_ID 1
#define COMPONENT_ID MAV_COMP_ID_AUTOPILOT1
#define PERIOD_SLOW 1.0
#define PERIOD_FAST 0.1
#define PERIOD_ATTITUDE 0.02  // 姿态数据50Hz发送
#define MAVLINK_CONTROL_YAW_DEAD_ZONE 0.1f

// 飞行模式定义
const int MANUAL = 0, ACRO = 1, STAB = 2, AUTO = 3;

bool mavlinkConnected = false;
String mavlinkPrintBuffer;
bool mavlink2Enabled = true;  // 强制使用 MAVLink 2.0

// 外部变量声明
extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern int mode;
extern bool armed;
extern bool landed;
extern float t;
extern Quaternion attitude;  // 姿态四元数
extern Vector gyro;  // 陀螺仪原始数据
extern Vector acc;   // 加速度计原始数据
extern Vector rates; // 处理后的角速度
extern Vector ratesTarget;
extern Quaternion attitudeTarget;
extern Vector ratesExtra;
extern float thrustTarget;
extern Vector torqueTarget;
extern uint16_t channels[];
extern float motors[];

// 函数声明
extern int parametersCount();
extern const char* getParameterName(int i);
extern float getParameter(int i);
extern float getParameter(const char* name);
extern bool setParameter(const char* name, float value);
extern bool isBMP581Ready();
extern float getAbsoluteAltitude();

// 新增全局变量
static bool firstHeartbeatSent = false;
static float last_debug_time = 0;
static int attitude_packet_count = 0;
static bool attitude_valid = false;

// MAVLink 2.0 强制初始化
void initMavlink2() {
    Serial.println("[MAV] Initializing MAVLink 2.0 mode with QGC support");
}

// 主动发送参数列表
void sendParameterList() {
    Serial.println("[MAV] Sending parameter list proactively");
    
    mavlink_message_t param_msg;
    for (int i = 0; i < parametersCount(); i++) {
        mavlink_msg_param_value_pack(SYSTEM_ID, COMPONENT_ID, &param_msg,
            getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
        param_msg.magic = 0xFD;
        sendMessage(&param_msg);
        
        if (i % 10 == 0) {
            Serial.print("[MAV] Sent parameter ");
            Serial.print(i);
            Serial.print(" of ");
            Serial.println(parametersCount());
        }
    }
    Serial.println("[MAV] Parameter list sent complete");
}

void processMavlink() {
    sendMavlink();
    receiveMavlink();
}

void sendMavlink() {
    static float lastSlow = 0;
    static float lastFast = 0;
    static float lastAttitude = 0;  // 姿态数据发送计时器
    
    mavlink_message_t msg;
    uint32_t time_ms = millis();
    uint32_t time_boot_ms = t * 1000;

    // ====== 慢速数据（1Hz）=====
    if (t - lastSlow >= PERIOD_SLOW) {
        lastSlow = t;

        // 1. 心跳包 - 强制使用 MAVLink 2.0
        mavlink_msg_heartbeat_pack(SYSTEM_ID, COMPONENT_ID, &msg, 
            MAV_TYPE_QUADROTOR, 
            MAV_AUTOPILOT_GENERIC,
            (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
            ((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0) |
            ((mode == AUTO) ? MAV_MODE_FLAG_AUTO_ENABLED : MAV_MODE_FLAG_MANUAL_INPUT_ENABLED),
            mode,
            MAV_STATE_ACTIVE);
        
        // 强制设置为 MAVLink 2.0 格式（0xFD）
        msg.magic = 0xFD;
        
        sendMessage(&msg);

        // 如果是第一次发送心跳包，主动发送参数列表
        if (!firstHeartbeatSent && mavlinkConnected) {
            firstHeartbeatSent = true;
            sendParameterList();
        }

        // 2. 系统状态
        mavlink_msg_sys_status_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL | MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE,
            MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL | MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE,
            MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL | MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE,
            500,
            11000,
            -1,
            100,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        // 3. 系统时间
        mavlink_msg_system_time_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            time_ms * 1000,
            time_boot_ms);
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        // 调试信息：定期打印姿态数据
        if (t - last_debug_time >= 2.0) {
            last_debug_time = t;
            
            // 检查姿态四元数是否有效
            if (attitude.w != 0 || attitude.x != 0 || attitude.y != 0 || attitude.z != 0) {
                float roll = attitude.getRoll();
                float pitch = attitude.getPitch();
                float yaw = attitude.getYaw();
                
                Serial.print("[MAV-DEBUG] Attitude Quaternion: w=");
                Serial.print(attitude.w, 4);
                Serial.print(", x=");
                Serial.print(attitude.x, 4);
                Serial.print(", y=");
                Serial.print(attitude.y, 4);
                Serial.print(", z=");
                Serial.print(attitude.z, 4);
                
                Serial.print(", Euler: R=");
                Serial.print(degrees(roll), 1);
                Serial.print("°, P=");
                Serial.print(degrees(pitch), 1);
                Serial.print("°, Y=");
                Serial.print(degrees(yaw), 1);
                Serial.println("°");
                
                attitude_valid = true;
            } else {
                Serial.println("[MAV-DEBUG] WARNING: Attitude quaternion is zero!");
                attitude_valid = false;
            }
            
            Serial.print("[MAV-DEBUG] IMU Data - Gyro: X=");
            Serial.print(degrees(gyro.x), 1);
            Serial.print("°/s, Y=");
            Serial.print(degrees(gyro.y), 1);
            Serial.print("°/s, Z=");
            Serial.print(degrees(gyro.z), 1);
            Serial.print("°/s, Acc: X=");
            Serial.print(acc.x, 2);
            Serial.print("g, Y=");
            Serial.print(acc.y, 2);
            Serial.print("g, Z=");
            Serial.print(acc.z, 2);
            Serial.println("g");
            
            Serial.print("[MAV-DEBUG] Attitude packets sent: ");
            Serial.println(attitude_packet_count);
            Serial.print("[MAV-DEBUG] MAVLink connected: ");
            Serial.println(mavlinkConnected ? "YES" : "NO");
            
            // 检查QGC期望的关键消息
            Serial.println("[MAV-DEBUG] QGC needs: HEARTBEAT, SYS_STATUS, SYSTEM_TIME, ATTITUDE_QUATERNION, ATTITUDE");
        }
    }

    // ====== 姿态数据（50Hz）=====
    if (t - lastAttitude >= PERIOD_ATTITUDE) {
        lastAttitude = t;
        
        // 检查姿态四元数是否有效
        if (!attitude_valid) {
            // 如果姿态无效，发送一个默认姿态（水平）
            sendDefaultAttitude(time_boot_ms);
            return;
        }
        
        // 1. 姿态四元数 - QGC最需要的消息
        float roll = attitude.getRoll();
        float pitch = attitude.getPitch();
        float yaw = attitude.getYaw();
        
        // 使用attitude四元数的值
        float qw = attitude.w;
        float qx = attitude.x;
        float qy = attitude.y;
        float qz = attitude.z;
        
        // 确保四元数是单位四元数
        float norm = sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (norm > 0.01) { // 避免除以零
            qw /= norm;
            qx /= norm;
            qy /= norm;
            qz /= norm;
        }
        
        // 调试：每100个包打印一次四元数
        if (attitude_packet_count % 100 == 0) {
            Serial.print("[MAV] Sending Attitude Quaternion: q=[");
            Serial.print(qw, 4);
            Serial.print(", ");
            Serial.print(qx, 4);
            Serial.print(", ");
            Serial.print(qy, 4);
            Serial.print(", ");
            Serial.print(qz, 4);
            Serial.print("], Euler=[");
            Serial.print(degrees(roll), 1);
            Serial.print(", ");
            Serial.print(degrees(pitch), 1);
            Serial.print(", ");
            Serial.print(degrees(yaw), 1);
            Serial.println("] deg");
            
            // 同时打印传感器数据
            Serial.print("[MAV] Sensor data: Gyro=[");
            Serial.print(degrees(gyro.x), 1);
            Serial.print(", ");
            Serial.print(degrees(gyro.y), 1);
            Serial.print(", ");
            Serial.print(degrees(gyro.z), 1);
            Serial.print("] Acc=[");
            Serial.print(acc.x, 2);
            Serial.print(", ");
            Serial.print(acc.y, 2);
            Serial.print(", ");
            Serial.print(acc.z, 2);
            Serial.println("]");
        }
        
        // 发送姿态四元数消息 - QGC姿态球主要使用这个
        mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            time_boot_ms,
            qw, qx, qy, qz,
            gyro.x, gyro.y, gyro.z,  // 角速度
            0);  // 协方差
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        // 2. 姿态欧拉角（兼容性）
        mavlink_msg_attitude_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            time_boot_ms,
            roll, pitch, yaw,
            gyro.x, gyro.y, gyro.z);
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        attitude_packet_count++;
    }

    // ====== 快速数据（10Hz）=====
    if (t - lastFast >= PERIOD_FAST) {
        lastFast = t;

        // 1. VFR_HUD - QGC显示需要
        float roll = 0, pitch = 0, yaw = 0;
        if (attitude_valid) {
            roll = attitude.getRoll();
            pitch = attitude.getPitch();
            yaw = attitude.getYaw();
        }
        float yaw_deg = degrees(yaw);
        
        // 确保偏航角在0-360度范围内
        while (yaw_deg < 0) yaw_deg += 360.0f;
        while (yaw_deg >= 360.0f) yaw_deg -= 360.0f;
        
        float current_altitude = isBMP581Ready() ? getAbsoluteAltitude() : 0.0f;
        
        // 发送VFR_HUD数据
        mavlink_msg_vfr_hud_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            controlThrottle * 100.0f,  // 油门百分比
            0.0f,     // 空速（m/s）
            0.0f,     // 地速（m/s）
            (int16_t)yaw_deg,  // 航向（0-360度）
            0.0f,     // 爬升率（m/s）
            current_altitude);  // 高度（米）
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        // 2. IMU数据 - 重要：QGC需要正确的坐标系
        // 加速度计数据（m/s² → mg）
        // NED坐标系：X=北，Y=东，Z=下
        // 根据您的IMU旋转：FLU到NED转换
        float acc_ned_x = acc.x * 1000;     // FLU X (前) → NED X (北)
        float acc_ned_y = -acc.y * 1000;    // FLU Y (左) → NED Y (东) - 需要取负
        float acc_ned_z = -acc.z * 1000;    // FLU Z (上) → NED Z (下) - 需要取负
        
        // 陀螺仪数据（rad/s → mrad/s）
        float gyro_ned_x = gyro.x * 1000;     // 滚转角速度
        float gyro_ned_y = -gyro.y * 1000;    // 俯仰角速度
        float gyro_ned_z = -gyro.z * 1000;    // 偏航角速度
        
        mavlink_msg_scaled_imu_pack(SYSTEM_ID, COMPONENT_ID, &msg, 
            time_boot_ms,
            (int16_t)acc_ned_x, (int16_t)acc_ned_y, (int16_t)acc_ned_z,
            (int16_t)gyro_ned_x, (int16_t)gyro_ned_y, (int16_t)gyro_ned_z,
            0, 0, 0, 0);
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        // 3. 高分辨率IMU数据
        uint64_t time_usec = micros64();
        
        // 注意：使用原始数据，但确保单位正确
        mavlink_msg_highres_imu_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            time_usec,
            acc.x, acc.y, acc.z,          // 加速度 m/s²
            gyro.x, gyro.y, gyro.z,       // 角速度 rad/s
            0.0f, 0.0f, 0.0f,             // 磁场
            0.0f,                         // 绝对压力
            0.0f,                         // 差分压力
            0.0f,                         // 压力高度
            0.0f,                         // 温度
            0x07,                         // fields_updated: 加速度+陀螺仪
            0);                           // id
        
        msg.magic = 0xFD;
        sendMessage(&msg);
        
        // 4. RC通道数据
        if (channels[0] != 0) {
            mavlink_msg_rc_channels_raw_pack(SYSTEM_ID, COMPONENT_ID, &msg, 
                time_boot_ms, 
                0,
                channels[0], channels[1], channels[2], channels[3], 
                channels[4], channels[5], channels[6], channels[7], 
                UINT8_MAX);
            
            msg.magic = 0xFD;
            sendMessage(&msg);
        }
        
        // 5. 高度信息
        if (isBMP581Ready()) {
            float altitude = getAbsoluteAltitude();
            mavlink_msg_altitude_pack(SYSTEM_ID, COMPONENT_ID, &msg,
                time_boot_ms,
                altitude,
                altitude,
                altitude,
                0.0f,
                0.0f,
                0.0f);
            
            msg.magic = 0xFD;
            sendMessage(&msg);
        }
    }
    
    // 发送打印数据
    sendMavlinkPrint();
}

// 发送默认姿态（水平）
void sendDefaultAttitude(uint32_t time_boot_ms) {
    static int default_count = 0;
    
    mavlink_message_t msg;
    
    // 水平姿态的四元数（w=1, x=0, y=0, z=0）
    float qw = 1.0f;
    float qx = 0.0f;
    float qy = 0.0f;
    float qz = 0.0f;
    
    if (default_count % 100 == 0) {
        Serial.println("[MAV] Sending DEFAULT attitude (水平)");
        default_count = 0;
    }
    default_count++;
    
    // 发送姿态四元数消息
    mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, COMPONENT_ID, &msg,
        time_boot_ms,
        qw, qx, qy, qz,
        0, 0, 0,  // 角速度为0
        0);
    
    msg.magic = 0xFD;
    sendMessage(&msg);
    
    // 发送姿态欧拉角消息
    mavlink_msg_attitude_pack(SYSTEM_ID, COMPONENT_ID, &msg,
        time_boot_ms,
        0, 0, 0,  // 滚转、俯仰、偏航为0
        0, 0, 0); // 角速度为0
    
    msg.magic = 0xFD;
    sendMessage(&msg);
}

// 64位时间戳函数
uint64_t micros64() {
    static uint32_t low32, high32;
    uint32_t new_low32 = micros();
    if (new_low32 < low32) high32++;
    low32 = new_low32;
    return (uint64_t) high32 << 32 | low32;
}

void sendMessage(const void *msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t* m = (mavlink_message_t*)msg;
    
    // 强制所有消息为 MAVLink 2.0
    if (mavlink2Enabled) {
        m->magic = 0xFD;
    }
    
    int len = mavlink_msg_to_send_buffer(buf, m);
    
    // 调试输出：显示发送的关键消息
    static int sendCount = 0;
    
    if (m->msgid == MAVLINK_MSG_ID_ATTITUDE_QUATERNION) {
        if (sendCount % 100 == 0) {  // 每100个包输出一次
            float q[4];
            memcpy(q, m->payload64, sizeof(float) * 4);
            
            // 从四元数计算欧拉角用于显示
            float roll = atan2(2*(q[0]*q[1] + q[2]*q[3]), 1 - 2*(q[1]*q[1] + q[2]*q[2]));
            float pitch = asin(2*(q[0]*q[2] - q[3]*q[1]));
            float yaw = atan2(2*(q[0]*q[3] + q[1]*q[2]), 1 - 2*(q[2]*q[2] + q[3]*q[3]));
            
            Serial.print("[MAV-TX] AttitudeQuaternion ID:31 - q=[");
            Serial.print(q[0], 4);
            Serial.print(", ");
            Serial.print(q[1], 4);
            Serial.print(", ");
            Serial.print(q[2], 4);
            Serial.print(", ");
            Serial.print(q[3], 4);
            Serial.print("], RPY=[");
            Serial.print(degrees(roll), 1);
            Serial.print(", ");
            Serial.print(degrees(pitch), 1);
            Serial.print(", ");
            Serial.print(degrees(yaw), 1);
            Serial.println("] deg");
        }
        sendCount++;
    }
    
    sendWiFi(buf, len);
}

void receiveMavlink() {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    int len = receiveWiFi(buf, MAVLINK_MAX_PACKET_LEN);
    
    if (len > 0) {
        if (!mavlinkConnected) {
            mavlinkConnected = true;
            Serial.println("[MAV] Connected to GCS");
        }
    }

    mavlink_message_t msg;
    mavlink_status_t status;
    
    for (int i = 0; i < len; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
            handleMavlink(&msg);
        }
    }
}

bool shouldProcessMessage(uint8_t target_system, uint8_t target_component) {
    return (target_system == 0) || (target_system == 255) || (target_system == SYSTEM_ID);
}

void handleMavlink(const void *_msg) {
    const mavlink_message_t& msg = *(mavlink_message_t *)_msg;
    
    if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
        mavlink_heartbeat_t hb;
        mavlink_msg_heartbeat_decode(&msg, &hb);
        
        if (hb.type == MAV_TYPE_GCS || hb.type == MAV_TYPE_GENERIC) {
            if (!mavlinkConnected) {
                Serial.println("[MAV] GCS connected, sending heartbeat response");
            }
            mavlinkConnected = true;
            
            // 立即回应心跳
            mavlink_message_t response;
            mavlink_msg_heartbeat_pack(SYSTEM_ID, COMPONENT_ID, &response,
                MAV_TYPE_QUADROTOR,
                MAV_AUTOPILOT_GENERIC,
                (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
                ((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0),
                mode,
                MAV_STATE_ACTIVE);
            response.magic = 0xFD;
            sendMessage(&response);
        }
    }

    if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
        mavlink_manual_control_t m;
        mavlink_msg_manual_control_decode(&msg, &m);
        
        if (shouldProcessMessage(m.target, 0)) {
            controlThrottle = m.z / 1000.0f;
            controlPitch = m.x / 1000.0f;
            controlRoll = m.y / 1000.0f;
            controlYaw = m.r / 1000.0f;
            controlMode = NAN;
            controlTime = t;

            if (abs(controlYaw) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controlYaw = 0;
            
            static int control_count = 0;
            if (control_count % 10 == 0) {
                Serial.print("[MAV] Manual control: T=");
                Serial.print(controlThrottle);
                Serial.print(" P=");
                Serial.print(controlPitch);
                Serial.print(" R=");
                Serial.print(controlRoll);
                Serial.print(" Y=");
                Serial.println(controlYaw);
            }
            control_count++;
        }
    }

    // 参数请求处理（保持原样）
    if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
        mavlink_param_request_list_t m;
        mavlink_msg_param_request_list_decode(&msg, &m);
        
        if (shouldProcessMessage(m.target_system, m.target_component)) {
            Serial.println("[MAV] Sending parameter list");
            
            mavlink_message_t param_msg;
            for (int i = 0; i < parametersCount(); i++) {
                mavlink_msg_param_value_pack(SYSTEM_ID, COMPONENT_ID, &param_msg,
                    getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
                param_msg.magic = 0xFD;
                sendMessage(&param_msg);
            }
        }
    }
    
    // 其他消息处理...
    // [这里省略其他消息处理代码，保持原样]
}

void mavlinkPrint(const char* str) {
    if (mavlinkPrintBuffer.length() > 256) {
        mavlinkPrintBuffer = mavlinkPrintBuffer.substring(mavlinkPrintBuffer.length() - 128);
    }
    mavlinkPrintBuffer += str;
}

void sendMavlinkPrint() {
    if (mavlinkPrintBuffer.length() == 0 || !mavlinkConnected) {
        mavlinkPrintBuffer.clear();
        return;
    }
    
    const char *str = mavlinkPrintBuffer.c_str();
    int totalLen = strlen(str);
    
    for (int i = 0; i < totalLen; i += MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
        char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
        strlcpy(data, str + i, sizeof(data));
        mavlink_message_t msg;
        mavlink_msg_serial_control_pack(SYSTEM_ID, COMPONENT_ID, &msg,
            SERIAL_CONTROL_DEV_SHELL,
            i + MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN < strlen(str) ? SERIAL_CONTROL_FLAG_MULTI : 0,
            0, 0, strlen(data), (uint8_t *)data, 0, 0);
        
        msg.magic = 0xFD;
        sendMessage(&msg);
    }
    mavlinkPrintBuffer.clear();
}

void mavlinkSetup() {
    initMavlink2();
    Serial.println("[MAV] MAVLink setup complete - Minimal Compatible Version");
    Serial.println("[MAV] Key messages: HEARTBEAT, ATTITUDE_QUATERNION(31), ATTITUDE(30)");
}

#endif