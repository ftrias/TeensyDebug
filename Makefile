DIST=license.txt TeensyDebug.h TeensyDebug.cpp gdbstub.cpp examples library.properties keywords.txt README.md teensy_debug.exe teensy_debug.py install.sh install.command install.bat

TeensyDebug.zip: $(DIST)
	zip -r TeensyDebug.zip $(DIST)