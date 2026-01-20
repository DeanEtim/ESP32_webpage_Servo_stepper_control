const char webpage[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>ESP32 Servo and Stepper Dashboard</title>
<style>
  /* Font and base */
  body { font-family: Helvetica, Arial, sans-serif; background:#ffffff; margin:0; color:#103014; -webkit-font-smoothing:antialiased; }
  .wrap { max-width:720px; margin:0 auto; padding:18px; box-sizing:border-box; }
  h1 { text-align:center; color:#1e6b2b; margin:6px 0 14px; font-size:clamp(18px,3.2vw,26px); }

  /* Card style */
  .card { background: #f8fff8; border-radius:18px; padding:18px; margin:14px 0; box-shadow:0 6px 18px rgba(16,48,20,0.06); }

  /* Gauge area */
  .gauge-wrap { display:flex; flex-direction:column; align-items:center; gap:8px; }
  canvas#gauge { width:100%; max-width:420px; height:auto; display:block; }

  /* Slider */
  .slider-row { width:100%; display:flex; align-items:center; gap:12px; margin-top:10px; }
  input[type=range] { 
    -webkit-appearance:none; 
    width:100%; height:10px; 
    border-radius:999px; 
    background:linear-gradient(to right,#6fbf73 var(--val,50%), #e6f5ea var(--val,50%));
    outline:none; 
    }
  input[type=range]::-webkit-slider-thumb { 
    -webkit-appearance:none; 
    width:20px; 
    height:20px; 
    border-radius:50%; 
    background:#2e7a2e; 
    border:2px solid #fff; 
    box-shadow:0 1px 4px rgba(0,0,0,0.2); 
    cursor:pointer; 
    }

  .val-badge { 
    min-width:48px; 
    text-align:right; 
    font-weight:700; 
    color:#1f5a25; 
    }

  /* Buttons */
  .btn-row { display:flex; gap:10px; justify-content:center; margin-top:12px; flex-wrap:wrap; }
  .btn { padding:10px 16px; border-radius:12px; border:0; cursor:pointer; font-weight:700; font-size:15px; color:#fff; transition:opacity .15s, transform .06s; }
  .btn:active { transform:scale(.98); }
  .btn-green { background:#28a745; }
  .btn-grey { background:#bfc4be; cursor:not-allowed; }
  .btn-outline { background:transparent; border:2px solid #28a745; color:#2e7a2e; font-weight:700; }

  /* responsive */
  @media (max-width:520px){
    .wrap { padding:12px; }
    .card { padding:16px; border-radius:14px; }
    .btn { font-size:14px; padding:10px 12px; }
  }
</style>
</head>
<body>
  <div class="wrap">
    <h1>Servo and Stepper Motor Control Dashboard</h1>

    <!-- Servo: Gauge + Slider -->
    <div class="card">
      <div class="gauge-wrap">
        <h3 style="margin:0;color:#195a1c;font-size:16px">Servo Angle</h3>
        <canvas id="gauge" width="600" height="300" aria-label="servo gauge"></canvas>
        <div style="width:100%;max-width:520px;display:flex;flex-direction:column;align-items:center;">
          <!-- slider slightly below gauge -->
          <div class="slider-row" style="width:100%;">
            <!-- default value set to 0 as requested -->
            <input id="servoSlider" type="range" min="0" max="180" value="0" style="--val:0%" />
            <div class="val-badge" id="servoText">0°</div>
          </div>
        </div>
      </div>
    </div>

    <!-- Stepper: Speed + Direction + Stop -->
    <div class="card">
      <h3 style="margin:0 0 8px 0;color:#195a1c;font-size:16px">Stepper Motor</h3>

      <div style="width:100%;max-width:520px;margin:0 auto;">
        <div class="slider-row">
          <input id="speedSlider" type="range" min="0" max="100" value="50" style="--val:50%" />
          <div class="val-badge" id="speedText">50%</div>
        </div>

        <!-- Buttons: CW / CCW -->
        <div class="btn-row" style="margin-top:14px;">
          <button id="cwBtn" class="btn btn-green">CW</button>
          <button id="ccwBtn" class="btn btn-green">CCW</button>
          <button id="stopBtn" class="btn btn-green">Stop Stepper</button>
        </div>
      </div>
    </div>
  </div>

<script>
/* ---------- WebSocket setup ---------- */
const wsUrl = `ws://${location.hostname}:81/`;
let ws;
function wsConnect(){
  ws = new WebSocket(wsUrl);
  ws.onopen = ()=>{ console.log('WS open'); /* optionally show connected state */ };
  ws.onclose = ()=>{ console.log('WS closed, retry 2s'); setTimeout(wsConnect,2000); };

  ws.onmessage = (ev) => {
    try {
      const msg = JSON.parse(ev.data);
      // Intentionally ignore incoming servo updates to prevent the UI snapping back.
      // The UI is now authoritative for servo position (user control). If you want server-driven updates later,
      // I can add an opt-in behavior that updates UI only when user hasn't interacted.
      if (msg.type === 'servo' || msg.type === 'servoAngle') {
        // ignore
        return;
      }
      // other server messages can be handled here in future
    } catch(e) { console.warn('WS parse', e); }
  };
}
wsConnect();

/* ---------- Gauge drawing (canvas) ---------- */
const canvas = document.getElementById('gauge');
const ctx = canvas.getContext('2d');

// Resize canvas for device pixel ratio
function resizeCanvas() {
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  canvas.width = Math.round(rect.width * dpr);
  canvas.height = Math.round((rect.width/2) * dpr); // keep 2:1 ratio visually
  ctx.setTransform(dpr,0,0,dpr,0,0);
  drawGauge(currentAngle);
}
window.addEventListener('resize', resizeCanvas);

// geometry
let currentAngle = 0;
const centerX = () => canvas.width / (2 * (window.devicePixelRatio || 1));
const centerY = () => canvas.height / (1.05 * (window.devicePixelRatio || 1));
const radius = () => Math.min(centerX() - 20, centerY() - 20);

function drawGauge(angle) {
  const cx = centerX(), cy = centerY(), r = radius();
  ctx.clearRect(0,0,canvas.width,canvas.height);

  // background semicircle track
  ctx.beginPath();
  ctx.lineWidth = 14;
  ctx.strokeStyle = '#e6f3e8';
  ctx.arc(cx, cy, r, Math.PI, 0, false);
  ctx.stroke();

  // foreground arc (from left to angle)
  const frac = Math.max(0, Math.min(180, angle)) / 180;
  const endAngle = Math.PI + frac * Math.PI; // maps 0->PI, 180->2PI
  ctx.beginPath();
  ctx.lineWidth = 14;
  ctx.strokeStyle = '#2e7a2e';
  ctx.arc(cx, cy, r, Math.PI, endAngle, false);
  ctx.stroke();

  // needle (from center to perimeter)
  const angleRad = Math.PI + (angle/180) * Math.PI;
  const nx = cx + Math.cos(angleRad) * (r - 18);
  const ny = cy + Math.sin(angleRad) * (r - 18);
  ctx.beginPath();
  ctx.lineWidth = 4;
  ctx.strokeStyle = '#2e2e2e';
  ctx.moveTo(cx, cy);
  ctx.lineTo(nx, ny);
  ctx.stroke();

  // center dot
  ctx.beginPath();
  ctx.fillStyle = '#2e7a2e';
  ctx.arc(cx, cy, 6, 0, 2*Math.PI);
  ctx.fill();

  // angle label
  ctx.font = '18px Helvetica, Arial';
  ctx.fillStyle = '#184d1a';
  ctx.textAlign = 'center';
  ctx.fillText(Math.round(angle) + '°', cx, cy + 30);
}

/* ---------- UI controls ---------- */
const servoSlider = document.getElementById('servoSlider');
const servoText = document.getElementById('servoText');
const speedSlider = document.getElementById('speedSlider');
const speedText = document.getElementById('speedText');
const cwBtn = document.getElementById('cwBtn');
const ccwBtn = document.getElementById('ccwBtn');
const stopBtn = document.getElementById('stopBtn');

/* helper to set slider background fill
   This computes percent from the element's min/max so thumb and fill always match.
*/
function paintRange(el, v) {
  const min = Number(el.getAttribute('min') || 0);
  const max = Number(el.getAttribute('max') || 100);
  const val = Number(v);
  const percent = ((val - min) / (max - min)) * 100;
  el.style.setProperty('--val', percent + '%');
}

/* ensure fills match thumbs on load */
paintRange(servoSlider, servoSlider.value);
paintRange(speedSlider, speedSlider.value);

/* update servo UI from local slider only (no server-side overrides) */
function setServoUI(val, send=true) {
  currentAngle = Number(val);
  if (Number.isNaN(currentAngle)) return;
  servoSlider.value = currentAngle;
  servoText.textContent = currentAngle + '°';
  paintRange(servoSlider, currentAngle);
  drawGauge(currentAngle);
  if (send && ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'servoAngle', value: Number(currentAngle) }));
  }
}

/* initial render & resizing */
resizeCanvas();
drawGauge(currentAngle);

/* events */
servoSlider.addEventListener('input', (e)=> setServoUI(e.target.value, true));

speedSlider.addEventListener('input', (e)=>{
  const v = Number(e.target.value);
  speedText.textContent = v + '%';
  paintRange(speedSlider, v);
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'stepperSpeed', value: v }));
  }
});

/* Button behaviors per your spec:
   - When user clicks CW: CW greys out (disabled), CCW stays enabled; Stop enabled.
   - When user clicks CCW: CCW greys out, CW stays enabled; Stop enabled.
   - When Stop clicked: Stop greys out (disabled) and both CW/CCW become enabled (ready to be pressed).
*/
function setButtonState(button, disabled) {
  if (disabled) {
    button.classList.remove('btn-green'); button.classList.add('btn-grey'); button.disabled = true;
  } else {
    button.classList.remove('btn-grey'); button.classList.add('btn-green'); button.disabled = false;
  }
}

cwBtn.addEventListener('click', ()=>{
  // CW clicked -> CW greyed (disabled). CCW remains enabled.
  setButtonState(cwBtn, true);
  setButtonState(ccwBtn, false);
  setButtonState(stopBtn, false);
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ type:'stepperDir', value:'CW' }));
});

