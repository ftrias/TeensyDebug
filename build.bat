pyinstaller --dist . --onefile teensy_debug.py
rmdir /s /q build
del teensy_debug.spec
