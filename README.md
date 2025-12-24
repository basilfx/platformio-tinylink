# TinyLink
A frame-based streaming protocol for embedded applications.

## Introduction
TinyLink is designed to be simple, reliable, and efficient. TinyLink provides
a robust way to send and receive data frames over serial connections with
built-in error detection and byte-stuffing.

## Features
- Frame-based communication protocol
- Built-in error detection using CRC
- Byte-stuffing for reliable data transmission
- Compatible with any Stream-based interface

## Installation

### PlatformIO
1. Open your PlatformIO project
2. Add TinyLink to your `platformio.ini`:
```ini
lib_deps =
    basilfx/platformio-tinylink
```

### Manual Installation
1. Download this repository
2. Copy the contents to your Arduino libraries folder
3. Restart the Arduino IDE

## Usage

### Basic Example
```cpp
#include <TinyLink.h>

// Create a buffer for the frame data
uint8_t buffer[256];
TinyLink tinylink(Serial, buffer, sizeof(buffer));

void setup() {
    Serial.begin(115200);
}

void loop() {
    frame_t frame;
    
    // Read a frame
    if (tinylink.readFrame(&frame)) {
        // Process the frame
        // ...
        
        // Echo the frame back
        tinylink.writeFrame(&frame);
    }
}
```

### Direct Data Transfer
```cpp
// Write data
uint8_t data[] = {1, 2, 3, 4};
tinylink.write(0x0001, data, sizeof(data));

// Read data
uint8_t buffer[256];
if (tinylink.read(buffer, sizeof(buffer))) {
    // Process the received data
}
```

## Protocol Details
TinyLink uses a simple but robust protocol:

1. **Preamble**: 4-byte synchronization pattern (0xAA55AA55)
2. **Header**: Contains flags, length, and checksum
3. **Body**: Contains the actual data and CRC32 checksum
4. **Byte-stuffing**: Special characters are escaped to prevent misinterpretation

For more information, see the [README.md](https://github.com/basilfx/python-tinylink/blob/master/README.md) of
the Python TinyLink implementation.

## Contributing
See the [`CONTRIBUTING.md`](CONTRIBUTING.md) file.

## License
See the [`LICENSE.md`](LICENSE.md) file (MIT license).
