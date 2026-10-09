// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink通信

#include <MAVLink.h>
#include "util.h"

extern float controlTime;
extern float voltage;

int mavlinkSysId = 1;

Rate telemetrySlow(2);
Rate telemetryAttitude(20);
Rate telemetryRC(10);
Rate telemetryMotors(10);
Rate telemetryIMU(15);

float mavlinkTime = NAN; // 上次收到消息的时间
String mavlinkPrintBuffer;

void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

void sendMavlink() {
	sendMavlinkPrint();

	mavlink_message_t msg;
	uint32_t time = t * 1000;

	if (telemetrySlow) {
		mavlink_msg_heartbeat_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_FLIX,
			(armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
			((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0) |
			((mode == AUTO) ? MAV_MODE_FLAG_AUTO_ENABLED : MAV_MODE_FLAG_MANUAL_INPUT_ENABLED),
			mode, MAV_STATE_STANDBY);
		sendMessage(&msg);
	}

	if (!valid(mavlinkTime)) return; // 连接建立前只发送心跳包

	if (telemetrySlow) {
		mavlink_msg_extended_sys_state_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			MAV_VTOL_STATE_UNDEFINED, landed ? MAV_LANDED_STATE_ON_GROUND : MAV_LANDED_STATE_IN_AIR);
		sendMessage(&msg);
	}

	if (telemetrySlow && valid(voltage)) {
		uint16_t voltages[] = {(uint16_t)(voltage * 1000), UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
		uint16_t voltagesExt[] = {0, 0, 0, 0};
		float remaining = constrain(mapf(voltage, 3.4, 4.2, 0, 1), 0, 1);
		mavlink_msg_battery_status_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, 0, MAV_BATTERY_FUNCTION_ALL,
			MAV_BATTERY_TYPE_LIPO, INT16_MAX, voltages, -1, -1, -1, remaining * 100, 0, MAV_BATTERY_CHARGE_STATE_OK, voltagesExt, 0, 0);
		sendMessage(&msg);
	}

	if (telemetryAttitude) {
		const float offset[] = {0, 0, 0, 0};
		mavlink_msg_attitude_quaternion_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitude.w, attitude.x, -attitude.y, -attitude.z, rates.x, -rates.y, -rates.z, offset); // 转换为FRD坐标系
		sendMessage(&msg);
	}

	if (telemetryRC && channels[0]) { // 0 表示没有遥控器输入
		mavlink_msg_rc_channels_raw_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, controlTime * 1000, 0,
			channels[0], channels[1], channels[2], channels[3], channels[4], channels[5], channels[6], channels[7], UINT8_MAX);
		sendMessage(&msg);
	}

	if (telemetryMotors) {
		float controls[8];
		memcpy(controls, motors, sizeof(motors));
		mavlink_msg_actuator_control_target_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, time, 0, controls);
		sendMessage(&msg);
	}

	if (telemetryIMU) {
		mavlink_msg_scaled_imu_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, time,
			acc.x / ONE_G * 1000, -acc.y / ONE_G * 1000, -acc.z / ONE_G * 1000, // 转换为FRD坐标系
			gyro.x * 1000, -gyro.y * 1000, -gyro.z * 1000,
			0, 0, 0, 0);
		sendMessage(&msg);
	}
}

void sendMessage(const void *msg) {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = mavlink_msg_to_send_buffer(buf, (mavlink_message_t *)msg);
	sendWiFi(buf, len);
}

void receiveMavlink() {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = receiveWiFi(buf, MAVLINK_MAX_PACKET_LEN);

	// 收到新数据包，进行解析
	mavlink_message_t msg;
	mavlink_status_t status;
	for (int i = 0; i < len; i++) {
		if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
			mavlinkTime = t;
			handleMavlink(&msg);
		}
	}
}

