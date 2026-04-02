/**
 * ESP32 LED Controller - Express.js Server Integration
 * 
 * This is an example Node.js Express server that can:
 * 1. Receive color requests from a web frontend
 * 2. Forward commands to the ESP32 LED Controller
 * 3. Provide a REST API for your application
 *
 * Installation:
 *   npm init -y
 *   npm install express cors body-parser
 *
 * Usage:
 *   node express_server.js
 *   Then visit http://localhost:3000
 */

const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const http = require('http');
const EventSource = require('eventsource');

const app = express();
const PORT = 3000;

// Configuration
const ESP32_IP = '192.168.1.100';  // Change to your ESP32 IP
const ESP32_BASE_URL = `http://${ESP32_IP}`;
const FOCUSFRAME_API_URL = process.env.FOCUSFRAME_API_URL || 'http://localhost:8080';

// Logger
function log(level, message) {
  console.log(`[${new Date().toISOString()}] [${level}] ${message}`);
}

// Middleware
app.use(cors());
app.use(bodyParser.json());
app.use(express.static('public'));

// Color presets
const colorPresets = {
  red: { r: 255, g: 0, b: 0 },
  green: { r: 0, g: 255, b: 0 },
  blue: { r: 0, g: 0, b: 255 },
  yellow: { r: 255, g: 255, b: 0 },
  cyan: { r: 0, g: 255, b: 255 },
  magenta: { r: 255, g: 0, b: 255 },
  white: { r: 255, g: 255, b: 255 },
  off: { r: 0, g: 0, b: 0 }
};

/**
 * Helper function to make HTTP requests to ESP32
 */
function makeRequest(method, endpoint, payload = null) {
  return new Promise((resolve, reject) => {
    const url = new URL(ESP32_BASE_URL + endpoint);
    
    const options = {
      method: method,
      headers: {
        'Content-Type': 'application/json'
      }
    };

    const req = http.request(url, options, (res) => {
      let data = '';

      res.on('data', (chunk) => {
        data += chunk;
      });

      res.on('end', () => {
        try {
          const jsonData = JSON.parse(data);
          resolve({
            status: res.statusCode,
            data: jsonData
          });
        } catch (e) {
          resolve({
            status: res.statusCode,
            data: data
          });
        }
      });
    });

    req.on('error', (error) => {
      reject(error);
    });

    if (payload) {
      req.write(JSON.stringify(payload));
    }

    req.end();
  });
}

/**
 * Route: GET /
 * Returns HTML dashboard
 */
