// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 时间相关函数

float t = NAN; // 当前时间，秒
float dt; // 与上一步的时间差，秒
float loopRate; // 频率，赫兹

void step() {
	float now = micros() / 1000000.0;
	dt = now - t;
	t = now;

	if (!(dt > 0)) {
		dt = 0; // 在第一步和重置时将dt视为0
	}

	computeLoopRate();
}

void computeLoopRate() {
	static float windowStart = 0;
	static uint32_t rate = 0;
	rate++;
	if (t - windowStart >= 1) { // 1秒窗口
		loopRate = rate;
		windowStart = t;
		rate = 0;
	}
}
