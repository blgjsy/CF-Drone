// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Time related functions

float loopRate; // Hz

// 重命名为不同的函数名，避免与flix.ino中的step冲突
void calculateTimeStep() {
	static unsigned long lastStepTime = 0;
	unsigned long currentTime = micros();
	
	if (lastStepTime > 0) {
		dt = (currentTime - lastStepTime) / 1000000.0f;
	} else {
		dt = 0.002f; // 初始值，约500Hz
	}
	
	// 更新全局时间t
	extern float t;
	t += dt;
	lastStepTime = currentTime;

	computeLoopRate();
}

void computeLoopRate() {
	static float windowStart = 0;
	static uint32_t rate = 0;
	extern float t;
	rate++;
	if (t - windowStart >= 1) { // 1 second window
		loopRate = rate;
		windowStart = t;
		rate = 0;
	}
}