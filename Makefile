DIST=license.txt TeensyDebug.h TeensyDebug.cpp gdbstubs.cpp library.properties keywords.txt README.md examples

TeensyDebug.zip: $(DIST)
	zip -r TeensyDebug.zip $(DIST)