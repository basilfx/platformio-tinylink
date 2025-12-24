#pragma once

#include <stddef.h>
#include <stdint.h>

#define CRC32_POLYNOMIAL 0xEDB88320L

uint32_t CRC32(uint32_t initial, const uint8_t* buffer, size_t length);
uint32_t CRC32(uint32_t initial, uint8_t value);
uint32_t CRC32(const uint8_t* buffer, size_t length);
uint32_t CRC32(uint8_t value);
