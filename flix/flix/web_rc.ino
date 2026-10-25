// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 网页遥控器服务端（手机浏览器通过 WiFi 热点控制无人机）
// 支持：多设备互斥（先打开者优先）、会话超时、实体/网页控制优先级

#if WEB_RC_ENABLED

#include <WebServer.h>
#include <WiFi.h>
#include "web_rc_html.h"
#include "util.h"

#define WEB_RC_TIMEOUT_MS 5000 // 连接超时：最后一次收包超过该时间视为断连

// ==================== 全局状态 ====================
int webServerOn = 1;          // 网页服务开关（参数 WEB_ENABLED，0=关闭 1=开启）
bool webRCEnabled = false;    // 当前是否有有效的网页遥控连接
bool useWebRC = false;        // 是否正在使用网页遥控
bool webRCUpdated = false;    // 是否收到过至少一次摇杆数据
bool webConsoleEnabled = false; // Web 调试控制台开关
float webRCRoll = 0, webRCPitch = 0, webRCYaw = 0, webRCThrottle = 0; // 处理后数值
uint16_t webRCButtons = 0;    // 按钮位掩码：bit0解锁 bit1上锁 bit2急停 bit3模式 bit4定高
unsigned long webRCLastUpdate = 0;
char webRCWarnMsg[64] = "";   // 发送给前端的警告消息，发出后自动清空

// ==================== 控制优先级 ====================
int ctlPriority = 0; // 0=实体遥控器优先（默认），1=网页遥控器优先

// ==================== 多设备会话管理 ====================
int webSessionTimeout = 60;      // 会话无操作超时（秒），0=永不过期
char webSessionId[17] = "";      // 当前控制会话 ID（页面加载时随机生成）
unsigned long webSessionLastPacket = 0;   // 拥有者最后一次收到任意包的时间
unsigned long webSessionLastActivity = 0; // 拥有者最后一次控制操作（摇杆/按钮）的时间

// 缩放与死区
const float WEB_STICK_MAX = 30.0f;  // 姿态轴满偏角度(°)，对应前端满偏
const float WEB_RAW_MAX = 100.0f;   // 前端摇杆归一化满偏值
float webRCStickScale = 0.85f;      // 横滚/俯仰灵敏度
float webRCYawScale = 0.68f;        // 偏航灵敏度（偏航惯性小，单独偏低）
float webRCStickDeadzone = 0.06f;   // 姿态轴死区（归一化比例）
float webRCThrottleDeadzone = 0.06f; // 油门低端死区（比例）

static float lastValidThrottle = 0, lastValidRoll = 0, lastValidPitch = 0, lastValidYaw = 0;
static int lastProcType = 0;
static int lastProcButtonIdx = -1;
static int lastProcButtonState = -1;
static char lastSid[17] = "";
// 解析后待应用的暂存值
static float pendingThrottle = 0, pendingRoll = 0, pendingPitch = 0, pendingYaw = 0;
static int pendingBtnIdx = -1, pendingBtnState = -1;

// ==================== Web 服务器 ====================
WebServer webRCServer(80); // 标准 HTTP 端口，手机浏览器无需输入端口号

// ==================== 控制台日志缓冲 ====================
#define WEB_CONSOLE_LINES 30
#define WEB_CONSOLE_LINE_LEN 160
static char webConsoleBuf[WEB_CONSOLE_LINES][WEB_CONSOLE_LINE_LEN];
static int webConsoleTail = 0, webConsoleFilled = 0, webConsoleTotal = 0;

#define WEB_CMD_QUEUE_SIZE 4
#define WEB_CMD_LEN 64
static char webCmdQueue[WEB_CMD_QUEUE_SIZE][WEB_CMD_LEN];
static int webCmdHead = 0, webCmdTail = 0, webCmdCount = 0;

// 按行写入日志缓冲（print 的钩子）
void webLog(const char* msg) {
	const char* start = msg;
	while (*start) {
		const char* end = strchr(start, '\n');
		int len = end ? (int)(end - start) : (int)strlen(start);
		if (len > 0) {
			int copy = min(len, WEB_CONSOLE_LINE_LEN - 1);
			strncpy(webConsoleBuf[webConsoleTail], start, copy);
			webConsoleBuf[webConsoleTail][copy] = '\0';
			webConsoleTail = (webConsoleTail + 1) % WEB_CONSOLE_LINES;
			if (webConsoleFilled < WEB_CONSOLE_LINES) webConsoleFilled++;
			webConsoleTotal++;
		}
		if (!end) break;
		start = end + 1;
	}
}

