#!/usr/bin/env python

#
# Program Teensy, wait for it to come back online and start GDB
#

import subprocess
import sys
import os
import argparse
import time
import sys
import re
import tempfile
import stat
import shutil

#
# Process args in style of teensy_post_compile
#
class args:
  def set(self, k, v):
    self.__dict__[k] = v
  def has(self, k):
    return k in self.__dict__

def parseCommandLine():
  ret = args()
  for arg in sys.argv[1:]:
    a = arg.split('=')
    name = a[0]
    if name[0] == '-':
      name = name[1:]
    else:
      print("Invalid parameter", name)
      continue
    if len(a) > 1:
      ret.set(name, a[1])
    else:
      ret.set(name, 1)
  return ret


def installGDB():
  print("Install GDB in Teensyduino")

  DIR = "/Applications/"
  HERE = None
  if os.path.exists(DIR + "Teensyduino.app"):
    HERE = DIR + "Teensyduino.app/"
  elif os.path.exists(DIR + "Arduino.app"):
    HERE = DIR + "Arduino.app/"
  else:
    print("Teensyduino not found!")
    return 0

  TOOLS = HERE + "Contents/Java/hardware/tools/"
  AVR = HERE + "Contents/Java/hardware/teensy/avr/"

  if os.path.exists(TOOLS + "run.command"):
    print("Already installed. Overwriting.")

  shutil.copy("run.command", TOOLS)

  with open(AVR + "boards.local.txt", "w+") as f:
    f.write("menu.gdb=GDB\n")
    for ver in ('41','40','32'):
      f.write("""
teensy%s.menu.gdb.serial=Take over Serial
teensy%s.menu.gdb.serial.build.gdb=2
teensy%s.menu.gdb.serial.build.flags.optimize=-O0 -g -DGDB_TAKE_OVER_SERIAL
teensy%s.menu.gdb.dual=Use dual Serial
teensy%s.menu.gdb.dual.build.gdb=1
teensy%s.menu.gdb.dual.build.flags.optimize=-O0 -g -DGDB_DUAL_SERIAL
teensy%s.menu.gdb.manual=Manual device selection
teensy%s.menu.gdb.manual.build.gdb=3
teensy%s.menu.gdb.manual.build.flags.optimize=-O0 -g -DGDB_MANUAL_SELECTION
teensy%s.menu.gdb.off=Off
teensy%s.menu.gdb.off.build.gdb=0
teensy%s.upload.tool=gdbtool
""" % (ver,ver,ver,ver,ver,ver,ver,ver,ver,ver,ver,ver))

  with open(AVR + "platform.local.txt", "w+") as f:
    f.write("""
## Debugger
tools.gdbtool.cmd.path={runtime.hardware.path}/../tools
tools.gdbtool.upload.params.quiet=
tools.gdbtool.upload.params.verbose=-verbose
tools.gdbtool.upload.pattern="{cmd.path}/run.command" "-gdb={build.gdb}" "-file={build.project_name}" "-path={build.path}" "-tools={cmd.path}" "-board={build.board}" -reboot "-port={serial.port}" "-portlabel={serial.port.label}" "-portprotocol={serial.port.protocol}"
""")

args = parseCommandLine()

if args.has("i"): 
  installGDB()
  exit(0)

gpath=args.tools + "/arm/bin/"
GDB="%s/arm-none-eabi-gdb" % gpath
ELF="%s/%s.elf" % (args.path, args.file)

# /Applications/Teensyduino.app/Contents/Java/hardware/teensy/../tools/run.command -file=gdb2.ino -path=/var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_189659 -tools=/Applications/Teensyduino.app/Contents/Java/hardware/teensy/../tools -board=TEENSY40 -reboot -port=/dev/cu.usbmodem61684901 -portlabel=/dev/cu.usbmodem61684901 -portprotocol=serial 

# cmd1="'%s/teensy_post_compile' -file=sx.ino '-path=%s' '-tools=%s' '-board=%s' -reboot '-port=%s' '-portlabel=%s'" % (args.tools, args.path, args.tools, args.board, args.port, args.portlabel)

# build commandline for programming Teensy
argline = "' '".join(sys.argv[1:])
cmd1="%s/teensy_post_compile '%s'" % (args.tools, argline)
x = os.system(cmd1)

# something went wrong with programming
if x != 0:
  exit(x)

# if gdb option is off or missing, don't run gdb so just end here
if args.has("gdb"):
  if args.gdb == "0": exit(0)
else:
  exit(0)

usedev = None

# wait for Teensy to come online
for i in range(5):
  out = subprocess.Popen([args.tools + '/teensy_ports', '-L'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  stdout,stderr = out.communicate()
  if len(stdout) > 10: break
  time.sleep(1)

# counldn't find it within timeout?
if len(stdout) < 10:
  print("Could not find Teensy")
  exit(1)

# Get the port status and find the right port to connect with. The right port
# is any port that we are not using for programming and serial monitor. Probably
# should be smarter
out = subprocess.Popen([args.tools + '/teensy_ports'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
time.sleep(2)
out.kill()
stdout,stderr = out.communicate()

if args.gdb == "3":

  gdbcommand = "'%s' '%s'" % (GDB, ELF)

else:

  devs = []
  # parse the output to get the ports
  for line in stdout.decode().splitlines():
    if not re.search('label', line): continue
    if re.search('no_device', line): continue
    if not re.search('Serial', line): continue
    a = line.split(': ')
    dev = a[1][1:].split(' ')[0]
    # if using dual serials and it's the programming port, ignore it
    if args.gdb == "1" and dev == args.port: continue
    print(dev)
    devs.append(dev)

  if len(devs) <= 0:
    print("Could not find a extra Serial device. Did you compile one?")
    exit(1)

  # get the last port found
  usedev = sorted(devs)[-1]

  # create gdb command and store in batch file
  gdbcommand = "'%s' -ex 'target remote %s' '%s'" % (GDB, usedev, ELF)

f = tempfile.NamedTemporaryFile("w", suffix=".command", delete=False)
f.write(gdbcommand)
f.write("\n")
f.close()

# make sure it's executable for Unix-like systems
os.chmod(f.name, stat.S_IREAD | stat.S_IEXEC)

#print("OPEN", f.name)

# run in separate terminal
os.system("/usr/bin/open -a Terminal %s &" % f.name)
