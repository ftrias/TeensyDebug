#include "TeensyDebug.h"

void setup() {
    int fd = debug.file_open("/tmp/test.out");
    if (fd < 0) {
        Serial.println("Failed to open file!");
        Serial.println(debug.file_errno());
    }
    else {
        debug.file_write(fd, "abc", 3);
        debug.file_close(fd);
    }
}

void loop() {
}