// 网页控制台命令入队（在主循环中执行，避免阻塞 HTTP 回调）
bool enqueueConsoleCmd(const char* cmd) {
	if (!cmd || !*cmd) return false;
	if (webCmdCount >= WEB_CMD_QUEUE_SIZE) return false;
	strncpy(webCmdQueue[webCmdTail], cmd, WEB_CMD_LEN - 1);
	webCmdQueue[webCmdTail][WEB_CMD_LEN - 1] = '\0';
	webCmdTail = (webCmdTail + 1) % WEB_CMD_QUEUE_SIZE;
	webCmdCount++;
	return true;
}

void processConsoleCommandQueue() {
	if (webCmdCount <= 0) return;
	String cmd = webCmdQueue[webCmdHead];
	webCmdHead = (webCmdHead + 1) % WEB_CMD_QUEUE_SIZE;
	webCmdCount--;
	doCommand(cmd, false);
}

// ==================== 摇杆处理 ====================

// 归一化空间死区：超出死区的部分线性重映射到满幅
float webApplyDeadzone(float norm, float deadzone) {
	if (fabsf(norm) < deadzone) return 0;
	float sign = (norm > 0) ? 1.0f : -1.0f;
	return sign * (fabsf(norm) - deadzone) / (1.0f - deadzone);
}

// 姿态轴：raw ∈ [-100, 100] → ±30°
float webProcessAxis(float raw, float& lastValid) {
	if (isnan(raw) || isinf(raw) || fabsf(raw) > 1000.0f) return lastValid;
	float norm = constrain(raw, -WEB_RAW_MAX, WEB_RAW_MAX) / WEB_RAW_MAX;
	norm = webApplyDeadzone(norm, webRCStickDeadzone);
	lastValid = norm * WEB_STICK_MAX;
	return lastValid;
}

// 油门轴：raw ∈ [-100, 100] → 0~100%，底部为 0
float webProcessThrottle(float raw) {
	if (isnan(raw) || isinf(raw) || fabsf(raw) > 1000.0f) return lastValidThrottle;
	raw = constrain(raw, -WEB_RAW_MAX, WEB_RAW_MAX);
	float pct = (raw + WEB_RAW_MAX) / (2.0f * WEB_RAW_MAX) * 100.0f;
	if (pct < webRCThrottleDeadzone * 100.0f) pct = 0;
	lastValidThrottle = constrain(pct, 0.0f, 100.0f);
	return lastValidThrottle;
}

// 实体遥控器是否优先接管（实体遥控优先 且 实体遥控器活跃）
bool webPhysicalOverride() {
	return ctlPriority == 0 && isPhysicalRCActive();
}

// 写入统一控制变量（与 SBUS/CRSF 同路径）；实体遥控优先时网页输入被忽略
void setWebRCInput(float roll, float pitch, float yaw, float throttle, uint16_t buttons) {
	float pThrottle = webProcessThrottle(throttle);
	float pYaw = webProcessAxis(yaw, lastValidYaw);
	float pPitch = webProcessAxis(pitch, lastValidPitch);
	float pRoll = webProcessAxis(roll, lastValidRoll);

	webRCThrottle = pThrottle;
	webRCYaw = pYaw;
	webRCPitch = pPitch;
	webRCRoll = pRoll;
	webRCButtons = buttons;
	webRCLastUpdate = millis();
	webRCUpdated = true;

	if (webPhysicalOverride()) {
		setWebRCWarn("实体遥控器控制中，网页输入无效");
		return;
	}

	controlRoll = constrain(pRoll * webRCStickScale / WEB_STICK_MAX, -1.0f, 1.0f);
	controlPitch = constrain(pPitch * webRCStickScale / WEB_STICK_MAX, -1.0f, 1.0f);
	controlYaw = constrain(pYaw * webRCYawScale / WEB_STICK_MAX, -1.0f, 1.0f);
	controlThrottle = pThrottle / 100.0f;
	controlMode = NAN; // 网页遥控没有模式通道，模式由按钮控制
	controlTime = t;
}

bool isWebRCEnabled() {
	return webRCUpdated && (millis() - webRCLastUpdate < WEB_RC_TIMEOUT_MS);
}

bool isUsingWebRC() {
	return isWebRCEnabled() && !webPhysicalOverride();
}

void setWebRCWarn(const char* msg) {
	if (strcmp(webRCWarnMsg, msg) == 0) return; // 去重，避免重复提示
	strncpy(webRCWarnMsg, msg, sizeof(webRCWarnMsg) - 1);
	webRCWarnMsg[sizeof(webRCWarnMsg) - 1] = '\0';
}

// ==================== 极简 JSON 解析 ====================

