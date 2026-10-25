// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 遥控接收机相关操作（支持 SBUS 与 ELRS CRSF 两种协议）

#include <SBUS.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <HardwareSerial.h>
#endif
#include "util.h"

// 遥控协议
const int RC_PROTO_SBUS = 0, RC_PROTO_CRSF = 1;
int rcProtocol = RC_PROTO_SBUS; // 0 = SBUS，1 = ELRS CRSF

SBUS rc(Serial1);
int rcRxPin = -1; // SBUS 接收引脚，-1 表示禁用

// ELRS CRSF 接收机配置（UART2，仅 ESP32 平台）
int crsfRxPin = 17;
int crsfTxPin = 16;
int crsfBaud = 420000;
#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial CRSFSerial(2);
#endif

uint16_t channels[16]; // 原始遥控通道（μs）
int channelZero[16]; // 校准零值（中位）
int channelMax[16]; // 校准最大值

bool rcFresh = false; // 实体遥控器最近是否收到有效帧
float rcTime = NAN;   // 上次收到实体遥控帧的时间

float controlRoll, controlPitch, controlYaw, controlThrottle; // 飞手输入，范围 [-1, 1]
float controlMode = NAN;
float controlTime = NAN; // 上次控制更新的时间

int rollChannel = -1, pitchChannel = -1, throttleChannel = -1, yawChannel = -1, modeChannel = -1; // 通道映射
int ledChannel = 5; // 遥控 LED 通道映射（默认 CH5）

// 遥控 LED 控制（由遥控器通道控制的外部 LED）
int ledPin = -1; // 遥控 LED 引脚，-1 表示禁用
float ledThreshold = 1500; // LED 开关阈值（μs），大于此值点亮
bool remoteLedState = false; // 当前 LED 状态

// ---- ELRS CRSF 协议 ----
#define CRSF_SYNC_BYTE 0xC8
#define CRSF_TYPE_CHANNELS 0x16
#define CRSF_TYPE_LINK_STATISTICS 0x14

uint8_t crsfBuffer[64];
int crsfBufferIndex = 0;

#if defined(ARDUINO_ARCH_ESP32)
bool crsfLinkActive = true;    // ELRS 链路是否有效（链接质量 > 0）
uint8_t crsfLinkQuality = 100; // 上行链接质量 0-100
#endif

void setupRC() {
	setupRemoteLED();
#if defined(ARDUINO_ARCH_ESP32)
	if (rcProtocol == RC_PROTO_CRSF) {
		print("正在初始化遥控器（ELRS CRSF）\n");
		// ELRS 通常使用 420000 波特率
		CRSFSerial.begin(crsfBaud, SERIAL_8N1, crsfRxPin, crsfTxPin);
		// CRSF 通道映射使用标准默认值（AETR），无需像 SBUS 那样拨杆校准
		if (rollChannel < 0) rollChannel = 0;
		if (pitchChannel < 0) pitchChannel = 1;
		if (throttleChannel < 0) throttleChannel = 2;
		if (yawChannel < 0) yawChannel = 3;
		if (modeChannel < 0) modeChannel = 4;
		return;
	}
#endif
	if (rcRxPin < 0) return;
	print("正在初始化遥控器（SBUS）\n");
	rc.begin(rcRxPin);
}

void setupRemoteLED() {
	if (ledPin < 0) return;
	pinMode(ledPin, OUTPUT);
	digitalWrite(ledPin, LOW);
	print("遥控 LED 已初始化：引脚 %d，通道 %d，阈值 %.0f\n", ledPin, ledChannel, ledThreshold);
}

bool readRC() {
#if defined(ARDUINO_ARCH_ESP32)
	if (rcProtocol == RC_PROTO_CRSF) {
		return readCRSF();
	}
#endif
	if (rcRxPin < 0) return false;
	if (!rc.read()) return false;

	rc.getChannels(channels);
	normalizeRC();
	controlRemoteLED(); // SBUS 也响应遥控 LED 通道
	controlTime = t;
	rcFresh = true;
	rcTime = t;
	return true;
}

