// web_rc_html.h
// Web RC HTML页面定义 - 气压定高版 - 性能优化版 by @刘振生 QQ:2315960

#if WEB_RC_ENABLED

const char webRCIndexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>Flix无人机遥控器 - 气压定高版</title>
<style>
/*======== 通用样式 ========*/
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;touch-action:none;user-select:none}
body{font-family:Arial,"Microsoft YaHei",sans-serif;background:linear-gradient(135deg,#0f2027 0%,#203a43 50%,#2c5364 100%);color:#fff;overflow:hidden;height:100vh;width:100vw}
.container{width:100%;height:100%;display:flex;flex-direction:column;padding:15px;max-width:1200px;margin:0 auto}

/*======== 顶部状态栏 ========*/
.header {
  text-align: center;
  padding: 8px 15px;
  background: rgba(0,0,0,.5);
  border-radius: 15px;
  margin-bottom: 12px;
  border: 2px solid rgba(76,201,240,.4);
  box-shadow: 0 4px 16px rgba(0,0,0,.4);
  backdrop-filter: blur(10px)
}

.header h1 {
  font-size: 1.4rem;
  color: #4cc9f0;
  text-shadow: 0 0 10px rgba(76,201,240,.8);
  margin-bottom: 5px;
}

.status-bar {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
  margin-top: 5px;
}

.status-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 5px 10px;
  background: rgba(0,0,0,.3);
  border-radius: 8px;
  border: 1px solid rgba(255,255,255,.1);
  font-size: 0.8rem;
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}

.network-info {
  display: flex;
  gap: 4px;
  font-size: 0.7rem;
  color: #a9def9;
}

.altitude-status {
  background: linear-gradient(145deg, #7209b7, #560bad);
  border: 1px solid #b5179e;
  color: #fff;
  font-weight: bold;
  animation: pulse-alt 2s infinite;
}

@keyframes pulse-alt {
  0% { box-shadow: 0 0 0 0 rgba(114,9,183,0.7); }
  70% { box-shadow: 0 0 0 10px rgba(114,9,183,0); }
  100% { box-shadow: 0 0 0 0 rgba(114,9,183,0); }
}

/*======== 内容区 ========*/
.content{display:flex;flex:1;gap:15px;height:60vh;min-height:400px}
.joystick-container{flex:1;display:flex;flex-direction:column;justify-content:center;align-items:center;background:rgba(0,0,0,.4);border-radius:20px;padding:15px;border:2px solid rgba(67,97,238,.5);box-shadow:inset 0 0 30px rgba(0,0,0,.5)}
.joystick-title{font-size:1.1rem;color:#72efdd;margin-bottom:12px;text-shadow:0 0 10px rgba(114,239,221,.6)}
.joystick-wrapper{width:100%;height:220px;display:flex;justify-content:center;align-items:center;position:relative}
.joystick{width:200px;height:200px;background:radial-gradient(circle at 30% 30%,rgba(255,255,255,.15),rgba(0,0,0,.3));border-radius:50%;position:relative;border:3px solid #4361ee;box-shadow:inset 0 0 25px rgba(67,97,238,.5),0 8px 25px rgba(0,0,0,.5);overflow:hidden}
.joystick::before{content:'';position:absolute;top:50%;left:50%;width:2px;height:100%;background:linear-gradient(to bottom,transparent,rgba(255,255,255,.2),transparent);transform:translate(-50%,-50%)}
.joystick::after{content:'';position:absolute;top:50%;left:50%;width:100%;height:2px;background:linear-gradient(to right,transparent,rgba(255,255,255,.2),transparent);transform:translate(-50%,-50%)}
.joystick-knob{width:60px;height:60px;background:radial-gradient(circle at 30% 30%,#fff,#b8c6ff);border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);border:2px solid #fff;box-shadow:0 4px 15px rgba(0,0,0,.5),inset 0 0 10px rgba(255,255,255,.7);cursor:move;transition:transform .1s ease-out;z-index:10}
.coordinates{margin-top:12px;text-align:center;font-family:'Courier New',monospace;font-size:0.9rem;color:#a9def9;display:flex;flex-direction:column;gap:3px}

/*======== 按钮区 ========*/
.buttons-container{flex:.8;display:grid;grid-template-columns:repeat(4,1fr);grid-template-rows:repeat(4,1fr);gap:8px;padding:12px;background:rgba(0,0,0,.4);border-radius:20px;border:2px solid rgba(247,37,133,.4);box-shadow:inset 0 0 20px rgba(0,0,0,.5)}
.button{background:linear-gradient(145deg,#3a0ca3,#4361ee);border:none;border-radius:10px;color:#fff;font-size:.85rem;font-weight:bold;display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center;padding:10px 5px;cursor:pointer;transition:all .15s cubic-bezier(.4,0,.2,1);box-shadow:0 3px 10px rgba(0,0,0,.3),inset 0 1px 0 rgba(255,255,255,.2);position:relative}
.button:hover{background:linear-gradient(145deg,#4361ee,#3a0ca3);transform:translateY(-1px)}
.button.active{background:linear-gradient(145deg,#f72585,#b5179e);box-shadow:0 0 15px rgba(247,37,133,.6),inset 0 1px 0 rgba(255,255,255,.3);transform:scale(.95)}
.button.alt-hold-active{background:linear-gradient(145deg,#7209b7,#560bad);box-shadow:0 0 20px rgba(114,9,183,0.8),inset 0 1px 0 rgba(255,255,255,.3);animation: pulse-button 2s infinite;}
@keyframes pulse-button {
  0% { box-shadow: 0 0 0 0 rgba(114,9,183,0.7); }
  70% { box-shadow: 0 0 0 10px rgba(114,9,183,0); }
  100% { box-shadow: 0 0 0 0 rgba(114,9,183,0); }
}
.button:active{transform:scale(.92)}
.button-icon{font-size:1.1rem;margin-bottom:4px}

/*======== 底部信息 ========*/
.control-info{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:12px;height:20vh}
.info-box{background:rgba(0,0,0,.4);padding:12px;border-radius:12px;border:1px solid rgba(255,255,255,.1);display:flex;flex-direction:column;justify-content:center}
.info-title{color:#72efdd;font-size:.85rem;margin-bottom:4px}
.info-value{font-size:1.3rem;font-weight:bold;color:#4cc9f0;font-family:'Courier New',monospace}
.footer{text-align:center;padding:10px;margin-top:12px;background:rgba(0,0,0,.4);border-radius:12px;font-size:.75rem;color:#a9a9a9;border:1px solid rgba(255,255,255,.05);display:flex;justify-content:space-between;align-items:center}

/*======== 控制滑块 ========*/
.control-sliders{display:flex;flex-direction:column;gap:8px;margin-top:8px}
.slider-group{display:flex;flex-direction:column;gap:4px}
.slider-label{color:#a9def9;font-size:.75rem;display:flex;justify-content:space-between}
.slider-label span{color:#4cc9f0}
.slider{width:100%;height:6px;-webkit-appearance:none;background:linear-gradient(to right,#3a0ca3,#4361ee);border-radius:3px;outline:none}
.slider::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;border-radius:50%;background:#fff;cursor:pointer;border:2px solid #4361ee;box-shadow:0 0 8px rgba(67,97,238,.8)}

/*======== 校准按钮 ========*/
.calibration-btn{background:linear-gradient(145deg,#7209b7,#560bad);border:none;border-radius:8px;color:#fff;padding:6px 12px;font-size:.75rem;cursor:pointer;transition:all .2s}
.calibration-btn:hover{background:linear-gradient(145deg,#560bad,#7209b7);transform:translateY(-1px)}

/*======== 气压定高控制 ========*/
.altitude-control {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 10px;
  padding: 10px;
  background: rgba(86,11,173,0.2);
  border-radius: 8px;
  border: 1px solid #7209b7;
}

.altitude-display {
  font-family: 'Courier New', monospace;
  font-size: 1.1rem;
  color: #72efdd;
  text-align: center;
}

.altitude-btn {
  background: linear-gradient(145deg,#7209b7,#560bad);
  border: none;
  border-radius: 6px;
  color: #fff;
  padding: 8px;
  font-size: 0.85rem;
  font-weight: bold;
  cursor: pointer;
  transition: all .2s;
}

.altitude-btn:hover {
  background: linear-gradient(145deg,#560bad,#7209b7);
  transform: translateY(-1px);
}

.altitude-btn.active {
  background: linear-gradient(145deg,#f72585,#b5179e);
  box-shadow: 0 0 15px rgba(247,37,133,0.6);
}

/*======== 协议选择器 =========*/
.protocol-selector {
  display: flex;
  gap: 10px;
  margin-top: 5px;
  justify-content: center;
}

.protocol-btn {
  background: rgba(67,97,238,0.3);
  border: 1px solid rgba(67,97,238,0.6);
  border-radius: 6px;
  color: #a9def9;
  padding: 4px 8px;
  font-size: 0.7rem;
  cursor: pointer;
  transition: all .2s;
}

.protocol-btn.active {
  background: rgba(67,97,238,0.8);
  color: #fff;
  border-color: #4cc9f0;
  box-shadow: 0 0 8px rgba(76,201,240,0.5);
}

/*======== 横屏锁定 ========*/
@media (orientation: portrait){body::before{content:"🔄 请旋转设备至横屏模式";position:fixed;inset:0;background:#1a1a2e;display:flex;justify-content:center;align-items:center;font-size:1.5rem;color:#f72585;text-align:center;padding:20px;z-index:10000;font-weight:bold}.container{display:none}}

/*======== 动画 ========*/
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(67,97,238,.7)}70%{box-shadow:0 0 0 12px rgba(67,97,238,0)}100%{box-shadow:0 0 0 0 rgba(67,97,238,0)}}
.joystick.active{animation:pulse 1.5s infinite}
@keyframes fadeIn{from{opacity:0;transform:translateY(20px)}to{opacity:1;transform:translateY(0)}}
.container > *{animation:fadeIn .5s ease-out}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>🚁 Flix无人机遥控器 - 性能优化版</h1>
    <div class="status-bar">
      <div class="status-item"><span class="status-dot" id="status-dot"></span><span id="connection-text">连接中...</span></div>
      <div class="status-item"><span>📶</span><span id="rssi">-</span></div>
      <div class="status-item"><span>⏱️</span><span id="latency">-</span></div>
      <div class="status-item"><span>📦</span><span id="packet-loss">0%</span></div>
      <div class="status-item"><span>📊</span><span id="protocol-type">JSON</span></div>
      <div class="status-item"><span>⚡</span><span id="battery">-</span></div>
      <div class="status-item altitude-status" id="altitude-status" style="display:none">
        <span>⛰️</span><span id="altitude-value">-</span>
      </div>
    </div>
    <div class="protocol-selector">
      <button class="protocol-btn active" onclick="setProtocol('json')">JSON协议</button>
      <button class="protocol-btn" onclick="setProtocol('binary')">二进制协议</button>
      <button class="protocol-btn" onclick="setProtocol('delta')">增量协议</button>
    </div>
  </div>

  <div class="content">
    <!-- 左摇杆 -->
    <div class="joystick-container">
      <div class="joystick-title">左摇杆 (油门/偏航)</div>
      <div class="joystick-wrapper">
        <div class="joystick" id="joystick-left"><div class="joystick-knob" id="knob-left"></div></div>
      </div>
      <div class="coordinates">
        <div>油门: <span id="left-y">0</span>%</div>
        <div>偏航: <span id="left-x">0</span></div>
      </div>
      <div class="altitude-control" id="altitude-control">
        <div class="altitude-display">
          高度: <span id="current-altitude">-</span>m
          <div style="font-size:0.8rem">目标: <span id="target-altitude">-</span>m</div>
        </div>
        <button class="altitude-btn" id="altitude-btn" onclick="toggleAltitudeHold()">
          ⏸️ 气压定高
        </button>
      </div>
    </div>

    <!-- 按钮区 -->
    <div class="buttons-container" id="buttons-container"></div>

    <!-- 右摇杆 -->
    <div class="joystick-container">
      <div class="joystick-title">右摇杆 (俯仰/横滚)</div>
      <div class="joystick-wrapper">
        <div class="joystick" id="joystick-right"><div class="joystick-knob" id="knob-right"></div></div>
      </div>
      <div class="coordinates">
        <div>俯仰: <span id="right-y">0</span></div>
        <div>横滚: <span id="right-x">0</span></div>
      </div>
    </div>
  </div>

  <div class="control-info">
    <div class="info-box">
      <div class="info-title">飞行控制</div>
      <div class="info-value" id="throttle-value">0%</div>
      <div class="control-sliders">
        <div class="slider-group">
          <div class="slider-label">油门灵敏度: <span id="throttle-sens-value">80</span>%</div>
          <input type="range" class="slider" id="throttle-sensitivity" min="50" max="120" value="80">
        </div>
        <div class="slider-group">
          <div class="slider-label">死区: <span id="deadzone-value">3</span>%</div>
          <input type="range" class="slider" id="deadzone-slider" min="0" max="10" value="3">
        </div>
      </div>
    </div>
    
    <div class="info-box">
      <div class="info-title">姿态控制</div>
      <div class="info-value" id="attitude-values">0%, 0°, 0°</div>
      <div class="control-sliders">
        <div class="slider-group">
          <div class="slider-label">摇杆灵敏度: <span id="stick-sens-value">85</span>%</div>
          <input type="range" class="slider" id="stick-sensitivity" min="50" max="120" value="85">
        </div>
        <div class="slider-group">
          <div class="slider-label">指数曲线: <span id="expo-value">25</span>%</div>
          <input type="range" class="slider" id="expo-slider" min="0" max="60" value="25">
        </div>
      </div>
    </div>
    
    <div class="info-box">
      <div class="info-title">飞行模式</div>
      <div class="info-value" id="flight-mode">STAB</div>
      <div class="control-sliders">
        <div class="slider-group">
          <div class="slider-label">最大倾斜: <span id="tilt-value">30</span>°</div>
          <input type="range" class="slider" id="tilt-slider" min="10" max="45" value="30">
        </div>
        <div class="slider-group">
          <div class="slider-label">平滑度: <span id="smooth-value">40</span>%</div>
          <input type="range" class="slider" id="smooth-slider" min="10" max="80" value="40">
        </div>
      </div>
    </div>
  </div>

  <div class="footer">
    <div>Flix无人机遥控器 v4.0 | 性能优化版</div>
    <div>WiFi: FlixDrone | 密码: 12345678</div>
    <button class="calibration-btn" onclick="calibrateCenter()">校准中心</button>
  </div>
</div>

<script>
/*======================== 全局变量 ========================*/
let lastAnimationTime = 0;
let connectionOk = false;
let packetStats = { sent: 0, lost: 0 };
let latencyHistory = new Array(10).fill(0);
let latencyIndex = 0;
let altitudeHoldActive = false;
let currentAltitude = 0;
let targetAltitude = 0;
let altitudeUpdateInterval;
let lastSendTime = 0;
const SEND_INTERVAL = 16; // ~60Hz

// ====================== 优化1: 使用Pointer Events统一处理 ======================
const touches = new Map();
let leftStick = {x:0, y:0, rawX:0, rawY:0};
let rightStick = {x:0, y:0, rawX:0, rawY:0};

// ====================== 优化2: Delta增量发送变量 ======================
let lastSentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
let currentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
const MIN_CHANGE_THRESHOLD = 0.5; // 变化阈值，减少微小变化发送

// ====================== 优化3: 协议支持 ======================
let currentProtocol = 'json'; // json, binary, delta
let useBinaryProtocol = false;
let useDeltaProtocol = false;

// 控制参数
let deadzone = 3;
let expo = 25;
let stickSensitivity = 85;
let throttleSensitivity = 80;
let smoothing = 40;
let tiltMax = 30;

// 按钮状态
let buttonStates = new Array(16).fill(false);
let lastButtonStates = new Array(16).fill(false);

/*======================== 按钮配置 ========================*/
const buttonConfigs = [
  {icon:"🔓",label:"解锁",color:"#00ff88",desc:"解锁电机"},
  {icon:"🔒",label:"上锁",color:"#ff3333",desc:"锁定电机"},
  {icon:"🚀",label:"起飞",color:"#ff9900",desc:"自动起飞"},
  {icon:"🛬",label:"降落",color:"#ff6600",desc:"自动降落"},
  {icon:"🏠",label:"返航",color:"#9966ff",desc:"返回起飞点"},
  {icon:"⛰️",label:"定高",color:"#7209b7",desc:"气压定高"},
  {icon:"🔄",label:"自稳",color:"#4cc9f0",desc:"稳定模式"},
  {icon:"🎯",label:"特技",color:"#f72585",desc:"特技模式"},
  {icon:"💡",label:"LED",color:"#ffff00",desc:"LED开关"},
  {icon:"🔊",label:"蜂鸣",color:"#ff9900",desc:"蜂鸣器测试"},
  {icon:"📹",label:"录像",color:"#ff3333",desc:"开始/停止录像"},
  {icon:"📸",label:"拍照",color:"#ffffff",desc:"拍摄照片"},
  {icon:"⬆️",label:"上升",color:"#00ff88",desc:"垂直上升"},
  {icon:"⬇️",label:"下降",color:"#ff3333",desc:"垂直下降"},
  {icon:"⚙️",label:"设置",color:"#4cc9f0",desc:"高级设置"},
  {icon:"ℹ️",label:"信息",color:"#72efdd",desc:"系统信息"}
];

/*======================== 初始化 ========================*/
function init() {
  initButtons();
  initControls();
  initNetwork();
  initPointerEvents();
  loadSettings();
  
  // 初始化高度更新
  startAltitudeUpdates();
  
  // 使用requestAnimationFrame替代setInterval，性能更好
  requestAnimationFrame(animationLoop);
}

function initButtons() {
  const container = document.getElementById('buttons-container');
  container.innerHTML = '';
  buttonConfigs.forEach((cfg, idx) => {
    const btn = document.createElement('button');
    btn.className = 'button';
    btn.id = `btn-${idx}`;
    btn.title = cfg.desc;
    btn.innerHTML = `<div class="button-icon">${cfg.icon}</div><div>${cfg.label}</div>`;
    btn.style.background = `linear-gradient(145deg, ${cfg.color}44, ${cfg.color}88)`;
    btn.style.border = `2px solid ${cfg.color}`;
    
    // 按钮5（气压定高）特殊处理
    if (idx === 5) {
      btn.classList.add('alt-hold-btn');
    }
    
    btn.addEventListener('pointerdown', e => {
      e.preventDefault();
      handleButton(idx);
      if (navigator.vibrate) navigator.vibrate(20);
    });
    
    btn.addEventListener('pointerup', e => {
      e.preventDefault();
      // 按钮释放不需要额外处理
    });
    
    container.appendChild(btn);
  });
}

function initControls() {
  const sliders = [
    'deadzone-slider', 'expo-slider', 'stick-sensitivity',
    'throttle-sensitivity', 'smooth-slider', 'tilt-slider'
  ];
  
  sliders.forEach(id => {
    const slider = document.getElementById(id);
    if (slider) {
      // 使用input事件而不是change，响应更快
      slider.addEventListener('input', updateControlParams);
    }
  });
  
  // 初始化气压定高按钮
  const altitudeBtn = document.getElementById('altitude-btn');
  if (altitudeBtn) {
    altitudeBtn.addEventListener('click', toggleAltitudeHold);
  }
}

function initNetwork() {
  fetch('/info')
    .then(r => r.json())
    .then(data => {
      document.getElementById('ip').textContent = data.ip;
      document.getElementById('rssi').textContent = data.rssi + 'dBm';
      updateConnectionStatus(true);
    })
    .catch(() => {
      updateConnectionStatus(false);
    });
  
  // 定期更新状态
  setInterval(updateNetworkStatus, 1000);
}

// ====================== 优化4: 统一Pointer Events处理（触摸和鼠标）======================
function initPointerEvents() {
  const joysticks = [
    { id: 'joystick-left', side: 'left' },
    { id: 'joystick-right', side: 'right' }
  ];
  
  joysticks.forEach(js => {
    const element = document.getElementById(js.id);
    
    element.addEventListener('pointerdown', e => {
      e.preventDefault();
      handlePointerStart(e, js.side);
      element.setPointerCapture(e.pointerId);
    });
    
    element.addEventListener('pointermove', e => {
      e.preventDefault();
      handlePointerMove(e, js.side);
    });
    
    element.addEventListener('pointerup', e => {
      e.preventDefault();
      handlePointerEnd(e, js.side);
    });
    
    element.addEventListener('pointercancel', e => {
      e.preventDefault();
      handlePointerEnd(e, js.side);
    });
    
    // 添加触摸事件阻止默认行为
    element.addEventListener('touchstart', e => e.preventDefault());
    element.addEventListener('touchmove', e => e.preventDefault());
  });
}

/*======================== 动画循环 ========================*/
function animationLoop(timestamp) {
  const deltaTime = timestamp - lastAnimationTime;
  
  if (deltaTime >= SEND_INTERVAL) { // 控制发送频率约60Hz
    processJoystickInput();
    checkAndSendChanges();
    updateDisplayAll();
    lastAnimationTime = timestamp;
  }
  
  requestAnimationFrame(animationLoop);
}

/*======================== 控制参数更新 ========================*/
function updateControlParams() {
  deadzone = parseInt(document.getElementById('deadzone-slider').value);
  expo = parseInt(document.getElementById('expo-slider').value);
  stickSensitivity = parseInt(document.getElementById('stick-sensitivity').value);
  throttleSensitivity = parseInt(document.getElementById('throttle-sensitivity').value);
  smoothing = parseInt(document.getElementById('smooth-slider').value);
  tiltMax = parseInt(document.getElementById('tilt-slider').value);
  
  document.getElementById('deadzone-value').textContent = deadzone;
  document.getElementById('expo-value').textContent = expo;
  document.getElementById('stick-sens-value').textContent = stickSensitivity;
  document.getElementById('throttle-sens-value').textContent = throttleSensitivity;
  document.getElementById('smooth-value').textContent = smoothing;
  document.getElementById('tilt-value').textContent = tiltMax;
  
  // 发送参数更新
  sendParamsUpdate();
  saveSettings();
}

/*======================== 摇杆曲线处理 ========================*/
function applyCurve(value) {
  const absVal = Math.abs(value);
  if (absVal < deadzone / 100) return 0;
  
  const normalized = (absVal - deadzone / 100) / (1 - deadzone / 100);
  const sign = value >= 0 ? 1 : -1;
  const expoFactor = expo / 100;
  
  let curved = normalized * (1 - expoFactor) + Math.pow(normalized, 3) * expoFactor;
  curved *= stickSensitivity / 100;
  
  return curved * sign;
}

function applyThrottleCurve(value) {
  const normalized = value / 100;
  
  let curved;
  if (normalized < 0.3) {
    curved = normalized * 0.7;
  } else if (normalized < 0.7) {
    curved = normalized;
  } else {
    curved = normalized * 0.8;
  }
  
  curved *= throttleSensitivity / 100;
  return curved * 100;
}

/*======================== 摇杆数据处理 ========================*/
function processJoystickInput() {
  // ============ 左摇杆 ============
  // 油门：将-100~100映射到0~100
  let throttleRaw = (leftStick.rawY + 100) / 2;
  currentValues.throttle = applyThrottleCurve(throttleRaw);
  
  // 偏航：直接使用-100~100
  currentValues.yaw = applyCurve(leftStick.rawX);
  
  // ============ 右摇杆 ============
  // 俯仰：直接使用-100~100
  currentValues.pitch = applyCurve(rightStick.rawY);
  
  // 滚转：直接使用-100~100
  currentValues.roll = applyCurve(rightStick.rawX);
  
  // 更新显示用的值
  leftStick.x = applyCurve(leftStick.rawX);    // 偏航显示 (-100~100)
  leftStick.y = throttleRaw - 50;              // 油门显示为-50~50
  rightStick.x = applyCurve(rightStick.rawX);  // 滚转显示 (-100~100)
  rightStick.y = applyCurve(rightStick.rawY);  // 俯仰显示 (-100~100)
}

// ====================== 优化5: 变化检测，Delta增量发送 ======================
function hasSignificantChange(newValues) {
  return (
    Math.abs(newValues.throttle - lastSentValues.throttle) > MIN_CHANGE_THRESHOLD ||
    Math.abs(newValues.roll - lastSentValues.roll) > MIN_CHANGE_THRESHOLD ||
    Math.abs(newValues.pitch - lastSentValues.pitch) > MIN_CHANGE_THRESHOLD ||
    Math.abs(newValues.yaw - lastSentValues.yaw) > MIN_CHANGE_THRESHOLD
  );
}

function checkAndSendChanges() {
  // 检查摇杆数据是否有显著变化
  if (hasSignificantChange(currentValues)) {
    sendJoystickData();
  }
  
  // 检查按钮状态变化
  for (let i = 0; i < buttonStates.length; i++) {
    if (buttonStates[i] !== lastButtonStates[i]) {
      sendButtonData(i, buttonStates[i]);
      lastButtonStates[i] = buttonStates[i];
    }
  }
}

/*======================== 数据发送函数 ========================*/
function sendJoystickData() {
  const data = {
    t: 1, // 类型: 摇杆数据
    th: Math.round(currentValues.throttle),
    r: Math.round(currentValues.roll),
    p: Math.round(currentValues.pitch),
    y: Math.round(currentValues.yaw),
    ts: performance.now(),
    dz: deadzone,
    ex: expo,
    ss: stickSensitivity
  };
  
  sendToESP('/web_rc', data);
  lastSentValues = {...currentValues};
  packetStats.sent++;
}

function sendButtonData(buttonIndex, state) {
  const data = {
    t: 2, // 类型: 按钮数据
    b: buttonIndex,
    s: state ? 1 : 0,
    ts: performance.now()
  };
  
  sendToESP('/web_rc', data);
  
  if (navigator.vibrate) {
    navigator.vibrate(20);
  }
}

function sendParamsUpdate() {
  const data = {
    t: 5, // 类型: 参数更新
    ths: throttleSensitivity,
    sss: stickSensitivity,
    ys: throttleSensitivity * 0.7,  // 偏航灵敏度设置为油门灵敏度的70%
    ts: performance.now()
  };
  
  sendToESP('/web_rc', data);
}

/*======================== 显示更新 ========================*/
function updateDisplayAll() {
  // 摇杆值显示
  // 左摇杆：Y轴显示油门百分比（0-100%），X轴显示偏航值（-100~100）
  // 右摇杆：Y轴显示俯仰值（-100~100），X轴显示横滚值（-100~100）
  
  const throttlePercent = Math.round((leftStick.rawY + 100) / 2); // 计算油门百分比
  const yawValue = Math.round(leftStick.x);  // 偏航值
  
  // 更新左摇杆显示
  document.getElementById('left-x').textContent = yawValue;  // 偏航
  document.getElementById('left-y').textContent = Math.round(currentValues.throttle) + '%'; // 油门百分比
  
  // 更新右摇杆显示
  document.getElementById('right-x').textContent = Math.round(rightStick.x);  // 横滚
  document.getElementById('right-y').textContent = Math.round(rightStick.y);  // 俯仰
  
  // 控制值显示
  document.getElementById('throttle-value').textContent = Math.round(currentValues.throttle) + '%';
  document.getElementById('attitude-values').textContent = 
    `${Math.round(currentValues.throttle)}%, ${Math.round(currentValues.pitch)}°, ${Math.round(currentValues.roll)}°`;
  
  // 高度显示
  document.getElementById('current-altitude').textContent = currentAltitude.toFixed(1);
  document.getElementById('target-altitude').textContent = targetAltitude.toFixed(1);
  
  // 更新协议类型显示
  document.getElementById('protocol-type').textContent = currentProtocol.toUpperCase();
}

/*======================== Pointer Events处理 ========================*/
function handlePointerStart(e, side) {
  e.preventDefault();
  touches.set(e.pointerId, side);
  document.getElementById(`joystick-${side}`).classList.add('active');
  updateJoystickPosition(side, e.clientX, e.clientY);
}

function handlePointerMove(e, side) {
  e.preventDefault();
  if (touches.get(e.pointerId) === side) {
    updateJoystickPosition(side, e.clientX, e.clientY);
  }
}

function handlePointerEnd(e, side) {
  e.preventDefault();
  if (touches.get(e.pointerId) === side) {
    touches.delete(e.pointerId);
    const joystick = document.getElementById(`joystick-${side}`);
    const knob = document.getElementById(`knob-${side}`);
    
    joystick.classList.remove('active');
    knob.style.transition = 'transform 0.2s ease-out';
    knob.style.transform = 'translate(-50%, -50%)';
    
    setTimeout(() => {
      knob.style.transition = '';
    }, 200);
    
    // 重置摇杆原始值
    if (side === 'left') {
      leftStick = {x:0, y:0, rawX:0, rawY:0};
    } else {
      rightStick = {x:0, y:0, rawX:0, rawY:0};
    }
    
    // 立即发送重置数据
    processJoystickInput();
    sendJoystickData();
  }
}

function updateJoystickPosition(side, clientX, clientY) {
  const joystick = document.getElementById(`joystick-${side}`);
  const knob = document.getElementById(`knob-${side}`);
  const rect = joystick.getBoundingClientRect();
  
  const centerX = rect.width / 2;
  const centerY = rect.height / 2;
  const radius = centerX - 30;
  
  let deltaX = (clientX - rect.left) - centerX;
  let deltaY = (clientY - rect.top) - centerY;
  
  const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
  if (distance > radius) {
    deltaX = (deltaX / distance) * radius;
    deltaY = (deltaY / distance) * radius;
  }
  
  knob.style.transform = `translate(calc(-50% + ${deltaX}px), calc(-50% + ${deltaY}px))`;
  
  // 存储原始值（-100到100）
  const normX = (deltaX / radius) * 100;
  const normY = -(deltaY / radius) * 100;  // 反转Y轴，向上为正
  
  if (side === 'left') {
    leftStick.rawX = normX;
    leftStick.rawY = normY;
  } else {
    rightStick.rawX = normX;
    rightStick.rawY = normY;
  }
}

/*======================== 网络处理 ========================*/
function sendToESP(url, data) {
  const startTime = performance.now();
  
  fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  })
  .then(response => {
    if (response.ok) {
      const latency = performance.now() - startTime;
      updateLatency(latency);
      return response.json();
    }
    throw new Error('Network response was not ok');
  })
  .then(responseData => {
    // 解析服务器响应
    if (responseData && responseData.s === "ok") {
      // 更新高度信息
      if (responseData.alt_current) {
        currentAltitude = parseFloat(responseData.alt_current) || 0;
      }
      if (responseData.alt_target) {
        targetAltitude = parseFloat(responseData.alt_target) || 0;
      }
    }
  })
  .catch(error => {
    packetStats.lost++;
    updateConnectionStatus(false);
  });
}

function updateLatency(latency) {
  latencyHistory[latencyIndex] = latency;
  latencyIndex = (latencyIndex + 1) % latencyHistory.length;
  
  const avgLatency = latencyHistory.reduce((a, b) => a + b) / latencyHistory.length;
  document.getElementById('latency').textContent = Math.round(avgLatency) + 'ms';
  
  const latencyDot = document.querySelector('.status-dot');
  if (avgLatency > 200) {
    latencyDot.className = 'status-dot warning';
  } else if (avgLatency > 100) {
    latencyDot.className = 'status-dot warning';
  } else {
    latencyDot.className = 'status-dot connected';
  }
}

function updateNetworkStatus() {
  const lossRate = packetStats.sent > 0 ? 
    (packetStats.lost / packetStats.sent * 100).toFixed(1) : '0';
  document.getElementById('packet-loss').textContent = lossRate + '%';
  
  fetch('/web_rc/status')
    .then(r => r.json())
    .then(data => {
      document.getElementById('battery').textContent = data.battery + '%';
      // 更新高度状态
      if (data.alt_hold) {
        updateAltitudeStatus(true, data.alt_target || 0);
      }
    })
    .catch(() => {
      document.getElementById('battery').textContent = '-';
    });
}

function updateConnectionStatus(connected) {
  connectionOk = connected;
  const dot = document.getElementById('status-dot');
  const text = document.getElementById('connection-text');
  
  if (connected) {
    dot.className = 'status-dot connected';
    text.textContent = '已连接';
    text.style.color = '#0f8';
  } else {
    dot.className = 'status-dot disconnected';
    text.textContent = '连接断开';
    text.style.color = '#f33';
  }
}

/*======================== 协议设置 ========================*/
function setProtocol(protocol) {
  currentProtocol = protocol;
  
  // 更新UI
  document.querySelectorAll('.protocol-btn').forEach(btn => {
    btn.classList.remove('active');
  });
  event.target.classList.add('active');
  
  // 更新内部标志
  useBinaryProtocol = (protocol === 'binary');
  useDeltaProtocol = (protocol === 'delta');
  
  // 保存设置
  localStorage.setItem('flix_rc_protocol', protocol);
}

/*======================== 按钮处理 ========================*/
function handleButton(idx) {
  buttonStates[idx] = !buttonStates[idx];
  const btn = document.getElementById(`btn-${idx}`);
  
  if (buttonStates[idx]) {
    btn.classList.add('active');
    btn.style.transform = 'scale(0.95)';
    
    // 特殊按钮处理：按钮5（气压定高）
    if (idx === 5) {
      toggleAltitudeHold();
    }
  } else {
    btn.classList.remove('active');
    btn.style.transform = '';
  }
  
  // 立即发送按钮状态（不等待检查循环）
  sendButtonData(idx, buttonStates[idx]);
  
  if (navigator.vibrate) {
    navigator.vibrate(20);
  }
}

/*======================== 气压定高功能 ========================*/
function toggleAltitudeHold() {
  if (!connectionOk) {
    alert('请先连接无人机');
    return;
  }
  
  altitudeHoldActive = !altitudeHoldActive;
  
  // 更新按钮状态
  const altHoldBtn = document.getElementById('altitude-btn');
  const button5 = document.getElementById('btn-5');
  
  if (altitudeHoldActive) {
    altHoldBtn.innerHTML = '✅ 定高中';
    altHoldBtn.classList.add('active');
    if (button5) button5.classList.add('alt-hold-active');
    
    // 显示高度状态
    document.getElementById('altitude-status').style.display = 'flex';
    
    // 发送气压定高使能命令
    sendToESP('/web_rc', {
      t: 6, // 类型: 气压定高
      cmd: "enable",
      ts: performance.now()
    });
    
    // 获取当前高度作为目标
    fetch('/web_rc/status')
      .then(r => r.json())
      .then(data => {
        if (data.alt_target) {
          targetAltitude = data.alt_target;
          updateDisplayAll();
        }
      });
    
  } else {
    altHoldBtn.innerHTML = '⏸️ 气压定高';
    altHoldBtn.classList.remove('active');
    if (button5) button5.classList.remove('alt-hold-active');
    
    // 隐藏高度状态
    document.getElementById('altitude-status').style.display = 'none';
    
    // 发送气压定高禁用命令
    sendToESP('/web_rc', {
      t: 6, // 类型: 气压定高
      cmd: "disable",
      ts: performance.now()
    });
  }
  
  // 更新高度显示
  updateAltitudeStatus(altitudeHoldActive, targetAltitude);
  
  if (navigator.vibrate) {
    navigator.vibrate(50);
  }
}

function updateAltitudeStatus(active, altitude) {
  const statusElement = document.getElementById('altitude-status');
  const valueElement = document.getElementById('altitude-value');
  
  if (active) {
    statusElement.style.display = 'flex';
    valueElement.textContent = altitude.toFixed(1) + 'm';
    currentAltitude = altitude;
    targetAltitude = altitude;
  } else {
    statusElement.style.display = 'none';
  }
  
  updateDisplayAll();
}

function startAltitudeUpdates() {
  // 定期更新高度信息
  altitudeUpdateInterval = setInterval(() => {
    if (connectionOk && altitudeHoldActive) {
      fetch('/web_rc/status')
        .then(r => r.json())
        .then(data => {
          if (data.alt_target) {
            targetAltitude = data.alt_target;
            updateDisplayAll();
          }
        })
        .catch(() => {
          // 更新失败时忽略
        });
    }
  }, 1000); // 每秒更新一次
}

/*======================== 校准功能 ========================*/
function calibrateCenter() {
  if (confirm('请确保摇杆在中心位置，然后点击确定进行校准')) {
    // 重置值
    leftStick = {x:0, y:0, rawX:0, rawY:0};
    rightStick = {x:0, y:0, rawX:0, rawY:0};
    currentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
    lastSentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
    
    // 发送校准命令
    sendToESP('/web_rc', {
      t: 3, // 类型: 校准
      ts: performance.now()
    });
    
    alert('校准完成！');
  }
}

/*======================== 设置保存/加载 ========================*/
function saveSettings() {
  const settings = {
    deadzone: deadzone,
    expo: expo,
    stickSensitivity: stickSensitivity,
    throttleSensitivity: throttleSensitivity,
    smoothing: smoothing,
    tiltMax: tiltMax,
    protocol: currentProtocol
  };
  
  localStorage.setItem('flix_rc_settings', JSON.stringify(settings));
}

function loadSettings() {
  const saved = localStorage.getItem('flix_rc_settings');
  if (saved) {
    try {
      const settings = JSON.parse(saved);
      deadzone = settings.deadzone || 3;
      expo = settings.expo || 25;
      stickSensitivity = settings.stickSensitivity || 85;
      throttleSensitivity = settings.throttleSensitivity || 80;
      smoothing = settings.smoothing || 40;
      tiltMax = settings.tiltMax || 30;
      currentProtocol = settings.protocol || 'json';
      
      document.getElementById('deadzone-slider').value = deadzone;
      document.getElementById('expo-slider').value = expo;
      document.getElementById('stick-sensitivity').value = stickSensitivity;
      document.getElementById('throttle-sensitivity').value = throttleSensitivity;
      document.getElementById('smooth-slider').value = smoothing;
      document.getElementById('tilt-slider').value = tiltMax;
      
      // 设置协议
      setProtocol(currentProtocol);
      
      updateControlParams();
    } catch (e) {
      console.error('Failed to load settings:', e);
    }
  }
}

/*======================== 事件监听器 ========================*/
document.addEventListener('DOMContentLoaded', init);
document.addEventListener('contextmenu', e => e.preventDefault());

// 定期发送心跳包
setInterval(() => {
  if (connectionOk) {
    sendToESP('/web_rc/heartbeat', {
      t: 4, // 类型: 心跳包
      ts: performance.now()
    });
  }
}, 3000);

// 页面关闭时禁用气压定高
window.addEventListener('beforeunload', () => {
  if (altitudeHoldActive && connectionOk) {
    // 发送气压定高禁用命令
    sendToESP('/web_rc', {
      t: 6,
      cmd: "disable",
      ts: performance.now()
    });
  }
});
</script>
</body>
</html>
)rawliteral";

#endif