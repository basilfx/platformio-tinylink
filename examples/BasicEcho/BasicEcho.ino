#include <Arduino.h>

#include <TinyLink.h>

uint8_t buffer[128];

TinyLink tinylink(Serial, buffer, sizeof(buffer));

void setup()
{
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
#endif

    Serial.begin(9600);
}

void loop()
{
    frame_t frame;

    if (Serial.available()) {
        if (tinylink.readFrame(&frame)) {
            // Toggle led based on the flags value.
#ifdef LED_BUILTIN
            digitalWrite(LED_BUILTIN, frame.flags != 0);
#endif

            // Echo the frame.
            tinylink.writeFrame(&frame);
        }
    }
}