#if defined(ARDUINO_ARCH_ESP32)

// CRC-8-DVB-S2（CRSF 协议使用，多项式 0xD5，初值 0x00）
uint8_t crsfCRC8(const uint8_t *data, int len) {
	uint8_t crc = 0;
	for (int i = 0; i < len; i++) {
		crc ^= data[i];
		for (int b = 0; b < 8; b++) {
			crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
		}
	}
	return crc;
}

// 读取并解析 ELRS CRSF 数据帧
bool readCRSF() {
	bool updated = false;
	while (CRSFSerial.available()) {
		uint8_t data = CRSFSerial.read();

		// 寻找同步字节
		if (crsfBufferIndex == 0 && data != CRSF_SYNC_BYTE) {
			continue;
		}

		crsfBuffer[crsfBufferIndex++] = data;

		// 检查是否收到完整帧
		if (crsfBufferIndex >= 2) {
			int frameLength = crsfBuffer[1];
			if (crsfBufferIndex >= frameLength + 2) {
				if (crsfFrameValid(crsfBuffer, frameLength + 2)) {
					handleCRSFFrame(crsfBuffer, frameLength + 2);
					updated = true;
				}
				crsfBufferIndex = 0;
			}
		}

		// 防止缓冲区溢出
		if (crsfBufferIndex >= sizeof(crsfBuffer)) {
			crsfBufferIndex = 0;
		}
	}
	return updated;
}

// 校验 CRSF 帧：长度字段一致性 + CRC-8 校验
bool crsfFrameValid(uint8_t *buffer, int length) {
	if (length < 5) return false;
	if (buffer[1] + 2 != length) return false; // 长度字段 = type + payload + crc
	uint8_t crc = crsfCRC8(buffer + 2, length - 3); // 覆盖 type..payload
	return crc == buffer[length - 1];
}

// 处理完整 CRSF 帧（已通过 CRC 校验）
void handleCRSFFrame(uint8_t *buffer, int length) {
	uint8_t type = buffer[2];

	if (type == CRSF_TYPE_CHANNELS && length == 26) {
		decodeCRSFChannels(buffer + 3);
		normalizeRC();
		controlRemoteLED();
		if (crsfLinkActive) {
			controlTime = t;
			rcFresh = true;
			rcTime = t;
		} else {
			// 链路丢失：不刷新控制时间，让 rcLossFailsafe 生效
			// （覆盖接收机配置为"失效保护值"仍持续发帧的情况）
			rcFresh = false;
		}
	} else if (type == CRSF_TYPE_LINK_STATISTICS && length >= 6) {
		// payload: [0]rssi1 [1]rssi2 [2]uplink_Link_quality ...
		crsfLinkQuality = buffer[5];
		crsfLinkActive = crsfLinkQuality > 0;
	}
}

// 从 22 字节解码 16 个通道（每通道 11 位）
void decodeCRSFChannels(uint8_t *payload) {
	channels[0]  = ((payload[0]  | payload[1] << 8) & 0x07FF);
	channels[1]  = ((payload[1] >> 3 | payload[2] << 5) & 0x07FF);
	channels[2]  = ((payload[2] >> 6 | payload[3] << 2 | payload[4] << 10) & 0x07FF);
	channels[3]  = ((payload[4] >> 1 | payload[5] << 7) & 0x07FF);
	channels[4]  = ((payload[5] >> 4 | payload[6] << 4) & 0x07FF);
	channels[5]  = ((payload[6] >> 7 | payload[7] << 1 | payload[8] << 9) & 0x07FF);
	channels[6]  = ((payload[8] >> 2 | payload[9] << 6) & 0x07FF);
	channels[7]  = ((payload[9] >> 5 | payload[10] << 3) & 0x07FF);

	channels[8]  = ((payload[11] | payload[12] << 8) & 0x07FF);
	channels[9]  = ((payload[12] >> 3 | payload[13] << 5) & 0x07FF);
	channels[10] = ((payload[13] >> 6 | payload[14] << 2 | payload[15] << 10) & 0x07FF);
	channels[11] = ((payload[15] >> 1 | payload[16] << 7) & 0x07FF);
	channels[12] = ((payload[16] >> 4 | payload[17] << 4) & 0x07FF);
	channels[13] = ((payload[17] >> 7 | payload[18] << 1 | payload[19] << 9) & 0x07FF);
	channels[14] = ((payload[19] >> 2 | payload[20] << 6) & 0x07FF);
	channels[15] = ((payload[20] >> 5 | payload[21] << 3) & 0x07FF);

	// ELRS CRSF 原始范围 172-1811，映射到标准 1000-2000 μs
	for (int i = 0; i < 16; i++) {
		channels[i] = (uint16_t)map(channels[i], 172, 1811, 1000, 2000);
	}
}

