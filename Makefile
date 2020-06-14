DIST=license.txt TeensyDebug.h TeensyDebug.cpp gdbstub.cpp examples library.properties keywords.txt README.md teensy_debug.exe teensy_debug.py install-linux.sh install-mac.command install-windows.bat

all: TeensyDebug.zip

TeensyDebug.zip: $(DIST)
	zip -r TeensyDebug.zip $(DIST)