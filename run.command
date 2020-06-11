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

#
# Process args in style of teensy_post_compile
#
class args:
  def set(self, k, v):
    self.__dict__[k] = v

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



# parser = argparse.ArgumentParser(description='Teensy GDB tool.')
# parser.add_argument('--dev', help="Device for programming")
# parser.add_argument('--file', help="Device for programming")
# parser.add_argument('--tools', help="Directory with tools")
# parser.add_argument('--path', help="Directory with tools")
# parser.add_argument('--board', help="Directory with tools")
# parser.add_argument('--port', help="Directory with tools")
# parser.add_argument('--portlabel', help="Directory with tools")
# parser.add_argument('--elf', help="Directory with tools")
# args = parser.parse_args()

# print(sys.version)

args = parseCommandLine()

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
try:
  if args.gdb == "0": exit(0)
except:
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

devs = []

# parse the output to get the ports
for line in stdout.decode().splitlines():
  if not re.search('label', line): continue
  if re.search('no_device', line): continue
  a = line.split(': ')
  dev = a[1][1:].split(' ')[0]
  # if it's the programming port, ignore it
  if dev == args.port: continue
  print(dev)
  devs.append(dev)

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
os.system("/usr/bin/open -a Terminal -n %s &" % f.name)