#endif // ARDUINO_ARCH_ESP32

void normalizeRC() {
#if defined(ARDUINO_ARCH_ESP32)
	if (rcProtocol == RC_PROTO_CRSF) {
		// CRSF 通道值已标准化为 1000-2000 μs（中位 1500），无需逐通道校准
		controlRoll = rollChannel < 0 ? 0 : mapf(channels[rollChannel], 1000, 2000, -1, 1);
		controlPitch = pitchChannel < 0 ? 0 : mapf(channels[pitchChannel], 1000, 2000, -1, 1);
		controlYaw = yawChannel < 0 ? 0 : mapf(channels[yawChannel], 1000, 2000, -1, 1);
		controlThrottle = throttleChannel < 0 ? 0 : mapf(channels[throttleChannel], 1000, 2000, 0, 1);
		controlMode = modeChannel < 0 ? NAN : mapf(channels[modeChannel], 1000, 2000, 0, 1); // 模式控制未映射时无效
		return;
	}
#endif

	// SBUS 使用校准值（中位为 0，最大为 1）
	float controls[16];
	for (int i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
	// 更新控制值
	controlRoll = rollChannel < 0 ? 0 : controls[rollChannel];
	controlPitch = pitchChannel < 0 ? 0 : controls[pitchChannel];
	controlYaw = yawChannel < 0 ? 0 : controls[yawChannel];
	controlThrottle = throttleChannel < 0 ? 0 : controls[throttleChannel];
	controlMode = modeChannel < 0 ? NAN : controls[modeChannel]; // 模式控制未映射时无效
}

// 根据遥控器通道控制外部 LED（SBUS 与 CRSF 共用）
void controlRemoteLED() {
	if (ledPin < 0 || ledChannel < 0) return;
	bool on = channels[ledChannel] > ledThreshold;
	if (on == remoteLedState) return; // 状态未变化
	remoteLedState = on;
	digitalWrite(ledPin, on ? HIGH : LOW);
}

// 手动控制遥控 LED（用于 CLI 命令）
void setRemoteLED(bool on) {
	if (ledPin < 0) {
		print("遥控 LED 未配置（请设置 RC_LED_PIN）\n");
		return;
	}
	remoteLedState = on;
	digitalWrite(ledPin, on ? HIGH : LOW);
	print("遥控 LED：%s\n", on ? "开" : "关");
}

// 获取遥控 LED 状态
bool getRemoteLEDState() {
	return remoteLedState;
}

// 实体遥控器是否活跃（最近 1 秒内有新帧）
// 用于控制优先级：默认实体遥控器优先时，判断其是否正在控制
bool isPhysicalRCActive() {
	return rcFresh && valid(rcTime) && (t - rcTime < 1);
}

void calibrateRC() {
#if defined(ARDUINO_ARCH_ESP32)
	if (rcProtocol == RC_PROTO_CRSF) {
		// CRSF 通道值已标准化，只需展示当前通道信息
		print("=== ELRS CRSF 通道检查 ===\n");
		print("通道映射：横滚 CH%d 俯仰 CH%d 偏航 CH%d 油门 CH%d 模式 CH%d LED CH%d\n",
			rollChannel, pitchChannel, yawChannel, throttleChannel, modeChannel, ledChannel);
		print("通道：");
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print("\n横滚：%g 俯仰：%g 偏航：%g 油门：%g 模式：%g\n",
			controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print("遥控 LED：%s（阈值 %.0f）\n", remoteLedState ? "开" : "关", ledThreshold);
		print("提示：CRSF 通道值已标准化，无需拨杆校准。若通道映射不对，请用 p RC_ROLL 等参数修改。\n");
		return;
	}
#endif

	if (rcRxPin < 0) {
		print("RC_RX_PIN = %d，请设置遥控接收引脚！\n", rcRxPin);
		return;
	}

	uint16_t zero[16]; // 用于零位
	uint16_t center[16]; // 用于中位
	uint16_t _[16]; // 用于未使用数据
	print("1/8 正在校准遥控器：将所有开关拨到默认位置 [3秒]\n");
	pause(3);
	calibrateRCChannel(NULL, _, zero, "2/8 拨动摇杆 [3秒]\n...     ...\n...     .o.\n.o.     ...\n");
	calibrateRCChannel(&throttleChannel, zero, _, "3/8 拨动摇杆 [3秒]\n.o.     ...\n...     .o.\n...     ...\n");
	calibrateRCChannel(NULL, _, center, "4/8 拨动摇杆 [3秒]\n...     ...\n.o.     .o.\n...     ...\n");
	calibrateRCChannel(&yawChannel, center, _, "5/8 拨动摇杆 [3秒]\n...     ...\n..o     .o.\n...     ...\n");
	calibrateRCChannel(&pitchChannel, zero, _, "6/8 拨动摇杆 [3秒]\n...     .o.\n...     ...\n.o.     ...\n");
	calibrateRCChannel(&rollChannel, zero, _, "7/8 拨动摇杆 [3秒]\n...     ...\n...     ..o\n.o.     ...\n");
	calibrateRCChannel(&modeChannel, zero, _, "8/8 将模式开关拨到最大 [3秒]\n");
	printRCCalibration();
}

void calibrateRCChannel(int *channel, uint16_t in[16], uint16_t out[16], const char *str) {
	print("%s", str);
	pause(3);
	for (int i = 0; i < 30; i++) readRC(); // 最多尝试更新30次
	memcpy(out, channels, sizeof(channels));

	if (channel == NULL) return; // 没有要校准的通道

	// 找出in和out之间变化最大的通道
	int ch = -1, diff = 0;
	for (int i = 0; i < 16; i++) {
		if (abs(out[i] - in[i]) > diff) {
			ch = i;
			diff = abs(out[i] - in[i]);
		}
	}
	if (ch >= 0 && diff > 10) { // 差异阈值为10
		*channel = ch;
		channelZero[ch] = in[ch];
		channelMax[ch] = out[ch];
	} else {
		*channel = -1;
	}
}

void printRCCalibration() {
	print("控制   通道   零值   最大值\n");
	print("横滚     %-7d%-7d%-7d\n", rollChannel, rollChannel < 0 ? 0 : channelZero[rollChannel], rollChannel < 0 ? 0 : channelMax[rollChannel]);
	print("俯仰     %-7d%-7d%-7d\n", pitchChannel, pitchChannel < 0 ? 0 : channelZero[pitchChannel], pitchChannel < 0 ? 0 : channelMax[pitchChannel]);
	print("偏航     %-7d%-7d%-7d\n", yawChannel, yawChannel < 0 ? 0 : channelZero[yawChannel], yawChannel < 0 ? 0 : channelMax[yawChannel]);
	print("油门     %-7d%-7d%-7d\n", throttleChannel, throttleChannel < 0 ? 0 : channelZero[throttleChannel], throttleChannel < 0 ? 0 : channelMax[throttleChannel]);
	print("模式     %-7d%-7d%-7d\n", modeChannel, modeChannel < 0 ? 0 : channelZero[modeChannel], modeChannel < 0 ? 0 : channelMax[modeChannel]);
}
