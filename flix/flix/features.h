// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 可选功能开关（编译期启用/禁用）

#pragma once

// 气压计定高功能（BMP581）
// 需要安装库：SparkFun BMP581 Arduino Library
// 未安装库时请改为 0 以通过编译；定高命令仍会提示"不可用"
#define BARO_ENABLED 1

// 网页遥控器功能（依赖 WiFi 热点，端口 80）
// 运行期还可用命令 "web on|off" 开启/关闭
#define WEB_RC_ENABLED 1