const char* webFindJsonValue(const char* json, const char* key) {
	const char* p = strstr(json, key);
	if (!p) return nullptr;
	p = strchr(p, ':');
	if (!p) return nullptr;
	p++;
	while (*p == ' ' || *p == '"') p++;
	return p;
}

// 提取字符串字段（如 "sid":"abcd1234"）
void webGetStringValue(const char* json, const char* key, char* out, int maxLen) {
	out[0] = '\0';
	const char* v = webFindJsonValue(json, key);
	if (!v) return;
	int i = 0;
	while (*v && *v != '"' && *v != ',' && *v != '}' && i < maxLen - 1) {
		out[i++] = *v++;
	}
	out[i] = '\0';
}

// 解析（不直接应用，由 handleWebRCRequest 根据会话/优先级决定是否应用）
bool webHandleJSON(String& body) {
	if (body.length() == 0 || body.indexOf('{') == -1) return false;
	const char* json = body.c_str();
	const char* typePos = strstr(json, "\"t\":");
	if (!typePos) return false;
	int type = atoi(typePos + 4);
	const char* v;

	webGetStringValue(json, "\"sid\"", lastSid, sizeof(lastSid));
	lastProcType = type;
	lastProcButtonIdx = -1;
	lastProcButtonState = -1;
	pendingBtnIdx = -1;
	pendingBtnState = -1;

	switch (type) {
		case 1: { // 摇杆数据
			float th = 0, r = 0, p = 0, y = 0;
			if ((v = webFindJsonValue(json, "\"th\""))) th = atof(v);
			if ((v = webFindJsonValue(json, "\"r\""))) r = atof(v);
			if ((v = webFindJsonValue(json, "\"p\""))) p = atof(v);
			if ((v = webFindJsonValue(json, "\"y\""))) y = atof(v);
			pendingThrottle = th;
			pendingRoll = r;
			pendingPitch = p;
			pendingYaw = y;
			break;
		}
		case 2: { // 按钮事件
			int idx = 0, state = 0;
			if ((v = webFindJsonValue(json, "\"b\""))) idx = atoi(v);
			if ((v = webFindJsonValue(json, "\"s\""))) state = atoi(v);
			pendingBtnIdx = idx;
			pendingBtnState = state;
			break;
		}
		case 4: // 心跳
			break;
	}
	return true;
}

// 会话管理：先打开（先发请求）的设备成为拥有者；
// 拥有者断连或超过 webSessionTimeout 秒无操作后，其他设备可接管
bool webSessionUpdate() {
	bool isOwner = false;

	if (webSessionId[0] == '\0') {
		// 无会话：当前设备认领（先到先得）
		if (lastSid[0]) {
			strncpy(webSessionId, lastSid, 16);
			webSessionId[16] = '\0';
			webSessionLastPacket = webSessionLastActivity = millis();
			isOwner = true;
			print("网页会话建立：%s\n", webSessionId);
		}
	} else if (strcmp(webSessionId, lastSid) == 0) {
		// 拥有者本人：续期
		isOwner = true;
		webSessionLastPacket = millis();
		if (lastProcType == 1 || lastProcType == 2) {
			webSessionLastActivity = millis();
		}
	} else {
		// 其他设备：检查当前会话是否已失效
		bool lost = (millis() - webSessionLastPacket) > WEB_RC_TIMEOUT_MS;
		bool idle = webSessionTimeout > 0 &&
			(millis() - webSessionLastActivity) > (unsigned long)webSessionTimeout * 1000UL;
		if (lost || idle) {
			strncpy(webSessionId, lastSid, 16);
			webSessionId[16] = '\0';
			webSessionLastPacket = webSessionLastActivity = millis();
			isOwner = true;
			print("网页会话易主：%s\n", webSessionId);
		}
	}
	return isOwner;
}

// ==================== HTTP 请求处理 ====================

