#include <Arduino.h>

#include <TinyLink.h>

uint8_t buffer[128];

TinyLink link(Serial, buffer, sizeof(buffer));

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(9600);
}

void loop()
{
    frame_t frame;

    if (Serial.available()) {
        if (link.readFrame(&frame)) {
            // Toggle led based on the flags value.
            digitalWrite(LED_BUILTIN, frame.flags != 0);

            // Echo the frame.
            link.writeFrame(&frame);
        }
    }
}
