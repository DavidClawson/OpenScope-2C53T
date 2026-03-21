import { useState, useRef, useEffect, useCallback } from 'react'
import './App.css'

// Original image dimensions
const IMG_WIDTH = 388
const IMG_HEIGHT = 735
const DEFAULT_SCALE = 1.2

// LCD screen region within the image (pixel coordinates on the original 388x735 image)
const LCD_REGION = {
  x: 42,
  y: 72,
  width: 305,
  height: 228,
}

// LCD native resolution
const LCD_WIDTH = 320
const LCD_HEIGHT = 240

// Button hotspot regions (x, y, width, height on the original image)
const BUTTONS = [
  // Row 1: MOVE, SELECT, TRIGGER, PRM
  { id: 'MOVE',    x: 55,  y: 345, w: 55, h: 20 },
  { id: 'SELECT',  x: 125, y: 345, w: 60, h: 20 },
  { id: 'TRIGGER', x: 205, y: 345, w: 70, h: 20 },
  { id: 'PRM',     x: 295, y: 345, w: 45, h: 20 },

  // CH1, CH2
  { id: 'CH1',     x: 40,  y: 385, w: 55, h: 30 },
  { id: 'CH2',     x: 293, y: 385, w: 55, h: 30 },

  // AUTO, SAVE
  { id: 'AUTO',    x: 40,  y: 430, w: 55, h: 30 },
  { id: 'SAVE',    x: 293, y: 430, w: 55, h: 30 },

  // D-pad
  { id: 'UP',      x: 165, y: 375, w: 52, h: 28 },
  { id: 'DOWN',    x: 165, y: 452, w: 52, h: 28 },
  { id: 'LEFT',    x: 118, y: 403, w: 42, h: 38 },
  { id: 'RIGHT',   x: 222, y: 403, w: 42, h: 38 },
  { id: 'OK',      x: 160, y: 403, w: 62, h: 38 },

  // Power button (the red circle button)
  { id: 'POWER',   x: 55, y: 480, w: 40, h: 40 },

  // MENU
  { id: 'MENU',    x: 278, y: 480, w: 65, h: 30 },
]

function LCDCanvas({ framebuffer }) {
  const canvasRef = useRef(null)

  useEffect(() => {
    const canvas = canvasRef.current
    const ctx = canvas.getContext('2d')

    if (framebuffer && framebuffer.length > 0) {
      // Decode RGB565 framebuffer from emulator
      const imageData = ctx.createImageData(LCD_WIDTH, LCD_HEIGHT)
      for (let i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        const lo = framebuffer[i * 2]
        const hi = framebuffer[i * 2 + 1]
        const pixel = lo | (hi << 8)
        imageData.data[i * 4 + 0] = ((pixel >> 11) & 0x1f) << 3
        imageData.data[i * 4 + 1] = ((pixel >> 5) & 0x3f) << 2
        imageData.data[i * 4 + 2] = (pixel & 0x1f) << 3
        imageData.data[i * 4 + 3] = 255
      }
      ctx.putImageData(imageData, 0, 0)
    } else {
      drawPlaceholder(ctx)
    }
  }, [framebuffer])

  return (
    <canvas
      ref={canvasRef}
      width={LCD_WIDTH}
      height={LCD_HEIGHT}
      className="lcd-canvas"
    />
  )
}

