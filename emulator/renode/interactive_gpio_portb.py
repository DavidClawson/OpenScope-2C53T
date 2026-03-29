# -*- coding: utf-8 -*-
# Interactive GPIO - Port B (reads button state from SDL viewer)
# IronPython 2.7 compatible - Renode PyDev
#
# GPIOB buttons (active-low):
#   pin0=UP, pin1=DOWN, pin2=LEFT, pin3=RIGHT, pin4=OK
#
# Reads /tmp/openscope_buttons.txt written by lcd_viewer.
# Format: "IDLE" or "B 1" (port + pin number)

MY_PORT = 'B'
BTN_FILE = "/tmp/openscope_buttons.txt"

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
    read_count = [0]
    cached_port = ['']
    cached_pin = [-1]

elif request.IsRead:
    off = request.Offset

    if off == GPIO_IDR:
        read_count[0] += 1
        if read_count[0] % 3 == 0:
            try:
                sf = open(BTN_FILE, "r")
                line = sf.read().strip()
                sf.close()
                if line == "IDLE":
                    cached_port[0] = ''
                    cached_pin[0] = -1
                else:
                    parts = line.split()
                    if len(parts) >= 2:
                        cached_port[0] = parts[0]
                        cached_pin[0] = int(parts[1])
                    else:
                        cached_port[0] = ''
                        cached_pin[0] = -1
            except:
                cached_port[0] = ''
                cached_pin[0] = -1

        idr = 0xFFFF
        if cached_port[0] == MY_PORT and cached_pin[0] >= 0:
            idr = idr & ~(1 << cached_pin[0])
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
