// 网页遥控器客户端界面（极简深色风格，支持浅色主题）

#if WEB_RC_ENABLED

const char webRCIndexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>FLIX 网页遥控</title>
<style>
:root{
  --bg:#0b0e14; --card:#141a24; --line:#232c3b; --fg:#e8edf5; --dim:#8b97a8;
  --accent:#3ea6ff; --ok:#34d399; --danger:#f87171; --warn:#fbbf24;
  --chip-bg:rgba(255,255,255,.03); --btn-bg:rgba(255,255,255,.03);
  --stick-grad:radial-gradient(circle at 32% 30%,rgba(255,255,255,.05),rgba(0,0,0,.25));
  --stick-line:rgba(255,255,255,.07); --console-bg:#070a10; --console-fg:#6fe3a2;
  --input-bg:rgba(255,255,255,.05); --toast-bg:rgba(6,9,14,.92); --toast-fg:#e8edf5;
  --js:clamp(150px,min(44vw,52vh),330px);
  --pad:clamp(6px,1.4vmin,14px);
}
body.light{
  --bg:#eef1f6; --card:#ffffff; --line:#d7dde8; --fg:#1f2937; --dim:#64748b;
  --accent:#2563eb; --ok:#059669; --danger:#dc2626; --warn:#d97706;
  --chip-bg:rgba(0,0,0,.03); --btn-bg:rgba(0,0,0,.04);
  --stick-grad:radial-gradient(circle at 32% 30%,rgba(0,0,0,.03),rgba(0,0,0,.07));
  --stick-line:rgba(0,0,0,.08); --console-bg:#f7f9fc; --console-fg:#0d9488;
  --input-bg:rgba(0,0,0,.05); --toast-bg:rgba(255,255,255,.95); --toast-fg:#1f2937;
}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;touch-action:none;user-select:none}
html,body{height:100%}
body{background:var(--bg);color:var(--fg);font-family:-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;overflow:hidden;transition:background .25s,color .25s}
#app{height:100%;display:flex;flex-direction:column;gap:var(--pad);padding:var(--pad);max-width:1100px;margin:0 auto}

/* 顶部状态栏 */
header{position:relative;display:flex;flex-direction:column;gap:6px;padding:8px 40px 8px 12px;background:var(--card);border:1px solid var(--line);border-radius:12px;flex-shrink:0}
.title{font-size:clamp(.85rem,2.6vmin,1.05rem);font-weight:600;letter-spacing:.06em;text-align:center;color:var(--fg)}
.chips{display:flex;justify-content:center;align-items:center;flex-wrap:wrap;gap:5px}
.chip{display:inline-flex;align-items:center;gap:4px;padding:2px 8px;border:1px solid var(--line);border-radius:999px;font-size:clamp(.62rem,1.9vmin,.72rem);color:var(--dim);background:var(--chip-bg);white-space:nowrap}
.chip b{color:var(--fg);font-weight:600}
.dot{width:7px;height:7px;border-radius:50%;background:var(--danger)}
.dot.on{background:var(--ok);box-shadow:0 0 6px var(--ok)}
.armed b{color:var(--danger)}
.armed.arm b{color:var(--ok)}
#theme-btn{position:absolute;top:8px;right:10px;width:30px;height:30px;border:1px solid var(--line);border-radius:50%;background:var(--chip-bg);color:var(--fg);font-size:15px;cursor:pointer;line-height:1}

/* 提示横幅（旁观/实体遥控接管） */
#notice{display:none;padding:6px 12px;background:rgba(251,191,36,.15);border:1px solid var(--warn);color:var(--warn);border-radius:10px;font-size:clamp(.68rem,2.1vmin,.8rem);text-align:center;flex-shrink:0;font-weight:600}
body.spectating .stick-box,body.spectating .btn-panel{opacity:.45;filter:saturate(.5);pointer-events:none}

/* 主区域 */
main{flex:1;display:flex;gap:var(--pad);min-height:0}