void handleMavlink(const void *_msg) {
	const mavlink_message_t& msg = *(mavlink_message_t *)_msg;

	if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
		mavlink_manual_control_t m;
		mavlink_msg_manual_control_decode(&msg, &m);
		if (m.target && m.target != mavlinkSysId) return; // 0 表示广播

		controlThrottle = m.z / 1000.0f;
		controlPitch = m.x / 1000.0f;
		controlRoll = m.y / 1000.0f;
		controlYaw = m.r / 1000.0f;
		controlMode = NAN;
		controlTime = t;
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_param_request_list_t m;
		mavlink_msg_param_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		mavlink_message_t msg;
		for (int i = 0; i < parametersCount(); i++) {
			mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
				getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t m;
		mavlink_msg_param_request_read_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		char name[MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id 可能不是以空字符结尾的
		float value = strlen(name) == 0 ? getParameter(m.param_index) : getParameter(name);
		if (m.param_index != -1) {
			memcpy(name, getParameterName(m.param_index), 16);
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			name, value, MAV_PARAM_TYPE_REAL32, parametersCount(), m.param_index);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t m;
		mavlink_msg_param_set_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		char name[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id 可能不是以空字符结尾的
		bool success = setParameter(name, m.param_value);
		if (!success) return;
		// 发送确认
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			m.param_id, getParameter(name), MAV_PARAM_TYPE_REAL32, parametersCount(), 0); // 索引未知
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST) { // 处理该消息以使地面站(QGC)满意
		mavlink_mission_request_list_t m;
		mavlink_msg_mission_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, 0, 0, 0, MAV_MISSION_TYPE_MISSION, 0);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SERIAL_CONTROL) {
		mavlink_serial_control_t m;
		mavlink_msg_serial_control_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, (const char *)m.data, m.count); // data 可能不是以空字符结尾的
		doCommand(data, true);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ATTITUDE_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_attitude_target_t m;
		mavlink_msg_set_attitude_target_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		if (!(m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE)) {
			// 姿态控制
			attitudeTarget.w = m.q[0];
			attitudeTarget.x = m.q[1];
			attitudeTarget.y = -m.q[2]; // 转换为FLU坐标系
			attitudeTarget.z = -m.q[3];
			ratesExtra.x = m.body_roll_rate;
			ratesExtra.y = -m.body_pitch_rate;
			ratesExtra.z = -m.body_yaw_rate;
		} else {
			// 角速度控制
			attitudeTarget.invalidate();
			ratesTarget.x = m.body_roll_rate;
			ratesTarget.y = -m.body_pitch_rate;
			ratesTarget.z = -m.body_yaw_rate;
		}

		thrustTarget = valid(m.thrust) ? m.thrust : thrustTarget;
		armed = m.thrust > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_actuator_control_target_t m;
		mavlink_msg_set_actuator_control_target_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget.invalidate();
		memcpy(motors, m.controls, sizeof(motors)); // 复制电机推力
		armed = motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_DATA) {
		mavlink_log_request_data_t m;
		mavlink_msg_log_request_data_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		// 发送全部日志记录
		for (int i = 0; i < sizeof(logBuffer) / sizeof(logBuffer[0]); i++) {
			mavlink_message_t msg;
			mavlink_msg_log_data_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, 0, i,
				sizeof(logBuffer[0]), (uint8_t *)logBuffer[i]);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		int result = handleMavlinkCommand(&m);
		mavlink_message_t ack;
		mavlink_msg_command_ack_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, result, UINT8_MAX, 0, msg.sysid, msg.compid);
		sendMessage(&ack);
	}
}

int handleMavlinkCommand(const void *_m) {
	const mavlink_command_long_t& m = *(mavlink_command_long_t *)_m;

	if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
		mavlink_message_t response;
		mavlink_msg_autopilot_version_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &response,
			MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
		sendMessage(&response);
		return MAV_RESULT_ACCEPTED;
	}

	if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_COMPONENT_METADATA) {
		mavlink_message_t response;
		mavlink_msg_component_metadata_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &response, t * 1000,
		2566517357, // crc
		"https://quadcopter.dev/meta/2566517357/general.json");
		sendMessage(&response);
		return MAV_RESULT_ACCEPTED;
	}

	if (m.command == MAV_CMD_COMPONENT_ARM_DISARM) {
		if (m.param1 == 1 && controlThrottle > 0.05) return MAV_RESULT_DENIED; // 油门未降到低位时禁止解锁
		armed = m.param1 == 1;
		return MAV_RESULT_ACCEPTED;
	}

	if (m.command == MAV_CMD_DO_SET_MODE) {
		if (m.param2 < 0 || m.param2 > AUTO) return MAV_RESULT_DENIED; // 模式不正确
		mode = m.param2;
		return MAV_RESULT_ACCEPTED;
	}

	return MAV_RESULT_UNSUPPORTED;
}

// 将控制台输出发送到地面站
void mavlinkPrint(const char* str) {
	mavlinkPrintBuffer += str;
}

void sendMavlinkPrint() {
	// 分块发送MAVLink打印数据
	const char *str = mavlinkPrintBuffer.c_str();
	for (int i = 0; i < strlen(str); i += MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, str + i, sizeof(data));
		mavlink_message_t msg;
		mavlink_msg_serial_control_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			SERIAL_CONTROL_DEV_SHELL,
			i + MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN < strlen(str) ? SERIAL_CONTROL_FLAG_MULTI : 0, // 还有更多数据块待发送
			0, 0, strlen(data), (uint8_t *)data, 0, 0);
		sendMessage(&msg);
	}
	mavlinkPrintBuffer.clear();
}
