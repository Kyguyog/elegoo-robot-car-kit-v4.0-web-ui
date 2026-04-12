#include <WiFi.h>
#include "Arduino.h"
#include "esp_http_server.h"

namespace {

httpd_handle_t control_httpd = NULL;

const char CONTROL_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>UMRT Control</title>
  <style>
    :root {
      --bg: #101820;
      --panel: #17212b;
      --panel-2: #223142;
      --accent: #ff6b35;
      --accent-2: #2ec4b6;
      --text: #f4f7fb;
      --muted: #9db0c2;
      --danger: #ff4d6d;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      font-family: Arial, Helvetica, sans-serif;
      background:
        radial-gradient(circle at top, rgba(46, 196, 182, 0.15), transparent 30%),
        linear-gradient(180deg, #0b1218, var(--bg));
      color: var(--text);
      min-height: 100vh;
    }

    .wrap {
      max-width: 960px;
      margin: 0 auto;
      padding: 24px 16px 40px;
    }

    .hero, .panel {
      background: rgba(23, 33, 43, 0.92);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 18px;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.25);
    }

    .hero {
      padding: 20px;
      margin-bottom: 16px;
    }

    h1 {
      margin: 0 0 8px;
      font-size: 32px;
    }

    p {
      margin: 0;
      color: var(--muted);
      line-height: 1.5;
    }

    .grid {
      display: grid;
      gap: 16px;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    }

    .panel {
      padding: 18px;
    }

    .stats {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
      margin-top: 18px;
    }

    .stat {
      background: var(--panel-2);
      border-radius: 14px;
      padding: 12px;
    }