void handleWebRCRequest() {
	if (!webRCServer.hasArg("plain")) {
		webRCServer.send(400, "application/json", "{\"e\":\"no data\"}");
		return;
	}
	String body = webRCServer.arg("plain");
	if (!webHandleJSON(body)) {
		webRCServer.send(400, "application/json", "{\"e\":\"parse failed\"}");
		return;
	}

	bool owner = webSessionUpdate();
	bool physical = webPhysicalOverride();

	if (owner) {
		switch (lastProcType) {
			case 1:
				setWebRCInput(pendingRoll, pendingPitch, pendingYaw, pendingThrottle, webRCButtons);
				break;
			case 2:
				if (pendingBtnIdx >= 0 && pendingBtnIdx < 16) {
					if (pendingBtnState) webRCButtons |= (1 << pendingBtnIdx);
					else                 webRCButtons &= ~(1 << pendingBtnIdx);
					lastProcButtonIdx = pendingBtnIdx;
					lastProcButtonState = pendingBtnState;
					webRCLastUpdate = millis();
					webRCUpdated = true;
					if (physical) setWebRCWarn("实体遥控器控制中，网页输入无效");
				}
				break;
			case 4:
				webRCLastUpdate = millis();
				webRCUpdated = true;
				break;
		}
	} else {
		// 旁观者：不应用控制，仅提示
		if (lastProcType == 1 || lastProcType == 2) {
			setWebRCWarn("另一台设备正在控制");
		}
	}

	bool baro = false;
#if BARO_ENABLED
	baro = isBaroReady();
#endif

	char resp[460];
	bool deliverWarn = (lastProcType == 2 || lastProcType == 4) && webRCWarnMsg[0];
	if (deliverWarn) {
		snprintf(resp, sizeof(resp),
			"{\"s\":\"ok\",\"m\":%d,\"arm\":%d,\"ah\":%d,\"baro\":%s,\"ow\":%d,\"phy\":%d,\"prio\":%d,\"rt\":%d,\"bi\":%d,\"bs\":%d,\"warn\":\"%s\"}",
			mode, (int)armed, (int)altHoldEngaged, baro ? "true" : "false",
			(int)owner, (int)physical, ctlPriority,
			lastProcType, lastProcButtonIdx, lastProcButtonState, webRCWarnMsg);
		webRCWarnMsg[0] = '\0';
	} else {
		snprintf(resp, sizeof(resp),
			"{\"s\":\"ok\",\"m\":%d,\"arm\":%d,\"ah\":%d,\"baro\":%s,\"ow\":%d,\"phy\":%d,\"prio\":%d,\"rt\":%d,\"bi\":%d,\"bs\":%d}",
			mode, (int)armed, (int)altHoldEngaged, baro ? "true" : "false",
			(int)owner, (int)physical, ctlPriority,
			lastProcType, lastProcButtonIdx, lastProcButtonState);
	}
	webRCServer.send(200, "application/json", resp);
}

// ==================== 初始化 ====================

void setupWebRC() {
	print("正在初始化网页遥控器\n");
	lastValidThrottle = 0;
	lastValidRoll = lastValidPitch = lastValidYaw = 0;

	// 首页
	webRCServer.on("/", HTTP_GET, []() {
		webRCServer.send(200, "text/html", webRCIndexHtml);
	});

	// 控制数据入口
	webRCServer.on("/web_rc", HTTP_POST, handleWebRCRequest);
	webRCServer.on("/web_rc/heartbeat", HTTP_POST, handleWebRCRequest);

	// 状态查询
	webRCServer.on("/web_rc/status", HTTP_GET, []() {
		float alt = 0;
		bool baro = false;
		float v = valid(voltage) ? voltage : 0.0f; // 防止 NaN 破坏 JSON
#if BARO_ENABLED
		baro = isBaroReady();
		if (baro) alt = getAltitude();
#endif
		String sidArg = webRCServer.arg("sid");
		bool owner = sidArg.length() > 0 && strcmp(webSessionId, sidArg.c_str()) == 0;
		bool phy = isPhysicalRCActive();

		char json[480];
		snprintf(json, sizeof(json),
			"{\"enabled\":%s,\"active\":%s,\"voltage\":%.2f,\"alt\":%.2f,\"baro\":%s,"
			"\"althold\":%s,\"mode\":%d,\"armed\":%s,\"owner\":%s,\"phy\":%s,\"prio\":%d}",
			isWebRCEnabled() ? "true" : "false",
			isUsingWebRC() ? "true" : "false",
			v, alt, baro ? "true" : "false",
			altHoldEngaged ? "true" : "false", mode, armed ? "true" : "false",
			owner ? "true" : "false", phy ? "true" : "false", ctlPriority);
		webRCServer.send(200, "application/json", json);
	});

	// 调试控制台（增量拉取日志）
	webRCServer.on("/console", HTTP_GET, []() {
		int since = -1, limit = 20;
		if (webRCServer.hasArg("since")) since = webRCServer.arg("since").toInt();
		if (webRCServer.hasArg("limit")) limit = webRCServer.arg("limit").toInt();
		if (limit <= 0) limit = 20;
		if (limit > WEB_CONSOLE_LINES) limit = WEB_CONSOLE_LINES;

		int availableFrom = max(0, webConsoleTotal - webConsoleFilled);
		int sendFrom = (since >= 0) ? since : availableFrom;
		if (sendFrom < availableFrom) sendFrom = availableFrom;
		if (sendFrom > webConsoleTotal) sendFrom = webConsoleTotal;
		int sendTo = min(webConsoleTotal, sendFrom + limit);

		String json = "{\"total\":";
		json += webConsoleTotal;
		json += ",\"next\":";
		json += sendTo;
		json += ",\"has_more\":";
		json += (sendTo < webConsoleTotal) ? "true" : "false";
		json += ",\"lines\":[";
		bool first = true;
		for (int i = sendFrom; i < sendTo; i++) {
			int idx = i % WEB_CONSOLE_LINES;
			if (!first) json += ",";
			first = false;
			json += "\"";
			String line = webConsoleBuf[idx];
			line.replace("\\", "\\\\");
			line.replace("\"", "\\\"");
			json += line + "\"";
		}
		json += "]}";
		webRCServer.send(200, "application/json", json);
	});

	webRCServer.on("/console/cmd", HTTP_POST, []() {
		String cmd = webRCServer.arg("plain");
		cmd.trim();
		if (cmd.length() == 0) {
			webRCServer.send(400, "application/json", "{\"ok\":0,\"e\":\"empty command\"}");
			return;
		}
		char buf[WEB_CONSOLE_LINE_LEN];
		snprintf(buf, sizeof(buf), "> %s", cmd.c_str());
		webLog(buf);
		if (!enqueueConsoleCmd(cmd.c_str())) {
			webLog("! 命令队列已满\n");
			webRCServer.send(503, "application/json", "{\"ok\":0,\"e\":\"queue full\"}");
			return;
		}
		webRCServer.send(200, "application/json", "{\"ok\":1}");
	});

	webRCServer.on("/console/enable", HTTP_POST, []() {
		webConsoleEnabled = true;
		webRCServer.send(200, "application/json", "{\"ok\":1}");
	});

	webRCServer.on("/console/disable", HTTP_POST, []() {
		webConsoleEnabled = false;
		webRCServer.send(200, "application/json", "{\"ok\":1}");
	});

	if (webServerOn) {
		webRCServer.begin();
		print("✓ 网页遥控器已启动：http://192.168.4.1\n");
	} else {
		print("网页遥控器已禁用（web on 可开启）\n");
	}
}