app.get('/', (req, res) => {
  const html = `
    <!DOCTYPE html>
    <html>
    <head>
      <title>ESP32 LED Controller Dashboard</title>
      <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          min-height: 100vh;
          display: flex;
          justify-content: center;
          align-items: center;
          padding: 20px;
        }
        .container {
          background: white;
          border-radius: 15px;
          box-shadow: 0 20px 60px rgba(0,0,0,0.3);
          max-width: 600px;
          width: 100%;
          padding: 40px;
        }
        h1 {
          color: #333;
          margin-bottom: 10px;
          text-align: center;
        }
        .subtitle {
          text-align: center;
          color: #666;
          margin-bottom: 30px;
          font-size: 14px;
        }
        .led-preview {
          width: 100px;
          height: 100px;
          border-radius: 50%;
          background: #444;
          margin: 0 auto 30px;
          border: 3px solid #ddd;
          box-shadow: inset 0 2px 5px rgba(0,0,0,0.5);
        }
        .section {
          margin-bottom: 30px;
        }
        .section-title {
          font-weight: bold;
          color: #333;
          margin-bottom: 15px;
          padding-bottom: 10px;
          border-bottom: 2px solid #667eea;
        }
        .color-grid {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 10px;
          margin-bottom: 20px;
        }
        .color-btn {
          padding: 12px;
          border: none;
          border-radius: 8px;
          color: white;
          font-weight: bold;
          cursor: pointer;
          transition: all 0.3s ease;
          font-size: 14px;
        }
        .color-btn:hover {
          transform: translateY(-2px);
          box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        }
        .color-btn:active {
          transform: translateY(0);
        }
        .btn-red { background: #ff4757; }
        .btn-green { background: #2ed573; }
        .btn-blue { background: #0984e3; }
        .btn-yellow { background: #ffa502; }
        .btn-white { background: #dfe6e9; color: #333; }
        .btn-off { background: #555; }
        .input-group {
          display: flex;
          gap: 10px;
          margin-bottom: 15px;
        }
        .input-group input {
          flex: 1;
          padding: 10px;
          border: 1px solid #ddd;
          border-radius: 5px;
          font-size: 14px;
        }
        .input-group button {
          padding: 10px 20px;
          background: #667eea;
          color: white;
          border: none;
          border-radius: 5px;
          cursor: pointer;
          font-weight: bold;
          transition: background 0.3s;
        }
        .input-group button:hover {
          background: #5568d3;
        }
        .status {
          background: #f0f0f0;
          padding: 15px;
          border-radius: 8px;
          margin-top: 20px;
          font-size: 12px;
          color: #666;
        }
        .status.online {
          background: #d4edda;
          color: #155724;
        }
        .status.offline {
          background: #f8d7da;
          color: #721c24;
        }
        .flash-section {
          background: #f9f9f9;
          padding: 15px;
          border-radius: 8px;
        }
        .flash-inputs {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 10px;
          margin-bottom: 10px;
        }
        .flash-inputs input {
          padding: 8px;
          border: 1px solid #ddd;
          border-radius: 5px;
        }
        label {
          display: block;
          font-size: 12px;
          color: #666;
          margin-bottom: 5px;
          font-weight: bold;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>🎨 LED Controller</h1>
        <p class="subtitle">Control your ESP32 RGB LED remotely</p>
        
        <div class="led-preview" id="ledPreview"></div>
        
        <div class="section">
          <div class="section-title">Preset Colors</div>
          <div class="color-grid">
            <button class="color-btn btn-red" onclick="setColor(255, 0, 0)">Red</button>
            <button class="color-btn btn-green" onclick="setColor(0, 255, 0)">Green</button>
            <button class="color-btn btn-blue" onclick="setColor(0, 0, 255)">Blue</button>
            <button class="color-btn btn-yellow" onclick="setColor(255, 255, 0)">Yellow</button>
            <button class="color-btn btn-white" onclick="setColor(255, 255, 255)">White</button>
            <button class="color-btn btn-off" onclick="setColor(0, 0, 0)">Off</button>
          </div>
        </div>

        <div class="section">
          <div class="section-title">Custom RGB</div>
          <div class="input-group">
            <input type="number" id="redInput" min="0" max="255" placeholder="Red (0-255)" value="255">
            <input type="number" id="greenInput" min="0" max="255" placeholder="Green (0-255)" value="0">
            <input type="number" id="blueInput" min="0" max="255" placeholder="Blue (0-255)" value="0">
          </div>
          <div class="input-group">
            <button onclick="setCustomColor()">Set Custom Color</button>
          </div>
        </div>

        <div class="section">
          <div class="section-title">Flash Effect</div>
          <div class="flash-section">
            <label>Color</label>
            <select id="flashColor" style="width: 100%; padding: 8px; margin-bottom: 10px; border-radius: 5px; border: 1px solid #ddd;">
              <option value="red">Red</option>
              <option value="green">Green</option>
              <option value="blue">Blue</option>
              <option value="yellow">Yellow</option>
              <option value="white">White</option>
            </select>
            
            <div class="flash-inputs">
              <div>
                <label>Flashes</label>
                <input type="number" id="flashCount" min="1" max="10" value="3">
              </div>
              <div>
                <label>Duration (ms)</label>
                <input type="number" id="flashDuration" min="100" max="2000" step="100" value="500">
              </div>
            </div>
            <button style="width: 100%; padding: 10px; background: #ff6b6b; color: white; border: none; border-radius: 5px; cursor: pointer; font-weight: bold;" onclick="flashLED()">Flash!</button>
          </div>
        </div>

        <div class="status" id="status">Checking device...</div>
      </div>

      <script>
        const ESP32_IP = '${ESP32_IP}';

        function rgb(r, g, b) {
          return \`rgb(\${r},\${g},\${b})\`;
        }

        function updatePreview(r, g, b) {
          document.getElementById('ledPreview').style.background = rgb(r, g, b);
          document.getElementById('ledPreview').style.boxShadow = \`inset 0 2px 5px rgba(0,0,0,0.5), 0 0 20px \${rgb(r, g, b)}\`;
        }

        async function setColor(r, g, b) {
          try {
            document.getElementById('redInput').value = r;
            document.getElementById('greenInput').value = g;
            document.getElementById('blueInput').value = b;
            
            const response = await fetch('/api/color', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ red: r, green: g, blue: b })
            });
            
            const data = await response.json();
            updatePreview(data.red, data.green, data.blue);
            updateStatus(true, 'Color changed');
          } catch (error) {
            updateStatus(false, 'Failed to change color');
          }
        }

        async function setCustomColor() {
          const r = parseInt(document.getElementById('redInput').value) || 0;
          const g = parseInt(document.getElementById('greenInput').value) || 0;
          const b = parseInt(document.getElementById('blueInput').value) || 0;
          await setColor(r, g, b);
        }

        async function flashLED() {
          try {
            const color = document.getElementById('flashColor').value;
            const count = parseInt(document.getElementById('flashCount').value);
            const duration = parseInt(document.getElementById('flashDuration').value);

            const response = await fetch('/api/flash', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ color, flashes: count, duration })
            });
            
            const data = await response.json();
            updateStatus(true, \`Flashed \${count} times\`);
          } catch (error) {
            updateStatus(false, 'Failed to flash');
          }
        }

        function updateStatus(online, message) {
          const statusEl = document.getElementById('status');
          statusEl.className = 'status ' + (online ? 'online' : 'offline');
          statusEl.textContent = message || (online ? 'Device online' : 'Device offline');
        }

        // Check status on load
        setTimeout(() => {
          fetch('/api/status')
            .then(r => r.json())
            .then(data => updateStatus(true, 'Device online - ' + data.ip))
            .catch(() => updateStatus(false, 'Device offline'));
        }, 500);
      </script>
    </body>
    </html>
  `;
  res.send(html);
});

