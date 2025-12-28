#include "Crc.h"

static void CRC32Value(uint32_t &CRC, uint8_t c) {
    uint32_t temp = (CRC >> 8) & 0x00FFFFFFL;
    uint32_t crc = (static_cast<int>(CRC) ^ c) & 0xFF;
    uint8_t length = 8;

    while (length--) {
        if (crc & 1) {
            crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
        }
        else {
            crc >>= 1;
        }
    }

    CRC = temp ^ crc;
}

uint32_t CRC32(uint8_t value) {
    return CRC32(0, &value, 1);
}

uint32_t CRC32(uint32_t initial, uint8_t value) {
    return CRC32(initial, &value, 1);
}

uint32_t CRC32(const uint8_t* buffer, size_t length) {
    return CRC32(0, buffer, length);
}

uint32_t CRC32(uint32_t initial, const uint8_t* buffer, size_t length) {
    uint32_t result = initial;

    while (length--) {
        CRC32Value(result, *buffer++);
    }

    return result;
}
