// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 低通滤波器实现

#pragma once

template <typename T> // 使用模板使滤波器可用于标量和向量值
class LowPassFilter {
public:
	float alpha; // 平滑常数，1 表示滤波器禁用
	T output;

	LowPassFilter(float alpha): alpha(alpha) {};

	T update(const T input) {
		if (!init) {
			init = true;
			return output = input;
		}
		return output += alpha * (input - output);
	}

	void setCutOffFrequency(float cutOffFreq, float dt) {
		alpha = 1 - exp(-2 * PI * cutOffFreq * dt);
	}

	void reset() {
		init = false;
	}

private:
	bool init = false;
};
