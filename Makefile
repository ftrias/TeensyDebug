DIST=license.txt TeensyDebug.h TeensyDebug.cpp gdbstub.cpp library.properties keywords.txt README.md examples

TeensyDebug.zip: $(DIST)
	zip -r TeensyDebug.zip $(DIST)