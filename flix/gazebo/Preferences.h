// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 仿真用ESP32 Preferences库的部分实现

#include <map>
#include <fstream>
#include "util.h"

class Preferences {
private:
	std::map<std::string, float> storage;
	std::string storagePath;

	void readFromFile() {
		std::ifstream file(storagePath);
		std::string key, value;
		while (file >> key >> value) {
			storage[key] = std::stof(value); // 使用stof以支持NaN和Infinity
		}
	}

	void writeToFile() {
		std::ofstream file(storagePath);
		for (auto &pair : storage) {
			file << pair.first << " " << pair.second << std::endl;
		}
	}

public:
	bool begin(const char *name, bool readOnly = false, const char *partition_label = NULL) {
		storagePath = getPluginPath().parent_path() / (std::string(name) + ".txt");
		gzmsg << "参数存储已初始化：" << storagePath << std::endl;
		readFromFile();
		return true;
	}

	bool isKey(const char *key) {
		return storage.find(key) != storage.end();
	}

	size_t putFloat(const char *key, float value) {
		storage[key] = value;
		writeToFile();
		return sizeof(value);
	}

	float getFloat(const char *key, float defaultValue = NAN) {
		if (!isKey(key)) {
			return defaultValue;
		}
		return storage[key];
	}

	bool clear() {
		storage.clear();
		writeToFile();
		return true;
	}
};
