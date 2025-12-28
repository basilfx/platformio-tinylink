#pragma once

#include <Stream.h>

// This can be anything, and is used to synchronize a frame.
#define PREAMBLE        0xAA55AA55

// The escape character is used for byte-stuffing of the header and body.
#define FLAG            0xAA
#define ESCAPE          0x1B

// Do not change the values below!
#define LEN_PREAMBLE    4

#define LEN_FLAGS       2
#define LEN_LENGTH      2
#define LEN_XOR         1
#define LEN_HEADER      (LEN_FLAGS + LEN_LENGTH + LEN_XOR)

#define LEN_CRC         4
#define LEN_BODY        LEN_CRC

// Protocol states.
typedef enum {
    WAITING_FOR_PREAMBLE = 1,
    WAITING_FOR_HEADER,
    WAITING_FOR_BODY
} tinylink_state_e;

struct frame_t {
    uint16_t length;
    uint16_t flags;
    const uint8_t* payload;
};

class TinyLink {
public:
    /**
     * @brief Construct a new TinyLink object.
     *
     * The size of the buffer should be large enough to hold the header and the
     * body, taking into account that the data in the body is byte-stuffed. That
     * means that, worst case, the byte-stuffed data is twice as large.
     *
     * @param _stream   The stream to use.
     * @param _buffer   The buffer to use.
     * @param _length   The length of the buffer.
     */
    TinyLink(Stream& _stream, uint8_t* _buffer, size_t _length);

    /**
     * @brief Read a frame from the stream.
     *
     * @param frame     The frame to read into.
     * @return true     If a frame was read.
     * @return false    If no frame was read.
     */
    bool readFrame(frame_t* frame);

    /**
     * @brief Read data from the stream directly into a buffer.
     *
     * If the buffer is too small to hold the data, the data will be discarded
     * and false will be returned.
     *
     * Flags are ignored by this method.
     *
     * @param buffer    The buffer to read into.
     * @param length    The length of the buffer.
     * @return true     If data was read.
     * @return false    If no data was read or the buffer too small.
     */
    bool read(void* buffer, const uint16_t length);

    /**
     * @brief Write a frame to the stream.
     *
     * @param frame     The frame to write.
     * @return true     If the frame was written.
     * @return false    If the frame was not written.
     */
    bool writeFrame(const frame_t* frame);

    /**
     * @brief Write data from a buffer to the stream.
     *
     * @param flags     The flags to use.
     * @param payload   The payload to write.
     * @param length    The length of the buffer.
     * @return true     If the data was written.
     * @return false    If the data was not written.
     */
    bool write(const uint16_t flags, const void* payload, const uint16_t length);
private:
    void writeStream(bool preamble, const uint8_t* buffer, const uint16_t length);

    uint8_t checksumHeader(const uint16_t flags, const uint16_t length);
    uint32_t checksumFrame(const uint8_t* header, const uint8_t* payload, const uint16_t length);

    Stream& stream;

    uint16_t length;
    uint8_t* buffer;

    size_t index;
    bool unescaping;

    tinylink_state_e state;
};