/**
 * Route: POST /api/color
 * Set LED color
 */
app.post('/api/color', async (req, res) => {
  try {
    const { red, green, blue } = req.body;
    
    const esp32Response = await makeRequest('POST', '/color', {
      red: Math.max(0, Math.min(255, red)),
      green: Math.max(0, Math.min(255, green)),
      blue: Math.max(0, Math.min(255, blue))
    });

    res.json(esp32Response.data);
  } catch (error) {
    res.status(500).json({ error: 'Failed to set color', details: error.message });
  }
});

/**
 * Route: POST /api/flash
 * Flash LED
 */
app.post('/api/flash', async (req, res) => {
  try {
    const { color, flashes = 3, duration = 500 } = req.body;
    
    const colorMap = {
      red: { red: 255, green: 0, blue: 0 },
      green: { red: 0, green: 255, blue: 0 },
      blue: { red: 0, green: 0, blue: 255 },
      yellow: { red: 255, green: 255, blue: 0 },
      white: { red: 255, green: 255, blue: 255 }
    };

    const selectedColor = colorMap[color] || colorMap.red;

    const esp32Response = await makeRequest('POST', '/flash', {
      ...selectedColor,
      flashes: Math.max(1, Math.min(100, flashes)),
      duration: Math.max(50, Math.min(5000, duration))
    });

    res.json(esp32Response.data);
  } catch (error) {
    res.status(500).json({ error: 'Failed to flash', details: error.message });
  }
});

/**
 * SSE Client — Subscribe to focusframe-api color events
 * Auto-connects on startup and reconnects on failure.
 */
function connectToSSE() {
  const sseUrl = `${FOCUSFRAME_API_URL}/api/sse/doormount`;
  log('INFO', `Connecting to SSE stream at ${sseUrl}`);

  const es = new EventSource(sseUrl);

  es.addEventListener('color', async (event) => {
    try {
      const { type, red, green, blue } = JSON.parse(event.data);
      log('INFO', `SSE color event received — type: ${type}, RGB(${red}, ${green}, ${blue})`);
      await makeRequest('POST', '/color', { red, green, blue });
      log('INFO', `LED set to RGB(${red}, ${green}, ${blue}) successfully`);
    } catch (error) {
      log('ERROR', `Failed to handle SSE color event: ${error.message}`);
    }
  });

  es.onopen = () => {
    log('INFO', 'SSE connection established');
  };

  es.onerror = (err) => {
    log('WARN', `SSE connection error — will auto-reconnect`);
  };
}

/**
 * Route: GET /api/status
 * Get device status
 */
app.get('/api/status', async (req, res) => {
  try {
    const esp32Response = await makeRequest('GET', '/status');
    res.json(esp32Response.data);
  } catch (error) {
    res.status(500).json({ error: 'Device offline', details: error.message });
  }
});

// Start server
app.listen(PORT, () => {
  log('INFO', `ESP32 LED Controller server started`);
  log('INFO', `Listening on http://localhost:${PORT}`);
  connectToSSE();
  log('INFO', `ESP32 target: ${ESP32_BASE_URL}`);
  log('INFO', `Listening for working signal on POST http://localhost:${PORT}/api/signal/working`);
});

// Graceful shutdown
process.on('SIGINT', () => {
  log('INFO', 'Server shutting down — SIGINT received');
  process.exit(0);
});
