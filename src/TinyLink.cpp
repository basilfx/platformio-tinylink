#include "TinyLink.h"
#include "Crc.h"

static uint32_t _read_uint32_t(uint8_t* buffer)
{
    uint32_t value = 0;

    value |= static_cast<uint32_t>(buffer[0]) << 0;
    value |= static_cast<uint32_t>(buffer[1]) << 8;
    value |= static_cast<uint32_t>(buffer[2]) << 16;
    value |= static_cast<uint32_t>(buffer[3]) << 24;

    return value;
}

static uint16_t _read_uint16_t(uint8_t* buffer)
{
    uint16_t value = 0;

    value |= static_cast<uint16_t>(buffer[0]) << 0;
    value |= static_cast<uint16_t>(buffer[1]) << 8;

    return value;
}

static void _write_uint16_t(uint8_t* buffer, uint16_t value)
{
    buffer[0] = (value & 0x00FF) >> 0;
    buffer[1] = (value & 0xFF00) >> 8;
}

static uint8_t _read_uint8_t(uint8_t* buffer)
{
    uint8_t value = 0;

    value |= static_cast<uint8_t>(buffer[0]) << 0;

    return value;
}

static void _write_uint8_t(uint8_t* buffer, uint8_t value)
{
    buffer[0] = (value & 0xFF) >> 0;
}

TinyLink::TinyLink(Stream& _stream, uint8_t* _buffer, size_t _length) : stream(_stream), buffer(_buffer)
{
    this->length = _length;

    this->state = WAITING_FOR_PREAMBLE;
    this->index = 0;
    this->unescaping = false;
}

uint8_t TinyLink::checksumHeader(const uint16_t flags, const uint16_t length)
{
    uint8_t a = (flags  & 0x00FF) >> 0;
    uint8_t b = (flags  & 0xFF00) >> 8;
    uint8_t c = (length & 0x00FF) >> 0;
    uint8_t d = (length & 0xFF00) >> 8;

    return a ^ b ^ c ^ d;
}

uint32_t TinyLink::checksumFrame(const uint8_t* header, const uint8_t* payload, const uint16_t length)
{
    return CRC32(CRC32(header, 5), payload, length);
}

void TinyLink::writeStream(bool preamble, const uint8_t* buffer, const uint16_t length)
{
    if (preamble) {
        this->stream.write(buffer, length);
    }
    else {
        for (uint16_t i = 0; i < length; i++) {
            if (buffer[i] == FLAG) {
                this->stream.write(ESCAPE);
                this->stream.write(FLAG);
            }
            else if (buffer[i] == ESCAPE) {
                this->stream.write(ESCAPE);
                this->stream.write(ESCAPE);
            }
            else {
                this->stream.write(buffer[i]);
            }
        }
    }
}

bool TinyLink::writeFrame(const frame_t* frame)
{
    // Do not exceed maximum length.
    if (frame->length > this->length) {
        return false;
    }

    // Send preamble.
    uint32_t preamble = PREAMBLE;

    this->writeStream(true, reinterpret_cast<uint8_t*>(&preamble), 4);

    // Send header.
    uint8_t header[5];
    uint8_t checksumHeader = this->checksumHeader(frame->flags, frame->length);

    _write_uint16_t(&header[0], frame->flags);
    _write_uint16_t(&header[2], frame->length);
    _write_uint8_t(&header[4], checksumHeader);

    this->writeStream(false, reinterpret_cast<uint8_t*>(header), sizeof(header));

    // Send body.
    uint32_t checksumFrame = this->checksumFrame(header, reinterpret_cast<uint8_t*>(frame->payload), frame->length);

    this->writeStream(false, reinterpret_cast<uint8_t*>(frame->payload), frame->length);
    this->writeStream(false, reinterpret_cast<uint8_t*>(&checksumFrame), 4);

    return true;
}

bool TinyLink::write(const uint16_t flags, const void* payload, const uint16_t length)
{
    frame_t frame;

    frame.length = length;
    frame.flags = flags;
    frame.payload = static_cast<uint8_t*>(payload);

    return this->writeFrame(&frame);
}

bool TinyLink::readFrame(frame_t* frame)
{
    uint8_t byte = this->stream.read();

    // Unescape and append to buffer.
    if (this->state == WAITING_FOR_HEADER || this->state == WAITING_FOR_BODY) {
        if (this->unescaping) {
            this->index -= 1;
            this->unescaping = false;
        }
        else if (byte == ESCAPE) {
            this->unescaping = true;
        }
    }

    this->buffer[this->index++] = byte;

    if (this->unescaping) {
        return false;
    }

    // Decide what to do.
    switch (this->state) {
        case WAITING_FOR_PREAMBLE:
        {
            if (this->index >= LEN_PREAMBLE) {
                uint32_t preamble = _read_uint32_t(&this->buffer[this->index - LEN_PREAMBLE]);

                if (preamble == PREAMBLE) {
                    // Preamble found, advance state.
                    this->state = WAITING_FOR_HEADER;
                    this->index = 0;
                } else if (this->index == this->length) {
                    // Preamble not found and buffer is full. Copy last four
                    // bytes, because the next byte may form the preamble
                    // together with the last three bytes.
                    memcpy(this->buffer, &this->buffer[this->index - 4], LEN_PREAMBLE);
                    this->index = LEN_PREAMBLE;
                }
            }

            break;
        }
        case WAITING_FOR_HEADER:
        {
            if (this->index == LEN_HEADER) {
                uint16_t flags = _read_uint16_t(&this->buffer[0]);
                uint16_t length = _read_uint16_t(&this->buffer[2]);
                uint8_t checksumHeader = _read_uint8_t(&this->buffer[4]);

                if (checksumHeader == this->checksumHeader(flags, length) && length <= this->length - LEN_HEADER - LEN_BODY - 1) {
                    this->state = WAITING_FOR_BODY;
                } else {
                    // Reset to start state.
                    this->state = WAITING_FOR_PREAMBLE;
                    this->index = 0;
                }
            }

            break;
        }
        case WAITING_FOR_BODY:
        {
            uint16_t flags = _read_uint16_t(&this->buffer[0]);
            uint16_t length = _read_uint16_t(&this->buffer[2]);

            if (this->index == LEN_HEADER + length + LEN_CRC) {
                uint32_t checksumFrame = _read_uint32_t(&this->buffer[this->index - LEN_CRC]);

                // Reset to start state.
                this->state = WAITING_FOR_PREAMBLE;
                this->index = 0;

                // Copy to frame.
                if (checksumFrame == this->checksumFrame(&this->buffer[0], &this->buffer[5], length)) {
                    frame->flags = flags;
                    frame->length = length;
                    frame->payload = &this->buffer[LEN_HEADER];

                    return true;
                }
            }

            break;
        }
    }

    // No frames processed.
    return false;
}

bool TinyLink::read(void* buffer, const uint16_t length)
{
    frame_t frame;

    if (!this->readFrame(&frame)) {
        return false;
    }

    if (frame.length > length) {
        return false;
    }

    memcpy(buffer, frame.payload, frame.length);

    return true;
}