ccwBtn.addEventListener('click', ()=>{
  // CCW clicked -> CCW greyed (disabled). CW remains enabled.
  setButtonState(ccwBtn, true);
  setButtonState(cwBtn, false);
  setButtonState(stopBtn, false);
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ type:'stepperDir', value:'CCW' }));
});

stopBtn.addEventListener('click', ()=>{
  // Stop clicked -> Stop greyed, CW/CCW enabled
  setButtonState(stopBtn, true);
  setButtonState(cwBtn, false);
  setButtonState(ccwBtn, false);
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ type:'stepperDir', value:'STOP' }));
});

/* initial states */
setButtonState(cwBtn, false);
setButtonState(ccwBtn, false);
setButtonState(stopBtn, true);

/* once WS opens, send default CW + speed to the board so it starts rotating */
function sendDefaultsWhenOpen() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    // speed default 50
    ws.send(JSON.stringify({ type:'stepperSpeed', value: Number(speedSlider.value) }));
    ws.send(JSON.stringify({ type:'stepperDir', value:'CW' }));
    // send initial servo 0 as well
    ws.send(JSON.stringify({ type: 'servoAngle', value: Number(currentAngle) }));
  } else {
    setTimeout(sendDefaultsWhenOpen, 400);
  }
}
sendDefaultsWhenOpen();

</script>
</body>
</html>
)rawliteral";
