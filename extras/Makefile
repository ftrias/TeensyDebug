DIST=license.txt library.properties keywords.txt README.md src examples extras

all: ../TeensyDebug.zip

../TeensyDebug.zip: $(DIST)
	cd ..; zip -r TeensyDebug.zip $(DIST)

extras/teensy_debug.exe:
	cd extras; build.bat