/* 摇杆 */
.stick-box{flex:1;display:flex;flex-direction:column;justify-content:center;align-items:center;gap:6px;background:var(--card);border:1px solid var(--line);border-radius:16px;padding:8px;min-width:0}
.stick-label{font-size:clamp(.62rem,2vmin,.78rem);color:var(--dim)}
.stick-wrap{position:relative;width:var(--js);height:var(--js)}
.stick{position:absolute;inset:0;border-radius:50%;border:1px solid var(--line);background:var(--stick-grad);box-shadow:inset 0 0 24px rgba(0,0,0,.25)}
.stick::before,.stick::after{content:'';position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);background:var(--stick-line)}
.stick::before{width:1px;height:100%}
.stick::after{width:100%;height:1px}
.stick.active{border-color:var(--accent);box-shadow:inset 0 0 24px rgba(0,0,0,.25),0 0 14px rgba(62,166,255,.25)}
.knob{position:absolute;top:50%;left:50%;width:calc(var(--js)*.24);height:calc(var(--js)*.24);border-radius:50%;transform:translate(-50%,-50%);background:radial-gradient(circle at 32% 30%,#fff,#9fb4c8);box-shadow:0 4px 14px rgba(0,0,0,.35);z-index:2}

/* 按钮区 */
.btn-panel{flex:.9;display:flex;flex-direction:column;justify-content:center;gap:8px;background:var(--card);border:1px solid var(--line);border-radius:16px;padding:10px;min-width:0}
.btn-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:7px}
.btn{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:3px;padding:9px 4px;border:1px solid var(--line);border-radius:10px;background:var(--btn-bg);color:var(--fg);font-size:clamp(.62rem,2vmin,.74rem);font-weight:600;cursor:pointer;transition:transform .08s,border-color .15s,background .15s}
.btn:active{transform:scale(.94)}
.btn .ico{font-size:clamp(.9rem,3.2vmin,1.15rem);line-height:1}
.btn.arm-btn{border-color:rgba(52,211,153,.5)}
.btn.disarm-btn{border-color:rgba(248,113,113,.5)}
.btn.estop-btn{border-color:rgba(255,60,90,.55)}
.btn.alt-btn{border-color:rgba(62,166,255,.5)}
.btn.alt-btn.active{background:rgba(62,166,255,.22);border-color:var(--accent);box-shadow:0 0 10px rgba(62,166,255,.3)}
.btn.debug-btn.active{background:rgba(139,151,168,.25)}

/* 控制台 */
#console{display:none;flex-direction:column;gap:6px;background:var(--console-bg);border:1px solid var(--line);border-radius:10px;padding:8px;max-height:32vh;flex-shrink:0}
#console-out{flex:1;overflow-y:auto;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:.68rem;color:var(--console-fg);line-height:1.45;min-height:70px;word-break:break-all;touch-action:pan-y}
#console-in-row{display:flex;gap:6px;touch-action:auto}
#console-in{flex:1;background:var(--input-bg);border:1px solid var(--line);border-radius:8px;color:var(--fg);padding:6px 10px;font-size:.72rem;touch-action:auto}
#console-in:focus{outline:none;border-color:var(--accent)}
.send{background:var(--accent);border:none;border-radius:8px;color:#fff;font-weight:700;padding:0 14px;font-size:.72rem;cursor:pointer;touch-action:auto}

/* 竖屏布局 */
@media (orientation:portrait){
  main{display:grid;grid-template-columns:1fr 1fr;grid-template-rows:auto minmax(0,1fr);flex:none;height:auto}
  .btn-panel{grid-column:1/3;grid-row:1;flex:none}
  #box-l{grid-column:1;grid-row:2;min-width:0;overflow:hidden}
  #box-r{grid-column:2;grid-row:2;min-width:0;overflow:hidden}
}
@media (orientation:landscape){
  main{flex-direction:row}
}

/* 提示浮层 */
#toast{position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:var(--toast-bg);border:1px solid var(--line);color:var(--toast-fg);padding:12px 22px;border-radius:12px;font-size:.85rem;z-index:99;pointer-events:none;opacity:0;transition:opacity .25s;max-width:80vw;text-align:center;white-space:pre-line}
</style>
</head>
<body>
<div id="app">
  <header>
    <div class="title">FLIX 网页遥控</div>
    <button id="theme-btn" title="切换主题">☀️</button>
    <div class="chips">
      <span class="chip"><span class="dot" id="dot"></span><span id="conn">连接中</span></span>
      <span class="chip armed" id="armed-chip"><span>状态</span><b id="armed">已上锁</b></span>
      <span class="chip"><span>模式</span><b id="mode">自稳</b></span>
      <span class="chip"><span>定高</span><b id="althold">关</b></span>
      <span class="chip"><span>电压</span><b id="volt">-</b></span>
      <span class="chip"><span>高度</span><b id="alt">-</b></span>
      <span class="chip"><span>延迟</span><b id="lat">-</b></span>
    </div>
  </header>

  <div id="notice"></div>

  <main>
    <div class="stick-box" id="box-l"><div class="stick-label">左摇杆 · 油门 / 偏航</div><div class="stick-wrap"><div class="stick" id="stick-l"><div class="knob" id="knob-l"></div></div></div></div>
    <div class="btn-panel">
      <div class="btn-grid">
        <button class="btn arm-btn"    id="btn-0"><span class="ico">🔓</span>解锁</button>
        <button class="btn disarm-btn" id="btn-1"><span class="ico">🔒</span>上锁</button>
        <button class="btn estop-btn"  id="btn-2"><span class="ico">🛑</span>急停</button>
        <button class="btn"            id="btn-3"><span class="ico">🔄</span>模式</button>
        <button class="btn alt-btn"    id="btn-4"><span class="ico">⛰️</span>定高</button>
        <button class="btn debug-btn"  id="btn-5"><span class="ico">🖥️</span>调试</button>
      </div>
    </div>
    <div class="stick-box" id="box-r"><div class="stick-label">右摇杆 · 俯仰 / 横滚</div><div class="stick-wrap"><div class="stick" id="stick-r"><div class="knob" id="knob-r"></div></div></div></div>
  </main>

  <div id="console">
    <div id="console-out"></div>
    <div id="console-in-row"><input id="console-in" placeholder="输入命令，如 help / rc / imu / baro" autocomplete="off"><button class="send" onclick="sendCmd()">发送</button></div>
  </div>
</div>
<div id="toast"></div>

<script>
"use strict";
/* 状态 */
let armed=false, mode=2, althold=false, connOk=false, fails=0;
let ownerOk=true, phyActive=false, prio=0; // 会话拥有权 / 实体遥控接管 / 优先级
const MODES=["手动","特技","自稳","自动"];
let left={x:0,y:0,rx:0,ry:-100}, right={x:0,y:0,rx:0,ry:0}; // ry=-100: 油门杆默认在底部
let lastSent={th:-999,r:-999,p:-999,y:-999};
let latHist=new Array(10).fill(0), latIdx=0;
const THRESH=0.5, EXPO=0.25;

/* 会话 ID（同标签页内持久，刷新保持同一身份） */
const SID=(function(){let s=sessionStorage.getItem('flix_sid');
  if(!s){s=Math.random().toString(36).slice(2,10)+Date.now().toString(36);sessionStorage.setItem('flix_sid',s);}
  return s;})();

/* 主题切换 */
const themeBtn=document.getElementById('theme-btn');
function applyTheme(t){document.body.classList.toggle('light',t==='light');themeBtn.textContent=t==='light'?'🌙':'☀️';localStorage.setItem('flix_theme',t);}
applyTheme(localStorage.getItem('flix_theme')||'dark');
themeBtn.addEventListener('click',()=>applyTheme(document.body.classList.contains('light')?'dark':'light'));

/* 曲线 */
function curve(v){const a=Math.abs(v);return (a*(1-EXPO)+a*a*a*EXPO)*(v>=0?1:-1);}

/* 摇杆指针事件 */
function bindStick(side,id){
  const el=document.getElementById(id),knob=document.getElementById("knob-"+side);
  const move=(e)=>{
    const r=el.getBoundingClientRect(),cx=r.width/2,cy=r.height/2,rad=cx-10;
    let dx=(e.clientX-r.left)-cx, dy=(e.clientY-r.top)-cy;
    const d=Math.hypot(dx,dy); if(d>rad){dx=dx/d*rad;dy=dy/d*rad;}
    knob.style.transform=`translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
    if(side==="l"){left.rx=dx/rad*100; left.ry=-dy/rad*100;}
    else{right.rx=dx/rad*100; right.ry=-dy/rad*100;}
  };
  const up=()=>{
    el.classList.remove("active");
    const targetY=(side==="l"&&!althold)?-100:0;
    const r=el.getBoundingClientRect(),rad=r.width/2-10,dy=-targetY/100*rad;
    knob.style.transition="transform .18s ease-out";
    knob.style.transform=`translate(calc(-50% + 0px), calc(-50% + ${dy}px))`;
    setTimeout(()=>{knob.style.transition=""},180);
    if(side==="l")left={x:0,y:0,rx:0,ry:targetY}; else right={x:0,y:0,rx:0,ry:0};
    sendSticks(true);
  };
  el.addEventListener("pointerdown",e=>{el.setPointerCapture(e.pointerId);el.classList.add("active");move(e);});
  el.addEventListener("pointermove",e=>{if(e.buttons)move(e);});
  el.addEventListener("pointerup",up); el.addEventListener("pointercancel",up);
  el.addEventListener("touchstart",e=>e.preventDefault());
  el.addEventListener("touchmove",e=>e.preventDefault());
}
bindStick("l","stick-l"); bindStick("r","stick-r");

/* 发送（自动附带会话 ID） */
function post(url,data,cb){const t0=performance.now();
  fetch(url,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(Object.assign({sid:SID},data))})
  .then(r=>r.json().then(j=>({ok:r.ok,j})))
  .then(({ok,j})=>{ if(!ok)throw 0;
    const lat=performance.now()-t0;latHist[latIdx]=lat;latIdx=(latIdx+1)%latHist.length;
    cb&&cb(j); return j; })
  .catch(()=>{if(++fails>=3)setConn(false);});
}
function setConn(on){connOk=on;document.getElementById("dot").className="dot"+(on?" on":"");document.getElementById("conn").textContent=on?"已连接":"连接断开";}
function sendSticks(force){
  if(!ownerOk)return;
  const v={th:left.ry, r:curve(right.rx/100)*100, p:curve(right.ry/100)*100, y:curve(left.rx/100)*100};
  const changed=Math.abs(v.th-lastSent.th)>THRESH||Math.abs(v.r-lastSent.r)>THRESH||Math.abs(v.p-lastSent.p)>THRESH||Math.abs(v.y-lastSent.y)>THRESH;
  if(changed||force){post("/web_rc",{t:1,th:Math.round(v.th),r:Math.round(v.r),p:Math.round(v.p),y:Math.round(v.y),ts:performance.now()},applyResp);lastSent=v;}
}
function sendBtn(i,s){if(!ownerOk)return;post("/web_rc",{t:2,b:i,s:s?1:0,ts:performance.now()},applyResp);}
function applyResp(j){
  fails=0;setConn(true);
  if(j.m!==undefined&&j.m!==mode){mode=j.m;document.getElementById("mode").textContent=MODES[mode]||"?";}
  if(j.arm!==undefined){armed=!!j.arm;const c=document.getElementById("armed-chip");
    c.classList.toggle("arm",armed);document.getElementById("armed").textContent=armed?"已解锁":"已上锁";}
  if(j.ah!==undefined){althold=!!j.ah;document.getElementById("althold").textContent=althold?"开":"关";
    document.getElementById("btn-4").classList.toggle("active",althold);}
  if(j.ow!==undefined){
    const wasOwner=ownerOk; ownerOk=!!j.ow;
    if(ownerOk&&!wasOwner)lastSent={th:-999,r:-999,p:-999,y:-999}; // 重新获得控制权时强制补发一次摇杆
  }
  if(j.phy!==undefined)phyActive=!!j.phy;
  if(j.prio!==undefined)prio=j.prio;
  updateNotice();
  if(j.warn)toast("⚠️ "+j.warn);
  document.getElementById("lat").textContent=Math.round(latHist.reduce((a,b)=>a+b,0)/latHist.length)+"ms";
}

/* 提示横幅：旁观 / 实体遥控接管 */
function updateNotice(){
  const n=document.getElementById('notice');
  let msg='';
  if(!ownerOk)msg='另一台设备正在控制（当前为只读）';
  else if(phyActive&&prio===0)msg='实体遥控器控制中，网页输入无效';
  n.style.display=msg?'block':'none';
  n.textContent=msg;
  document.body.classList.toggle('spectating',!!msg);
}

/* 按钮 */
for(let i=0;i<3;i++){document.getElementById("btn-"+i).addEventListener("click",()=>{
  if(navigator.vibrate)navigator.vibrate(15);
  sendBtn(i,1);setTimeout(()=>sendBtn(i,0),100);
});}
document.getElementById("btn-3").addEventListener("click",()=>{
  if(navigator.vibrate)navigator.vibrate(15);
  sendBtn(3,1);setTimeout(()=>sendBtn(3,0),100);
});
document.getElementById("btn-4").addEventListener("click",()=>{
  if(navigator.vibrate)navigator.vibrate(15);
  sendBtn(4,1);setTimeout(()=>sendBtn(4,0),100);
});
document.getElementById("btn-5").addEventListener("click",toggleConsole);

/* 主循环：仅在有变化时发送（无操作即停止，配合会话超时） */
(function loop(){sendSticks(false);requestAnimationFrame(loop);})();
setInterval(()=>{if(connOk)post("/web_rc/heartbeat",{t:4,ts:performance.now()},applyResp);},2000);

/* 状态轮询（附带会话 ID，用于判断拥有权） */
setInterval(()=>{fetch("/web_rc/status?sid="+SID).then(r=>r.json()).then(d=>{
  if(d.voltage>0.5)document.getElementById("volt").textContent=d.voltage.toFixed(2)+"V";
  if(d.baro){document.getElementById("alt").textContent=d.alt.toFixed(1)+"m";}
  else document.getElementById("alt").textContent="--";
  if(d.althold!==undefined&&d.althold!==althold){althold=!!d.althold;
    document.getElementById("althold").textContent=althold?"开":"关";
    document.getElementById("btn-4").classList.toggle("active",althold);}
  if(d.armed!==undefined&&d.armed!==armed){armed=!!d.armed;
    document.getElementById("armed-chip").classList.toggle("arm",armed);
    document.getElementById("armed").textContent=armed?"已解锁":"已上锁";}
  if(d.mode!==undefined&&d.mode!==mode){mode=d.mode;document.getElementById("mode").textContent=MODES[mode]||"?";}
  if(d.owner!==undefined&&d.owner!==ownerOk){ownerOk=!!d.owner;if(ownerOk)lastSent={th:-999,r:-999,p:-999,y:-999};}
  if(d.phy!==undefined)phyActive=!!d.phy;
  if(d.prio!==undefined)prio=d.prio;
  updateNotice();
}).catch(()=>{});},2000);

/* 调试控制台 */
let consoleOpen=false,consoleCursor=0,consoleBusy=false;
function toggleConsole(){const c=document.getElementById("console");
  consoleOpen=!consoleOpen;c.style.display=consoleOpen?"flex":"none";
  document.getElementById("btn-5").classList.toggle("active",consoleOpen);
  if(consoleOpen){document.getElementById("console-out").innerHTML="";consoleCursor=0;
    post("/console/enable",{});pollConsole();}else post("/console/disable",{});}
function pollConsole(){if(!consoleOpen||consoleBusy)return;consoleBusy=true;
  fetch("/console?since="+consoleCursor+"&limit=30").then(r=>r.json()).then(d=>{
    const out=document.getElementById("console-out");
    (d.lines||[]).forEach(l=>{const div=document.createElement("div");div.textContent=l;out.appendChild(div);});
    while(out.children.length>200)out.removeChild(out.firstChild);
    out.scrollTop=out.scrollHeight;
    if(typeof d.next==="number")consoleCursor=d.next;
    setTimeout(pollConsole,d.has_more?80:500);
  }).catch(()=>setTimeout(pollConsole,500)).finally(()=>{consoleBusy=false;});}
function sendCmd(){const inp=document.getElementById("console-in"),cmd=inp.value.trim();
  if(!cmd)return;inp.value="";
  fetch("/console/cmd",{method:"POST",headers:{"Content-Type":"text/plain"},body:cmd}).then(()=>pollConsole()).catch(()=>{});}
document.getElementById("console-in").addEventListener("keydown",e=>{if(e.key==="Enter")sendCmd();});
document.getElementById("console-in").addEventListener("touchstart",e=>e.stopPropagation());
document.getElementById("console-in").addEventListener("pointerdown",e=>e.stopPropagation());

/* Toast */
let toastTimer=null;
function toast(msg){const t=document.getElementById("toast");t.textContent=msg;t.style.opacity=1;
  clearTimeout(toastTimer);toastTimer=setTimeout(()=>t.style.opacity=0,2200);}

/* 手势与上下文 */
document.addEventListener("contextmenu",e=>e.preventDefault());
document.addEventListener("gesturestart",e=>e.preventDefault());
</script>
</body>
</html>
)rawliteral";

#endif // WEB_RC_ENABLED
