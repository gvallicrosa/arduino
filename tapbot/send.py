from __future__ import annotations

import sys
import time

import serial
from translate import MOTORS_DISABLE

filename = sys.argv[1]
s = serial.Serial("/dev/ttyUSB0", timeout=1.0)

with open(filename) as fh:
    for line in fh.readlines():
        # send
        print(repr(line))
        s.write(f"{line}\r\n".encode())
        print(s.readline())
        if line.startswith("SM,"):
            duration_ms = int(line.split(",")[1])
            if duration_ms > 50:
                time.sleep((duration_ms - 50) / 1000.0)
    time.sleep(10.0)
    s.write(f"{MOTORS_DISABLE}\r\n".encode())

s.close()
