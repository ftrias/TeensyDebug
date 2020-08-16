pyinstaller --dist . --onefile teensy_debug
rmdir /s /q build
del teensy_debug.spec
