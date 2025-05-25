import sys

import serial

filename = sys.argv[1]
s = serial.Serial("/dev/ttyUSB0", timeout=1.0)

with open(filename) as fh:
    for line in fh.readlines():
        # send
        print(repr(line))
        s.write(f"{line}\r\n".encode())
        # print(f"w = {w}")
        # time.sleep(0.01)
        print(repr(s.read(20)))
s.close()
