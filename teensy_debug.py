#!/usr/bin/env python

#
# Program Teensy, wait for it to come back online and start GDB.
# This program will:
#   1. Run teensy_post_compile to upload program.
#   2. Run teensy_ports to figure out what ports to use.
#   3. Run GDB in new window pointing to right port.
#
# Use with "-i" to install the program as the default uploader.
# This will:
#   1. copy this script to the "tools" directory.
#   2. create a boards.local.txt and platform.local.txt file.
#      These files will redirect uploads to this script.
#    

import subprocess
import sys
import os
import os.path
# import argparse
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

def parseCommandLine(x=None):
  ret = args()
  if x is None:
    x = sys.argv[1:]

  for arg in x:
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

def askUser(question):
  print(question)
  print("[y/n] ? ", end='')
  ask = input()
  if ask[0] == 'y' or ask[0] == 'Y':
    return True
  return False

def pauseUser():
    print("Press Enter to close")
    input()

def installGDB():
  global args

  ask = askUser("Do you want to install GDB support for Teensyduino?")

  if not ask:
    return 0

  print("Install GDB in Teensyduino")

  DIR = None

  if args.has("i") and args.i != 1:
    DIR = args.i
  else:
    if os.name == 'nt':
      DIR = "/Program Files (x86)/Arduino/"
      EXT = "exe"
    elif sys.platform == 'darwin':
      APPDIR = "/Applications/"
      EXT = "command"
      if os.path.exists(APPDIR + "Teensyduino.app"):
        DIR = APPDIR + "Teensyduino.app/"
      elif os.path.exists(DIR + "Arduino.app"):
        DIR = APPDIR + "Arduino.app/"    
    else:
      EXT = "py"
      DIR = "~/arduino/"

  if DIR is None:
    print("Teensyduino not found in %s! Try using -i" % DIR)
    return 0

  if os.path.exists(DIR + "hardware"):
    TOOLS = DIR + "hardware/tools/"
    AVR = DIR + "hardware/teensy/avr/"
  elif os.path.exists(DIR + "Contents"):
    TOOLS = DIR + "Contents/Java/hardware/tools/"
    AVR = DIR + "Contents/Java/hardware/teensy/avr/"
  else:
    print("Teensyduino app found, but contents not found in %s! Try using -i" % DIR)
    return 0

  print("Teensyduino found in %s" % DIR)
  print("Teensyduino tools in %s" % TOOLS)

  if os.path.exists(TOOLS + "teensy_debug.py"):
    print("Already installed. Overwriting.")

  # shutil.copy("run.command", TOOLS)
  if EXT == "exe":
    shutil.copy("teensy_debug.exe", TOOLS)
  elif EXT == "command":
    shutil.copy("teensy_debug.py", TOOLS + "teensy_debug.command")
  else:
    shutil.copy("teensy_debug.py", TOOLS)

  createFiles(AVR, EXT)

def createFiles(AVR, EXT):
  with open(AVR + "boards.local.txt", "w+") as f:
    f.write("menu.gdb=GDB\n")
    for ver in ('41','40','32'):
      f.write("""
teensy%s.menu.gdb.serial=Take over Serial
teensy%s.menu.gdb.serial.build.gdb=2
teensy%s.menu.gdb.serial.build.flags.optimize=-Og -g -DGDB_TAKE_OVER_SERIAL
teensy%s.menu.gdb.dual=Use dual Serial
teensy%s.menu.gdb.dual.build.gdb=1
teensy%s.menu.gdb.dual.build.flags.optimize=-Og -g -DGDB_DUAL_SERIAL
teensy%s.menu.gdb.manual=Manual device selection
teensy%s.menu.gdb.manual.build.gdb=3
teensy%s.menu.gdb.manual.build.flags.optimize=-Og -g -DGDB_MANUAL_SELECTION
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
tools.gdbtool.upload.pattern="{cmd.path}/teensy_debug.%s" "-gdb={build.gdb}" "-file={build.project_name}" "-path={build.path}" "-tools={cmd.path}" "-board={build.board}" -reboot "-port={serial.port}" "-portlabel={serial.port.label}" "-portprotocol={serial.port.protocol}"
""" % (EXT))

def getPort():
  global args

  usedev = None

  # wait for Teensy to come online
  for i in range(5):
    out = subprocess.Popen([args.tools + '/teensy_ports', '-L'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout, stderr = out.communicate()
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
  return usedev

def runGDB(arguments):
  global args

  if args.has("i"): 
    installGDB()
    exit(0)

  if args.has("c"): 
    createFiles("./")
    exit(0)

  # gpath=args.tools + "/arm/bin/"
  # GDB="%s/arm-none-eabi-gdb-py" % gpath
  ELF="%s/%s.elf" % (args.path, args.file)

  # build commandline for programming Teensy
  #argline = '" "'.join(arguments)
  post = convertPathSlashes('%s/teensy_post_compile' % args.tools)
  #cmd1='"%s" "%s"' % (post, argline)
  #x = os.system(cmd1)
  x = subprocess.run([post] + arguments)

  # something went wrong with programming
  if x.returncode != 0:
    exit(x)

  # if gdb option is off or missing, don't run gdb so just end here
  if args.has("gdb"):
    if args.gdb == "0": exit(0)
  else:
    exit(0)

  usedev = getPort()

  gpath=args.tools + "/arm/bin/"
  GDB=convertPathSlashes("%s/arm-none-eabi-gdb" % gpath)

  if args.gdb == "3":
    gdbcommand = '"%s" "%s"' % (GDB, ELF)
  else:
    gdbcommand = '"%s" -ex "target remote %s" "%s"' % (GDB, usedev, ELF)

  runCommand(gdbcommand)

def convertPathSlashes(f):
  if os.name == 'nt':
    return f.replace("/", "\\")
  return f

def runOnMac(command):
  f = tempfile.NamedTemporaryFile("w", suffix=".command", delete=False)
  f.write(command)
  f.write("\n")
  f.close()
  # make sure it's executable for Unix-like systems
  os.chmod(f.name, stat.S_IREAD | stat.S_IEXEC)
  # run in separate terminal
  os.system("/usr/bin/open -a Terminal %s &" % f.name)

def runOnLinux(command):
  f = tempfile.NamedTemporaryFile("w", suffix=".sh", delete=False)
  f.write("#!/bin/sh")
  f.write(command)
  f.write("\n")
  f.close()
  # make sure it's executable for Unix-like systems
  os.chmod(f.name, stat.S_IREAD | stat.S_IEXEC)
  # run in separate terminal
  os.system("/usr/bin/xterm -e %s &" % f.name)

def runOnWindows(command):
  f = tempfile.NamedTemporaryFile("w", suffix=".bat", delete=False)
  f.write(command)
  f.write("\n")
  f.close()
  # run in separate terminal
  print("batch file is %s" % f.name)
  os.system("start %s" % f.name)
  # subprocess.run(["cmd.exe", "/k", f.name])

def runCommand(command):
  if os.name == 'nt':
    runOnWindows(command)
  elif sys.platform == 'darwin':
    runOnMac(command)
  else:
    runOnLinux(command)

args = parseCommandLine(sys.argv[1:])
if len(sys.argv)==1:
  try:
    installGDB()
  except Exception as ex:
    print(ex)
  pauseUser()
else:
  runGDB(sys.argv[1:])
