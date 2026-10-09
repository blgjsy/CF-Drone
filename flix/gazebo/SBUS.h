// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// SBUS库模拟桩，使模拟器能够与rc.ino一起编译

#include "joystick.h"

struct SBUSData {
	int16_t ch[16];
};

class SBUS {
public:
	SBUS(HardwareSerial& bus, const bool inv = true) {};
	SBUS(HardwareSerial& bus, const int8_t rxpin, const int8_t txpin, const bool inv = true) {};
	void begin(int rxpin = -1, int txpin = -1, bool inv = true, bool fast = false) {};
	bool read() { return joystickInit(); };
	void getChannels(uint16_t (&channels)[16]) const {
		int16_t ch[16];
		joystickGet(ch);
		for (int i = 0; i < 16; i++) {
			channels[i] = map(ch[i], -32768, 32767, 1000, 2000); // 转换为脉宽格式
		}
	};
};