// ==================== 运行时开关 ====================

void setWebServer(bool on) {
	if (on == (webServerOn != 0)) return;
	webServerOn = on;
	if (on) {
		webRCServer.begin();
		print("✓ 网页遥控器已开启：http://192.168.4.1\n");
	} else {
		bool wasUsing = useWebRC;
		webRCServer.stop();
		webRCUpdated = false;
		webRCLastUpdate = 0;
		webRCEnabled = useWebRC = false;
		webSessionId[0] = '\0'; // 关闭网页时同时清空会话
		print("网页遥控器已关闭\n");
		if (armed && wasUsing) {
			armed = false;
			controlThrottle = 0;
			print("⚠️ 网页遥控已关闭，无人机已上锁\n");
		}
	}
}

// 参数 WEB_ENABLED 修改后的回调
void applyWebEnabled() {
	setWebServer(webServerOn != 0);
}

void printWebServerStatus() {
	print("网页遥控器：%s\n", webServerOn ? "开启" : "关闭");
	print("地址：http://192.168.4.1\n");
	print("连接：%s\n", isWebRCEnabled() ? "已连接" : "未连接");
	print("优先级：%s\n", ctlPriority == 0 ? "实体遥控器优先" : "网页遥控器优先");
	print("会话：%s（无操作超时 %d 秒）\n", webSessionId[0] ? webSessionId : "无", webSessionTimeout);
}

// ==================== 主循环调用 ====================

void readWebRC() {
	if (!webServerOn) {
		webRCEnabled = useWebRC = false;
		return;
	}

	webRCServer.handleClient();

	if (isWebRCEnabled()) {
		webRCEnabled = useWebRC = true;
	} else if (webRCEnabled) {
		// 从活跃变为超时：安全处理
		webRCEnabled = useWebRC = false;
		print("⚠️ 网页遥控连接超时，已上锁\n");
		armed = false;
		controlThrottle = 0;
		disengageAltHold("网页遥控超时");
	} else {
		webRCEnabled = useWebRC = false;
	}
}

#else // WEB_RC_ENABLED

void setupWebRC() {}
void readWebRC() {}
void processConsoleCommandQueue() {}
void setWebServer(bool on) {}
void applyWebEnabled() {}
void printWebServerStatus() {}

#endif // WEB_RC_ENABLED
