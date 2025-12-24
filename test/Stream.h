#pragma once

#include <cstdint>
#include <cstring>

/**
 * @brief Mock Stream class for unit testing without Arduino framework dependency.
 *
 * This is a minimal implementation of the Arduino Stream interface that provides the methods needed for TinyLink
 * testing.
 */
class Stream {
public:
    virtual ~Stream() = default;

    /**
     * @brief Write a single byte to the stream.
     * @param b The byte to write.
     * @return The number of bytes written (1 on success).
     */
    virtual size_t write(uint8_t b) = 0;

    /**
     * @brief Write multiple bytes to the stream.
     * @param buffer Pointer to the data to write.
     * @param size The number of bytes to write.
     * @return The number of bytes written.
     */
    virtual size_t write(const uint8_t* buffer, size_t size) {
        size_t written = 0;
        for (size_t i = 0; i < size; i++) {
            written += write(buffer[i]);
        }
        return written;
    }

    /**
     * @brief Get the number of bytes available to read.
     * @return The number of bytes available.
     */
    virtual int available() = 0;

    /**
     * @brief Read a single byte from the stream.
     * @return The byte read, or -1 if no data available.
     */
    virtual int read() = 0;

    /**
     * @brief Peek at the next byte without removing it.
     * @return The byte peeked, or -1 if no data available.
     */
    virtual int peek() = 0;

    /**
     * @brief Flush the stream (wait for transmission to complete).
     */
    virtual void flush() = 0;
};
