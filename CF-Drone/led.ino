// 板载LED灯控制
// Board's LED control

#include "board_config.h"

#if BOARD_LED_ENABLED
// 如果启用了板载LED

#define BLINK_PERIOD      500000  // 慢闪：500ms 半周期 → 1 Hz
#define BLINK_FAST_PERIOD  62500  // 快闪：62.5ms 半周期 → 8 Hz


// ---- 外部依赖声明 ----
extern bool armed;
extern float t;
extern float controlTime;
extern float rcLossTimeout;
extern float thrustTarget;    // control.ino
extern float batteryVoltage;  // battery.ino
extern bool isInverted; // safety.ino
#if WEB_RC_ENABLED
extern bool webRCEnabled;
extern bool useWebRC;
bool isUsingWebRC();
#endif

void setupLED() {
	pinMode(BOARD_LED_PIN, OUTPUT);
	digitalWrite(BOARD_LED_PIN, BOARD_LED_INVERTED ? HIGH : LOW); // 初始熄灭
	//                                    条件     ? 成立  : 不成立
	//                          false 高电平点亮, true 低电平点亮
}

void setLED(bool on) {
	static bool state = false;
	if (on == state) {
		return; // 如果[你想让LED变成什么状态]:on与当前状态state相同，则直接跳过
	}
	digitalWrite(BOARD_LED_PIN, (on ^ BOARD_LED_INVERTED) ? HIGH : LOW);
	//                              ^ 逻辑异或，自动处理高低电平
	state = on;
}

void blinkLED() {
	setLED(micros() / BLINK_PERIOD % 2);
}

// 电池告警：按飞行状态选择阈值
// 飞行中（油门输出thrustTarget >= 0.15）→ L2（2.8V），L1 在飞行中不适用
// 未解锁 / 解锁怠速 → L1（3.4V）
bool batteryAlertActive() {
	if (batteryVoltage <= VBAT_ABSENT_THRESHOLD) return false;
	// 电池未接入，忽略所有告警。电压太低说明根本没接电池
	bool flying = armed && thrustTarget >= 0.15f;
	// 0.15f的f是浮点数字面的后缀，表示是float类型，避免编译器将其当作double类型处理
	// 飞行中判定：解锁且推力≥0.15
	if (flying) return batteryVoltage < VBAT_LOW_THRESHOLD;   // L2：飞行中
					//     当前电压    < 飞行中低电告警门限，返回布尔值
	else        return batteryVoltage < VBAT_WARN_THRESHOLD;   // L1：未解锁/怠速
	                //    当前电压    < 空载/怠速预警门限，返回布尔值
}

// 检测是否有任意告警（低电 / 遥控失联 / 倒置）
bool ledAlertActive() {
	// 倒置检测
	if (isInverted) return true;

	// 遥控失联检测（SBUS RC，仅解锁后）
	if (controlTime != 0 && armed && (t - controlTime > rcLossTimeout)) return true;
	//  上次控制更新的时间不为0 且 已解锁 且 当前时间 - 上次控制更新的时间 > 遥控失联超时时间

#if WEB_RC_ENABLED
	// Web RC 失联检测：已激活但超时
	if (webRCEnabled && useWebRC && !isUsingWebRC()) return true;
	//  Web RC已激活 且 正在使用Web RC 且 Web RC 失联（超时） → 返回 true
#endif

	if (batteryAlertActive()) return true;

	return false;
}

// 主循环调用：根据飞行状态驱动 LED
void updateLED() {
	if (!armed) {
		// 未解锁状态：根据电池告警状态快闪或常灭
		if (batteryAlertActive()) {
			/** bool batteryAlertActive()
			* 电池告警：按飞行状态选择阈值
			* 飞行中（油门输出thrustTarget >= 0.15）→ L2（2.8V），L1 在飞行中不适用
			* 未解锁 / 解锁怠速 → L1（3.4V）
			*/
			setLED(micros() / BLINK_FAST_PERIOD % 2); // 解锁前低电：快闪
		} else {
			setLED(false); // 正常待机：常灭
		}
		return;
	}
	if (ledAlertActive()) {
		setLED(micros() / BLINK_FAST_PERIOD % 2); // 任何告警：快闪 8Hz
	} else {
		setLED(micros() / BLINK_PERIOD % 2); // 正常飞行：慢闪 1Hz
	}
}

#else // BOARD_LED_ENABLED == 0（ESP32-C3 无板载 LED）

void setupLED() {}
void setLED(bool on) {}
void updateLED() {}

#endif // BOARD_LED_ENABLED
