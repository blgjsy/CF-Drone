// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 可选功能开关（编译期启用/禁用）
//
// 仅在 ESP32 Arduino 平台启用这些功能；gazebo 仿真器等非 ESP32 构建下自动关闭。
// 因此本头文件可被每个使用这些宏的 .ino 文件安全地包含（自包含，不依赖拼接顺序）。

#pragma once

#if defined(ARDUINO_ARCH_ESP32)

// 气压计定高功能（BMP581）
// 需要安装库：SparkFun BMP581 Arduino Library
// 未安装库时请改为 0 以通过编译；定高命令仍会提示"不可用"
#define BARO_ENABLED 1

// 网页遥控器功能（依赖 WiFi 热点，端口 80）
// 运行期还可用命令 "web on|off" 开启/关闭
#define WEB_RC_ENABLED 1

#else // 非 ESP32 Arduino 平台（如 gazebo 仿真器）

#define BARO_ENABLED 0
#define WEB_RC_ENABLED 0

#endif
