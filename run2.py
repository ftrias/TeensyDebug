import subprocess
from threading  import Thread
import time

GCC="/Applications/Teensyduino.app/Contents/Java/hardware/teensy/../tools/arm/bin/"
DIR="/Applications/Teensyduino.app/Contents/Java/hardware/tools"
ELF="/var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_472274/breakpoint_test.ino.elf"

out = subprocess.Popen([DIR + '/teensy_ports', '-L'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
stdout,stderr = out.communicate()
(usb, dev, desc) = stdout.decode().split(' ',2)
print(dev)

def enqueue_output(out):
  for line in iter(out.readline, b''):
    print("GDB:", line)
  out.close()

p = subprocess.Popen([GCC + '/arm-none-eabi-gdb', ELF], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

t = Thread(target=enqueue_output, args=(p.stdout,))
t.start()

time.sleep(3)
p.stdin.write(b"test command\r\n")
p.stdin.write(b"target /dev/xyz\r\n")