    .label {
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--muted);
    }

    .value {
      margin-top: 6px;
      font-size: 18px;
      font-weight: 700;
      word-break: break-word;
    }

    .pad {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-top: 14px;
    }

    button, input, textarea {
      font: inherit;
    }

    .key {
      min-height: 64px;
      border: 0;
      border-radius: 16px;
      background: linear-gradient(180deg, #2c4156, #1f2d3a);
      color: var(--text);
      font-size: 20px;
      font-weight: 700;
      cursor: pointer;
      transition: transform 0.08s ease, filter 0.08s ease;
    }

    .key:active, .key.active {
      transform: translateY(2px) scale(0.98);
      filter: brightness(1.08);
    }

    .accent { background: linear-gradient(180deg, #ff874f, var(--accent)); }
    .teal { background: linear-gradient(180deg, #48dacd, var(--accent-2)); }
    .danger { background: linear-gradient(180deg, #ff7290, var(--danger)); }

    .wide { grid-column: span 3; }

    .row {
      display: flex;
      gap: 10px;
      align-items: center;
      flex-wrap: wrap;
      margin-top: 14px;
    }

    input[type="range"] {
      width: 100%;
    }

    textarea {
      width: 100%;
      min-height: 140px;
      margin-top: 12px;
      padding: 12px;
      border-radius: 14px;
      border: 1px solid rgba(255, 255, 255, 0.1);
      background: #0d141b;
      color: var(--text);
      resize: vertical;
    }

    .send {
      width: 100%;
      min-height: 52px;
      margin-top: 10px;
      border: 0;
      border-radius: 14px;
      background: linear-gradient(180deg, #ff874f, var(--accent));
      color: #fff;
      font-weight: 700;
      cursor: pointer;
    }

    .hint, .log {
      margin-top: 12px;
      font-size: 14px;
      color: var(--muted);
    }

    .log {
      min-height: 20px;
      color: var(--accent-2);
    }

    @media (max-width: 640px) {
      h1 { font-size: 26px; }
      .stats { grid-template-columns: 1fr; }
      .key { min-height: 58px; font-size: 18px; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <section class="hero">
      <h1>UMRT Robot Control</h1>
      <div class="stats">
        <div class="stat">
          <div class="label">SSID</div>
          <div class="value" id="ssid">Loading...</div>
        </div>
        <div class="stat">
          <div class="label">IP</div>
          <div class="value" id="ip">Loading...</div>
        </div>
        <div class="stat">
          <div class="label">Clients</div>
          <div class="value" id="clients">0</div>
        </div>
      </div>
    </section>

    <div class="grid">
      <section class="panel">
        <div class="label">Drive Pad</div>
        <div class="pad">
          <div></div>
          <button class="key accent" data-key="w">W</button>
          <div></div>
          <button class="key teal" data-key="a">A</button>
          <button class="key danger" data-key="x">STOP</button>
          <button class="key teal" data-key="d">D</button>
          <div></div>
          <button class="key accent" data-key="s">S</button>
          <div></div>
        </div>
        <div class="hint">Keyboard: <code>W A S D</code>, arrow keys, and <code>X</code> for stop. Controller: left stick turns, triggers drive, D-pad up/down changes speed, shoulders stop.</div>
        <div class="row">
          <input id="speed" type="range" min="0" max="255" value="180">
          <div class="value">Speed: <span id="speedValue">180</span></div>
        </div>
        <div class="log" id="log"></div>
      </section>
    </div>
  </div>

  <script>
    const logEl = document.getElementById('log');
    const speedEl = document.getElementById('speed');
    const speedValueEl = document.getElementById('speedValue');

    function setLog(message, isError = false) {
      logEl.textContent = message;
      logEl.style.color = isError ? '#ff8da1' : '#2ec4b6';
    }

    async function refreshStatus() {
      try {
        const response = await fetch('/status');
        const status = await response.json();
        document.getElementById('ssid').textContent = status.ssid;
        document.getElementById('ip').textContent = status.ip;
        document.getElementById('clients').textContent = status.clients;
      } catch (error) {
        setLog('Status refresh failed', true);
      }
    }

    async function sendJson(payload) {
      const response = await fetch('/json_command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: payload
      });
      if (!response.ok) {
        throw new Error(`JSON send failed: ${response.status}`);
      }
      const text = await response.text();
      setLog(text || 'JSON command sent');
    }

    function pressButton(button) {
      button.classList.add('active');
    }

    function releaseButton(button) {
      button.classList.remove('active');
    }

    const keyboardState = {
      w: false,
      a: false,
      s: false,
      d: false
    };

    let lastDrivePayload = '';
    let gamepadActive = false;
    let lastGamepadSignature = '';
    let lastDpadAdjustTime = 0;

    function buildDriveCommand(forward, reverse, left, right, speed) {
      if (forward) {
        if (left && !right) {
          return { N: 102, D1: 5, D2: speed };
        }
        if (!left && right) {
          return { N: 102, D1: 7, D2: speed };
        }
        return { N: 102, D1: 1, D2: speed };
      }

      if (reverse) {
        if (left && !right) {
          return { N: 102, D1: 6, D2: speed };
        }
        if (!left && right) {
          return { N: 102, D1: 8, D2: speed };
        }
        return { N: 102, D1: 2, D2: speed };
      }

      if (left && !right) {
        return { N: 102, D1: 3, D2: speed };
      }

      if (!left && right) {
        return { N: 102, D1: 4, D2: speed };
      }

      return { H: 1, N: 1, D1: 0, D2: 0, D3: 1 };
    }

    function buildKeyboardDriveCommand() {
      return buildDriveCommand(
        keyboardState.w,
        keyboardState.s,
        keyboardState.a,
        keyboardState.d,
        Number(speedEl.value)
      );
    }

    async function syncDriveCommand(commandObject = null) {
      const payload = JSON.stringify(commandObject || buildKeyboardDriveCommand());
      if (payload === lastDrivePayload) {
        return;
      }
      lastDrivePayload = payload;
      await sendJson(payload);
    }

    async function handleDriveKeyDown(key) {
      if (!Object.prototype.hasOwnProperty.call(keyboardState, key)) {
        return;
      }
      keyboardState[key] = true;
      if (!gamepadActive) {
        await syncDriveCommand();
      }
    }

    async function handleDriveKeyUp(key) {
      if (!Object.prototype.hasOwnProperty.call(keyboardState, key)) {
        return;
      }
      keyboardState[key] = false;
      if (!gamepadActive) {
        await syncDriveCommand();
      }
    }

    async function sendImmediateCommand(payloadObject, label) {
      const payload = JSON.stringify(payloadObject);
      lastDrivePayload = payload;
      await sendJson(payload);
      setLog(label);
    }

    function normalizeKeyboardKey(key) {
      const normalized = key.toLowerCase();
      if (normalized === 'arrowup') return 'w';
      if (normalized === 'arrowleft') return 'a';
      if (normalized === 'arrowdown') return 's';
      if (normalized === 'arrowright') return 'd';
      return normalized;
    }

    function clampSpeed(value) {
      return Math.max(0, Math.min(255, value));
    }

    function updateSpeedDisplay() {
      speedValueEl.textContent = speedEl.value;
    }

    function setSpeedValue(nextSpeed) {
      speedEl.value = String(clampSpeed(nextSpeed));
      updateSpeedDisplay();
    }

    function buildGamepadCommand(gamepad) {
      const leftX = gamepad.axes[0] || 0;
      const leftTrigger = gamepad.buttons[6] ? gamepad.buttons[6].value : 0;
      const rightTrigger = gamepad.buttons[7] ? gamepad.buttons[7].value : 0;
      const shoulderStop = (gamepad.buttons[4] && gamepad.buttons[4].pressed) || (gamepad.buttons[5] && gamepad.buttons[5].pressed);

      if (shoulderStop) {
        return {
          active: true,
          command: { H: 1, N: 1, D1: 0, D2: 0, D3: 1 },
          signature: 'stop'
        };
      }

      const deadzone = 0.35;
      const triggerDeadzone = 0.15;
      const left = leftX < -deadzone;
      const right = leftX > deadzone;
      const forward = rightTrigger > triggerDeadzone;
      const reverse = leftTrigger > triggerDeadzone;

      const active = left || right || forward || reverse;
      const signature = `${forward ? 1 : 0}${reverse ? 1 : 0}${left ? 1 : 0}${right ? 1 : 0}`;

      return {
        active,
        command: buildDriveCommand(forward, reverse, left, right, Number(speedEl.value)),
        signature
      };
    }

    function pollGamepad() {
      const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
      const gamepad = gamepads && gamepads[0];

      if (gamepad) {
        const now = Date.now();
        if (gamepad.buttons[12] && gamepad.buttons[12].pressed && now - lastDpadAdjustTime > 180) {
          setSpeedValue(Number(speedEl.value) + 10);
          lastDpadAdjustTime = now;
        } else if (gamepad.buttons[13] && gamepad.buttons[13].pressed && now - lastDpadAdjustTime > 180) {
          setSpeedValue(Number(speedEl.value) - 10);
          lastDpadAdjustTime = now;
        }

        const state = buildGamepadCommand(gamepad);

        if (state.active) {
          gamepadActive = true;
          if (state.signature !== lastGamepadSignature || JSON.stringify(state.command) !== lastDrivePayload) {
            lastGamepadSignature = state.signature;
            syncDriveCommand(state.command).catch((error) => setLog(error.message, true));
          }
        } else if (gamepadActive) {
          gamepadActive = false;
          lastGamepadSignature = '';
          syncDriveCommand().catch((error) => setLog(error.message, true));
        }
      } else if (gamepadActive) {
        gamepadActive = false;
        lastGamepadSignature = '';
        syncDriveCommand().catch((error) => setLog(error.message, true));
      }

      window.requestAnimationFrame(pollGamepad);
    }

    document.querySelectorAll('[data-key]').forEach((button) => {
      const key = button.dataset.key;
      button.addEventListener('mousedown', async () => {
        pressButton(button);
        try {
          if (['w', 'a', 's', 'd'].includes(key)) {
            await handleDriveKeyDown(key);
          } else if (key === 'x') {
            await sendImmediateCommand({ H: 1, N: 1, D1: 0, D2: 0, D3: 1 }, `Sent ${key.toUpperCase()} stop`);
          }
        } catch (error) {
          setLog(error.message, true);
        }
      });
      button.addEventListener('touchstart', async (event) => {
        event.preventDefault();
        pressButton(button);
        try {
          if (['w', 'a', 's', 'd'].includes(key)) {
            await handleDriveKeyDown(key);
          } else if (key === 'x') {
            await sendImmediateCommand({ H: 1, N: 1, D1: 0, D2: 0, D3: 1 }, `Sent ${key.toUpperCase()} stop`);
          }
        } catch (error) {
          setLog(error.message, true);
        }
      }, { passive: false });

      const release = async (event) => {
        if (event) {
          event.preventDefault();
        }
        releaseButton(button);
        try {
          if (['w', 'a', 's', 'd'].includes(key)) {
            await handleDriveKeyUp(key);
          }
        } catch (error) {
          setLog(error.message, true);
        }
      };

      button.addEventListener('mouseup', release);
      button.addEventListener('mouseleave', release);
      button.addEventListener('touchend', release, { passive: false });
    });

    document.addEventListener('keydown', async (event) => {
      if (event.repeat) {
        return;
      }

      const key = normalizeKeyboardKey(event.key);
      const allowed = ['w', 'a', 's', 'd', 'x'];
      if (!allowed.includes(key)) {
        return;
      }

      try {
        if (['w', 'a', 's', 'd'].includes(key)) {
          await handleDriveKeyDown(key);
        } else if (key === 'x') {
          await sendImmediateCommand({ H: 1, N: 1, D1: 0, D2: 0, D3: 1 }, `Sent ${key.toUpperCase()} stop`);
        }
      } catch (error) {
        setLog(error.message, true);
      }
    });

    document.addEventListener('keyup', async (event) => {
      const key = normalizeKeyboardKey(event.key);
      if (!['w', 'a', 's', 'd'].includes(key)) {
        return;
      }

      try {
        await handleDriveKeyUp(key);
      } catch (error) {
        setLog(error.message, true);
      }
    });

    speedEl.addEventListener('input', () => {
      updateSpeedDisplay();
      if (!gamepadActive && (keyboardState.w || keyboardState.a || keyboardState.s || keyboardState.d)) {
        syncDriveCommand().catch((error) => setLog(error.message, true));
      }
    });

    window.addEventListener('gamepadconnected', (event) => {
      setLog(`Controller connected: ${event.gamepad.id}`);
    });

    window.addEventListener('gamepaddisconnected', () => {
      gamepadActive = false;
      lastGamepadSignature = '';
      setLog('Controller disconnected', true);
    });

    refreshStatus();
    updateSpeedDisplay();
    window.requestAnimationFrame(pollGamepad);
    setInterval(refreshStatus, 3000);
  </script>
</body>
</html>
)HTML";

void setCorsHeaders(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

esp_err_t sendBadRequest(httpd_req_t *req, const char *message = "Bad Request") {
  setCorsHeaders(req);
  httpd_resp_set_status(req, "400 Bad Request");
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  return httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
}

esp_err_t indexHandler(httpd_req_t *req) {
  setCorsHeaders(req);
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, CONTROL_PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t statusHandler(httpd_req_t *req) {
  char response[160];
  IPAddress ip = WiFi.softAPIP();
  snprintf(
    response,
    sizeof(response),
    "{\"ssid\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"clients\":%d}",
    WiFi.softAPSSID().c_str(),
    ip[0], ip[1], ip[2], ip[3],
    WiFi.softAPgetStationNum());

  setCorsHeaders(req);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t unsupportedCameraHandler(httpd_req_t *req) {
  static const char response[] = "{\"ok\":false,\"message\":\"Camera features removed from this firmware\"}";
  setCorsHeaders(req);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_status(req, "410 Gone");
  return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t keypressHandler(httpd_req_t *req) {
  const size_t query_len = httpd_req_get_url_query_len(req) + 1;
  if (query_len <= 1) {
    return sendBadRequest(req, "Missing key query parameter");
  }

  char *query = static_cast<char *>(malloc(query_len));
  if (query == NULL) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char key[16] = {0};
  esp_err_t result = httpd_req_get_url_query_str(req, query, query_len);
  if (result == ESP_OK) {
    result = httpd_query_key_value(query, "key", key, sizeof(key));
  }
  free(query);

  if (result != ESP_OK || key[0] == '\0') {
    return sendBadRequest(req, "Invalid key query parameter");
  }

  Serial.printf("Key pressed: %s\n", key);
  Serial2.print(key);
  Serial2.print("\n");

  setCorsHeaders(req);
  return httpd_resp_send(req, NULL, 0);
}

esp_err_t jsonCommandHandler(httpd_req_t *req) {
  if (req->content_len <= 0 || req->content_len > 1024) {
    return sendBadRequest(req, "Invalid JSON body length");
  }

  String body;
  body.reserve(req->content_len);

  int remaining = req->content_len;
  while (remaining > 0) {
    char chunk[128];
    const int to_read = remaining > static_cast<int>(sizeof(chunk) - 1) ? sizeof(chunk) - 1 : remaining;
    const int received = httpd_req_recv(req, chunk, to_read);
    if (received <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        httpd_resp_send_408(req);
      }
      return ESP_FAIL;
    }

    chunk[received] = '\0';
    body += chunk;
    remaining -= received;
  }

  Serial.println("Received JSON command:");
  Serial.println(body);
  Serial2.print(body);
  Serial2.print("\n");

  setCorsHeaders(req);
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  return httpd_resp_send(req, "Command forwarded to motor controller", HTTPD_RESP_USE_STRLEN);
}

}  // namespace

void startControlServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 8;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = indexHandler,
    .user_ctx = NULL
  };

  httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = statusHandler,
    .user_ctx = NULL
  };

  httpd_uri_t keypress_uri = {
    .uri = "/keypress",
    .method = HTTP_GET,
    .handler = keypressHandler,
    .user_ctx = NULL
  };

  httpd_uri_t json_command_uri = {
    .uri = "/json_command",
    .method = HTTP_POST,
    .handler = jsonCommandHandler,
    .user_ctx = NULL
  };

  httpd_uri_t removed_camera_uri = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = unsupportedCameraHandler,
    .user_ctx = NULL
  };

  httpd_uri_t removed_stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = unsupportedCameraHandler,
    .user_ctx = NULL
  };

  httpd_uri_t removed_control_uri = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = unsupportedCameraHandler,
    .user_ctx = NULL
  };

  if (httpd_start(&control_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(control_httpd, &index_uri);
    httpd_register_uri_handler(control_httpd, &status_uri);
    httpd_register_uri_handler(control_httpd, &keypress_uri);
    httpd_register_uri_handler(control_httpd, &json_command_uri);
    httpd_register_uri_handler(control_httpd, &removed_camera_uri);
    httpd_register_uri_handler(control_httpd, &removed_stream_uri);
    httpd_register_uri_handler(control_httpd, &removed_control_uri);
    Serial.printf("Control server ready at http://%s/\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("Failed to start control HTTP server");
  }
}