function drawPlaceholder(ctx) {
  ctx.fillStyle = '#000000'
  ctx.fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT)

  // Grid
  ctx.strokeStyle = '#1a1a2e'
  ctx.lineWidth = 0.5
  for (let x = 0; x < LCD_WIDTH; x += 32) {
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, LCD_HEIGHT); ctx.stroke()
  }
  for (let y = 0; y < LCD_HEIGHT; y += 24) {
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(LCD_WIDTH, y); ctx.stroke()
  }

  // Center cross
  ctx.strokeStyle = '#333355'
  ctx.beginPath()
  ctx.moveTo(LCD_WIDTH / 2, 0); ctx.lineTo(LCD_WIDTH / 2, LCD_HEIGHT)
  ctx.moveTo(0, LCD_HEIGHT / 2); ctx.lineTo(LCD_WIDTH, LCD_HEIGHT / 2)
  ctx.stroke()

  // CH1 waveform (yellow sine)
  ctx.strokeStyle = '#ffff00'
  ctx.lineWidth = 1.5
  ctx.beginPath()
  for (let x = 0; x < LCD_WIDTH; x++) {
    const y = LCD_HEIGHT / 2 - 40 * Math.sin((x / LCD_WIDTH) * Math.PI * 8)
    x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
  }
  ctx.stroke()

  // CH2 waveform (cyan square)
  ctx.strokeStyle = '#00ffff'
  ctx.lineWidth = 1.5
  ctx.beginPath()
  for (let x = 0; x < LCD_WIDTH; x++) {
    const phase = ((x / LCD_WIDTH) * 8) % 2
    const y = LCD_HEIGHT / 2 + (phase < 1 ? -25 : 25) + 60
    x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
  }
  ctx.stroke()

  // Status bar
  ctx.fillStyle = '#111122'
  ctx.fillRect(0, 0, LCD_WIDTH, 14)
  ctx.font = '9px monospace'
  ctx.fillStyle = '#888'; ctx.fillText('FNIRSI', 4, 10)
  ctx.fillStyle = '#fff'; ctx.fillText('H=50uS', 60, 10)
  ctx.fillStyle = '#f44'; ctx.fillText('STOP', 140, 10)
  ctx.fillStyle = '#fff'; ctx.fillText('2C53T', 275, 10)

  // Bottom bar
  ctx.fillStyle = '#111122'
  ctx.fillRect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14)
  ctx.font = '9px monospace'
  ctx.fillStyle = '#ff0'; ctx.fillText('2V X1 DC', 4, LCD_HEIGHT - 4)
  ctx.fillStyle = '#0f0'; ctx.fillText('Auto', 100, LCD_HEIGHT - 4)
  ctx.fillStyle = '#ff0'; ctx.fillText('CH1', 170, LCD_HEIGHT - 4)
  ctx.fillStyle = '#0ff'; ctx.fillText('200mV', 250, LCD_HEIGHT - 4)

  // Trigger marker
  ctx.fillStyle = '#00ff00'
  ctx.beginPath()
  ctx.moveTo(LCD_WIDTH - 6, LCD_HEIGHT / 2 - 4)
  ctx.lineTo(LCD_WIDTH - 1, LCD_HEIGHT / 2)
  ctx.lineTo(LCD_WIDTH - 6, LCD_HEIGHT / 2 + 4)
  ctx.fill()

  // Watermark
  ctx.fillStyle = 'rgba(255, 255, 255, 0.06)'
  ctx.font = '20px monospace'
  ctx.fillText('EMULATOR', 95, LCD_HEIGHT / 2 + 7)
}

