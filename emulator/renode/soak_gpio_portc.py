# -*- coding: utf-8 -*-
# Soak Test GPIO - Port C (Primary fuzzer)
# IronPython 2.7 compatible - Renode PyDev
#
# GPIOC buttons (active-low):
#   pin0=MENU, pin1=AUTO, pin2=SAVE, pin3=CH1, pin4=CH2,
#   pin5=PRM, pin6=SELECT
#
# This is the PRIMARY fuzzer — it generates random button events
# and writes the current state to /tmp/openscope_soak_state.txt
# so port B can read it.

MY_PORT = 'C'

# --- Timing (in IDR reads) ---
# In Renode, emulated time runs ~20-30x slower than wall clock.
# The input task reads GPIO every 20ms and needs 3 consecutive
# reads to register a press (debounce). Keep durations minimal.
PRESS_INTERVAL = 10        # Short gap between presses
HOLD_DURATION = 5          # Just enough for debounce (3+ reads)
RELEASE_DURATION = 5       # Brief cooldown

# --- PRNG ---
LCG_A = 1103515245
LCG_C = 12345
LCG_M = 2147483648

# --- Buttons across both ports ---
ALL_BUTTONS = [
    ('C', 0, "MENU"),  ('C', 1, "AUTO"),  ('C', 2, "SAVE"),
    ('C', 3, "CH1"),   ('C', 4, "CH2"),   ('C', 5, "PRM"),
    ('C', 6, "SELECT"),
    ('B', 0, "UP"),    ('B', 1, "DOWN"),  ('B', 2, "LEFT"),
    ('B', 3, "RIGHT"), ('B', 4, "OK"),
]

# --- GPIO register offsets ---
GPIO_CRL  = 0x00
GPIO_CRH  = 0x04
GPIO_IDR  = 0x08
GPIO_ODR  = 0x0C
GPIO_BOP  = 0x10
GPIO_BC   = 0x14
GPIO_LCKR = 0x18

if request.IsInit:
    regs = {
        GPIO_CRL: [0x44444444],
        GPIO_CRH: [0x44444444],
        GPIO_ODR: [0x00000000],
        GPIO_LCKR: [0x00000000],
    }
    rng = [42]
    countdown = [20]   # Start with short delay for first press
    fz_state = [0]          # 0=idle, 1=pressing, 2=cooldown
    cur_port = ['']
    cur_pin = [-1]
    cur_name = ['']
    presses = [0]

    # Read seed
    try:
        sf = open("/tmp/openscope_soak_seed.txt", "r")
        rng[0] = int(sf.read().strip())
        sf.close()
    except:
        pass

    # Clear log
    try:
        lf = open("/tmp/openscope_soak_buttons.log", "w")
        lf.write("# Soak test button log (seed=" + str(rng[0]) + ")\n")
        lf.close()
    except:
        pass

    # Clear state file
    try:
        sf = open("/tmp/openscope_soak_state.txt", "w")
        sf.write("IDLE\n")
        sf.close()
    except:
        pass

    def rand_next():
        rng[0] = (LCG_A * rng[0] + LCG_C) % LCG_M
        return rng[0]

    def rand_range(lo, hi):
        return lo + (rand_next() % (hi - lo + 1))

    def log_btn(msg):
        try:
            lf = open("/tmp/openscope_soak_buttons.log", "a")
            lf.write(str(presses[0]) + ": " + msg + "\n")
            lf.close()
        except:
            pass

    def write_state():
        try:
            sf = open("/tmp/openscope_soak_state.txt", "w")
            if fz_state[0] == 1:
                sf.write(cur_port[0] + " " + str(cur_pin[0]) + "\n")
            else:
                sf.write("IDLE\n")
            sf.close()
        except:
            pass

elif request.IsRead:
    off = request.Offset

    if off == GPIO_IDR:
        countdown[0] -= 1

        # State machine
        if fz_state[0] == 0:  # Idle
            if countdown[0] <= 0:
                idx = rand_range(0, len(ALL_BUTTONS) - 1)
                port, pin, name = ALL_BUTTONS[idx]
                cur_port[0] = port
                cur_pin[0] = pin
                cur_name[0] = name
                fz_state[0] = 1
                countdown[0] = rand_range(HOLD_DURATION - 5,
                                          HOLD_DURATION + 10)
                presses[0] += 1
                log_btn("PRESS " + name)
                write_state()

        elif fz_state[0] == 1:  # Holding
            if countdown[0] <= 0:
                log_btn("RELEASE " + cur_name[0])
                fz_state[0] = 2
                countdown[0] = rand_range(RELEASE_DURATION - 10,
                                          RELEASE_DURATION + 30)
                write_state()

        elif fz_state[0] == 2:  # Cooldown
            if countdown[0] <= 0:
                cur_port[0] = ''
                cur_pin[0] = -1
                fz_state[0] = 0
                countdown[0] = rand_range(PRESS_INTERVAL - 30,
                                          PRESS_INTERVAL + 60)
                write_state()

        # Build IDR: all high except active button on THIS port
        idr = 0xFFFF
        if fz_state[0] == 1 and cur_port[0] == MY_PORT:
            idr = idr & ~(1 << cur_pin[0])
        request.Value = idr

    elif off == GPIO_CRL:
        request.Value = regs[GPIO_CRL][0]
    elif off == GPIO_CRH:
        request.Value = regs[GPIO_CRH][0]
    elif off == GPIO_ODR:
        request.Value = regs[GPIO_ODR][0]
    elif off == GPIO_LCKR:
        request.Value = regs[GPIO_LCKR][0]
    else:
        request.Value = 0

elif request.IsWrite:
    off = request.Offset
    val = request.Value
    if off == GPIO_CRL:
        regs[GPIO_CRL][0] = val
    elif off == GPIO_CRH:
        regs[GPIO_CRH][0] = val
    elif off == GPIO_ODR:
        regs[GPIO_ODR][0] = val
    elif off == GPIO_BOP:
        regs[GPIO_ODR][0] = (regs[GPIO_ODR][0] | (val & 0xFFFF)) & ~((val >> 16) & 0xFFFF)
    elif off == GPIO_BC:
        regs[GPIO_ODR][0] = regs[GPIO_ODR][0] & ~(val & 0xFFFF)
    elif off == GPIO_LCKR:
        regs[GPIO_LCKR][0] = val
