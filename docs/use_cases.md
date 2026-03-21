# Use Cases and Module Roadmap

## Signal Path Architecture

Understanding which signals go where is key to knowing what's possible:

```
Scope probes → Analog front end → External ADC → FPGA → MCU (via USART2)
               (amps, attenuators,  (250 MS/s)    (buffers +    (receives
                protection)                         processes)    digital data)

Multimeter → Separate analog path → FPGA or MCU internal ADC (12-bit)

Signal gen → MCU DAC (12-bit) → Output amplifier → BNC output
```

The oscilloscope analog signals never touch the MCU directly — all high-speed sampling is done by the FPGA. The MCU receives digital sample buffers and handles display, UI, and storage.

## CAN Bus Hardware Note

The GD32F307 has 2x CAN controllers built in, but **CAN1 shares pins with USB** (PA11/PA12). Since USB is used, CAN1's default pins are occupied. Options:

- CAN0 can be remapped to PD0/PD1 via AFIO
- CAN1 alternate pins: PB12/PB13
- Physical availability depends on PCB routing — need to check for unused pads/test points
- Even without native CAN pins, CAN can be decoded from the oscilloscope's analog capture

## Priority Roadmap

### Phase 1: Foundation (prove the concept)

| Priority | Module | Description | Why First |
|---|---|---|---|
| 1 | **UART decode** | Decode serial data overlaid on waveform | Simplest protocol, proves the overlay concept works |
| 2 | **I2C decode** | Address, R/W, data byte overlay | Most-requested by electronics hobbyists |
| 3 | **SPI decode** | MOSI/MISO/CLK/CS with byte values | Completes the "big three" protocols |

### Phase 2: Automotive (the compelling differentiator)

| Priority | Module | Description | Why |
|---|---|---|---|
| 4 | **CAN bus waveform analysis** | Signal levels, termination quality, dominant/recessive | Useful even without protocol decode |
| 5 | **CAN bus decode** | Message IDs + data bytes overlaid on waveform | The killer feature for auto diagnostics |
| 6 | **OBD-II reader** | Decode standard PIDs to human-readable values | RPM, coolant temp, speed, DTCs |
| 7 | **Relative compression test** | Current clamp starter analysis with cylinder bar graph | No cheap tool does this — huge value |
| 8 | **Injector analysis** | Pulse width, duty cycle, timing diagram | Common diagnostic procedure |

### Phase 3: Advanced Features

| Priority | Module | Description |
|---|---|---|
| 9 | **USB streaming** | Stream samples to PC for sigrok/PulseView |
| 10 | **Long-term logging** | Hours-long captures to flash storage |
| 11 | **Component tester** | Curve tracer using signal gen + scope together |
| 12 | **Bode plot** | Frequency sweep with signal gen, measure response |
| 13 | **Known-good waveform library** | Overlay reference waveforms for comparison |
| 14 | **Guided diagnostic tests** | Step-by-step procedures with expected results |

## Detailed Module Descriptions

### UART Decode

**What it does:** Detects start bits, samples at the baud rate, extracts bytes, displays as hex/ASCII overlay on the waveform.

**Implementation:**
- Auto-detect baud rate from bit timing (measure shortest pulse)
- Detect start bit (falling edge from idle high)
- Sample 8 data bits at baud rate intervals
- Check stop bit
- Display: `0x48 'H'` overlaid at the byte position on the waveform

**Resources needed:** ~2KB code, ~256 bytes buffer. Trivial on this hardware.

### I2C Decode

**What it does:** Detects start/stop conditions on SDA while SCL is high, extracts 7-bit address + R/W + data bytes, shows ACK/NACK.

**Implementation:**
- Needs 2 channels: CH1 = SDA, CH2 = SCL
- Detect START: SDA falls while SCL high
- Detect STOP: SDA rises while SCL high
- Clock data on SCL rising edges
- Display: `[S] 0x50 W [A] 0x12 [A] [P]`

**Resources needed:** ~3KB code, ~512 bytes buffer.

### CAN Bus Decode

**Two approaches available:**

**Approach A: From oscilloscope waveform (no extra hardware)**
- Connect scope probe to CAN-H or CAN-L
- Detect dominant/recessive levels from analog waveform
- Decode bit-by-bit including bit stuffing
- Slower but works with existing hardware
- Also shows analog signal quality (noise, reflections, termination issues)

**Approach B: Using MCU's native CAN controller (needs transceiver)**
- Connect a CAN transceiver (MCP2551, ~$1) to MCU pins
- CAN controller handles all bit timing, error detection, frame parsing
- Much more reliable, can decode at full bus speed
- Needs physical access to MCU GPIO pins (PCB modification)
- Could simultaneously show analog waveform (scope) + decoded frames (CAN controller)

**Display:** `ID: 0x7E8  DLC: 8  Data: 41 0C 1A F8 00 00 00 00`

### Relative Compression Test

**What it does:** Monitors starter motor current with a current clamp during cranking. Each cylinder's compression stroke causes a current spike. By measuring the relative height of each spike, you can identify weak cylinders.

**Implementation:**
- CH1: Current clamp on battery/starter cable
- CH2 (optional): Trigger from ignition signal
- Detect periodic current peaks during cranking
- Count cylinders from number of peaks per revolution
- Calculate relative amplitude of each peak
- Display as bar graph: `CYL1: ████████ 95%  CYL2: ████████ 93%  CYL3: █████ 62%  CYL4: ████████ 91%`

**Why it's valuable:** This test normally requires a $500+ dedicated tool or $2,000+ PicoScope with automotive software. A $70 scope doing this would be remarkable.

### USB Streaming to PC

**What it does:** Adds a mode where the device streams raw ADC samples over USB instead of rendering on the LCD. PC software (sigrok/PulseView/custom) handles display and analysis.

**Implementation:**
- Add USB CDC (virtual serial port) or USB bulk endpoint
- When activated: stop LCD task, start streaming FPGA samples to USB
- PC side: sigrok hardware driver or standalone app
- Bandwidth: USB Full-Speed (~1 MB/s) limits continuous streaming but burst captures work fine

**Why it matters:** Turns the $70 handheld into a PC-connected instrument with unlimited screen real estate, protocol decoders, data export, and scripting.

## Vehicle Definition Files

For automotive modules, vehicle-specific data would be stored as files on the device's filesystem:

```
2:/Vehicles/
├── generic_obd2.json          # Standard OBD-II PIDs (universal)
├── toyota_camry_2020.json     # Toyota-specific CAN IDs
├── honda_civic_2019.json      # Honda-specific CAN IDs
└── ford_f150_2021.json        # Ford-specific CAN IDs
```

The DBC file format (industry standard) could also be supported. Open-source DBC databases exist for many vehicles and would be community-contributed.

## PicoScope Feature Comparison

What PicoScope's $1,000+ automotive software offers that we could replicate:

| PicoScope Feature | Feasibility on 2C53T | Notes |
|---|---|---|
| Guided tests (step-by-step) | High | JSON procedure files on filesystem |
| Known-good waveform library | High | Reference waveforms as data files |
| Automatic pass/fail analysis | Medium | Pattern matching against references |
| Multi-channel correlation | High | Already has 2 channels |
| Serial protocol decode | High | Software-only, plenty of resources |
| Report generation | Medium | Save as BMP/CSV, format on PC |
| Math channels (A-B, FFT) | Medium | FFT already partially exists in firmware |
| Persistence display | Easy | Just don't clear the framebuffer between sweeps |