function App() {
  const [framebuffer, setFramebuffer] = useState(null)
  const [connected, setConnected] = useState(false)
  const [log, setLog] = useState([])
  const [activeButton, setActiveButton] = useState(null)
  const [showHotspots, setShowHotspots] = useState(false)
  const [scale, setScale] = useState(DEFAULT_SCALE)
  const [wsUrl, setWsUrl] = useState('ws://localhost:8765')
  const wsRef = useRef(null)
  const containerRef = useRef(null)

  const addLog = useCallback((msg) => {
    setLog(prev => [...prev.slice(-100), `${new Date().toLocaleTimeString()} ${msg}`])
  }, [])

  const handleButtonPress = useCallback((buttonId) => {
    setActiveButton(buttonId)
    addLog(`Button: ${buttonId}`)
    setTimeout(() => setActiveButton(null), 150)

    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ type: 'button', button: buttonId }))
    }
  }, [addLog])

  const handleImageClick = useCallback((e) => {
    const container = containerRef.current
    if (!container) return

    const rect = container.getBoundingClientRect()
    const scaleX = IMG_WIDTH / rect.width
    const scaleY = IMG_HEIGHT / rect.height
    const x = (e.clientX - rect.left) * scaleX
    const y = (e.clientY - rect.top) * scaleY

    for (const btn of BUTTONS) {
      if (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h) {
        handleButtonPress(btn.id)
        return
      }
    }
  }, [handleButtonPress])

  const connectEmulator = useCallback(() => {
    try {
      const ws = new WebSocket(wsUrl)
      ws.binaryType = 'arraybuffer'
      ws.onopen = () => { setConnected(true); addLog('Connected to emulator') }
      ws.onmessage = (event) => {
        if (event.data instanceof ArrayBuffer) {
          setFramebuffer(new Uint8Array(event.data))
        } else {
          try {
            const msg = JSON.parse(event.data)
            if (msg.type === 'log') addLog(`EMU: ${msg.message}`)
          } catch {}
        }
      }
      ws.onclose = () => { setConnected(false); addLog('Disconnected') }
      ws.onerror = () => { addLog('Connection error') }
      wsRef.current = ws
    } catch (e) {
      addLog(`Failed: ${e.message}`)
    }
  }, [wsUrl, addLog])

  return (
    <div className="app">
      <div className="toolbar">
        <h1>OpenScope-2C53T</h1>
        <div className="toolbar-controls">
          <div className="zoom-controls">
            <button onClick={() => setScale(s => Math.max(0.5, s - 0.1))} className="zoom-btn">-</button>
            <span className="zoom-label">{Math.round(scale * 100)}%</span>
            <button onClick={() => setScale(s => Math.min(2.5, s + 0.1))} className="zoom-btn">+</button>
          </div>
          <label className="hotspot-toggle">
            <input
              type="checkbox"
              checked={showHotspots}
              onChange={(e) => setShowHotspots(e.target.checked)}
            />
            Show hotspots
          </label>
          <input
            type="text"
            value={wsUrl}
            onChange={(e) => setWsUrl(e.target.value)}
            className="ws-input"
          />
          <button
            onClick={connectEmulator}
            className={`connect-btn ${connected ? 'connected' : ''}`}
          >
            {connected ? 'Connected' : 'Connect'}
          </button>
        </div>
      </div>

      <div className="main-content">
        {/* Device with image overlay */}
        <div
          className="device-wrapper"
          ref={containerRef}
          onClick={handleImageClick}
          style={{
            width: IMG_WIDTH * scale,
            height: IMG_HEIGHT * scale,
          }}
        >
          {/* Product photo background */}
          <img
            src="/scope.jpg"
            alt="FNIRSI 2C53T"
            className="device-image"
            draggable={false}
          />

          {/* LCD overlay - positioned exactly over the screen area */}
          <div
            className="lcd-overlay"
            style={{
              left: LCD_REGION.x * scale,
              top: LCD_REGION.y * scale,
              width: LCD_REGION.width * scale,
              height: LCD_REGION.height * scale,
            }}
          >
            <LCDCanvas framebuffer={framebuffer} />
          </div>

          {/* Button hotspot overlays (debug visualization) */}
          {showHotspots && BUTTONS.map((btn) => (
            <div
              key={btn.id}
              className={`hotspot ${activeButton === btn.id ? 'hotspot-active' : ''}`}
              style={{
                left: btn.x * scale,
                top: btn.y * scale,
                width: btn.w * scale,
                height: btn.h * scale,
              }}
            >
              <span className="hotspot-label">{btn.id}</span>
            </div>
          ))}

          {/* Active button flash */}
          {activeButton && !showHotspots && (() => {
            const btn = BUTTONS.find(b => b.id === activeButton)
            if (!btn) return null
            return (
              <div
                className="hotspot hotspot-flash"
                style={{
                  left: btn.x * scale,
                  top: btn.y * scale,
                  width: btn.w * scale,
                  height: btn.h * scale,
                }}
              />
            )
          })()}
        </div>

        {/* Log panel */}
        <div className="log-panel">
          <h3>Event Log</h3>
          <div className="log-content">
            {log.length === 0 ? (
              <div className="log-hint">
                Click buttons on the device image to interact.
                <br />Enable "Show hotspots" to see clickable regions.
                <br />Connect to emulator for live LCD output.
              </div>
            ) : (
              log.map((entry, i) => (
                <div key={i} className="log-entry">{entry}</div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  )
}

export default App
