import subprocess
import sys
from pygdbmi.gdbcontroller import GdbController

import pexpect
import time
import sys

GCC="/Applications/Teensyduino.app/Contents/Java/hardware/teensy/../tools/arm/bin/"
DIR="/Applications/Teensyduino.app/Contents/Java/hardware/tools"
ELF="/var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_472274/breakpoint_test.ino.elf"

out = subprocess.Popen([DIR + '/teensy_ports', '-L'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
stdout,stderr = out.communicate()
(usb, dev, desc) = stdout.decode().split(' ',2)
print(dev)

def p(r):
  for t in r:
    line = t['payload']
    if line and isinstance(line, str):
      line = str(line).replace('\\n','\n').replace('\\t','\t')
      sys.stdout.write(line)

gdbmi = GdbController(GCC + "arm-none-eabi-gdb")
print("Start")
response = gdbmi.write('target remote %s' % dev)
p(response)
print("get")
response = gdbmi.write('file %s' % ELF)
p(response)

sys.stdout.write("<done>\n")
for line in sys.stdin:
  command = line.strip()
  r = gdbmi.write(command)
  sys.stdout.write("==========================\n")
  p(r)
  sys.stdout.write("\n-------------------------\n")
