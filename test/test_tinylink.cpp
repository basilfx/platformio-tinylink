#include <Crc.h>
#include <Stream.h>
#include <TinyLink.h>
#include <unity.h>

#include <queue>
#include <vector>

/**
 * @brief Simple stream that captures outgoing bytes and replays queued incoming bytes.
 */
class MockStream : public Stream {
public:
    size_t write(uint8_t b) override {
        written.push_back(b);
        return 1;
    }

    int available() override { return static_cast<int>(incoming.size()); }

    int read() override {
        if (incoming.empty()) {
            return -1;
        }

        uint8_t value = incoming.front();
        incoming.pop();
        return value;
    }

    int peek() override {
        if (incoming.empty()) {
            return -1;
        }

        return incoming.front();
    }

    void flush() override {}

    void feed(const std::vector<uint8_t>& data) {
        for (uint8_t b : data) {
            incoming.push(b);
        }
    }

    std::vector<uint8_t> written;

private:
    std::queue<uint8_t> incoming;
};



void test_constructor(void) {
    uint8_t buffer[256];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    TEST_PASS(); // Constructor worked without issues.
}

void test_write_frame_encodes_and_escapes(void) {
    uint8_t buffer[64];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    const std::vector<uint8_t> payload{0x10, 0xAA, 0x1B};

    frame_t frame;
    frame.length = static_cast<uint16_t>(payload.size());
    frame.flags = 0x1234;
    frame.payload = const_cast<uint8_t*>(payload.data());

    TEST_ASSERT_TRUE(tinylink.writeFrame(&frame));

    // Build the expected frame bytes that should have been written to the stream.
    std::vector<uint8_t> expected;
    const uint32_t preamble = PREAMBLE;
    expected.push_back(static_cast<uint8_t>(preamble >> 0));
    expected.push_back(static_cast<uint8_t>(preamble >> 8));
    expected.push_back(static_cast<uint8_t>(preamble >> 16));
    expected.push_back(static_cast<uint8_t>(preamble >> 24));

    const uint8_t header[] = {0x34, 0x12, 0x03, 0x00, 0x25};
    expected.insert(expected.end(), header, header + sizeof(header));

    expected.push_back(0x10);
    expected.push_back(ESCAPE);
    expected.push_back(FLAG);
    expected.push_back(ESCAPE);
    expected.push_back(ESCAPE);

    const uint32_t crc = CRC32(CRC32(header, sizeof(header)), payload.data(), payload.size());
    expected.push_back(static_cast<uint8_t>(crc >> 0));
    expected.push_back(static_cast<uint8_t>(crc >> 8));
    expected.push_back(static_cast<uint8_t>(crc >> 16));
    expected.push_back(static_cast<uint8_t>(crc >> 24));

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected.size(), stream.written.size(), "Unexpected number of bytes written");
    for (size_t i = 0; i < expected.size(); i++) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], stream.written[i]);
    }
}

void test_read_frame_parses_valid_frame(void) {
    uint8_t buffer[64];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    // Encoded bytes matching the frame used in the write test.
    const std::vector<uint8_t> encoded{
        0x55, 0xAA, 0x55, 0xAA, // Preamble
        0x34, 0x12, 0x03, 0x00, 0x25, // Header
        0x10, 0x1B, 0xAA, 0x1B, 0x1B, // Escaped payload
        0x3D, 0xC3, 0x15, 0x22 // CRC
    };

    stream.feed(encoded);

    frame_t frame;
    bool parsed = false;
    for (size_t i = 0; i < encoded.size(); i++) {
        parsed = tinylink.readFrame(&frame);
        if (parsed) {
            break;
        }
    }

    TEST_ASSERT_TRUE(parsed);
    TEST_ASSERT_EQUAL_UINT16(0x1234, frame.flags);
    TEST_ASSERT_EQUAL_UINT16(3, frame.length);
    TEST_ASSERT_NOT_NULL(frame.payload);
    TEST_ASSERT_EQUAL_UINT8(0x10, frame.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, frame.payload[1]);
    TEST_ASSERT_EQUAL_UINT8(0x1B, frame.payload[2]);
}

void test_read_frame_wraps_buffer_when_full_without_preamble(void) {
    uint8_t buffer[24];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    // Feed 24 bytes of data that does not contain the full preamble.
    // The last two bytes will be 0x55, 0xAA, which is half of the preamble.
    const std::vector<uint8_t> junk{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // Junk data
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, // Junk data
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, // Junk data
        0x55, 0xAA // First half of the preamble
    };
    stream.feed(junk);

    // Read all junk bytes. At this point the buffer is full and should wrap,
    // copying the last 4 bytes to the beginning.
    for (size_t i = 0; i < junk.size(); i++) {
        frame_t frame;
        bool parsed = tinylink.readFrame(&frame);
        TEST_ASSERT_FALSE(parsed);
    }

    // Feed the rest of a valid frame, which starts with the other half of
    // the preamble. The buffer should correctly wrap and parse the frame.
    const std::vector<uint8_t> validFrame{
        0x55, 0xAA, // Other half of the preamble
        0x34, 0x12, 0x03, 0x00, 0x25, // Header
        0x10, 0x1B, 0xAA, 0x1B, 0x1B, // Escaped payload
        0x3D, 0xC3, 0x15, 0x22 // CRC
    };
    stream.feed(validFrame);

    frame_t frame;
    bool parsed = false;
    for (size_t i = 0; i < validFrame.size(); i++) {
        parsed = tinylink.readFrame(&frame);
        if (parsed) {
            break;
        }
    }

    TEST_ASSERT_TRUE(parsed);
    TEST_ASSERT_EQUAL_UINT16(0x1234, frame.flags);
    TEST_ASSERT_EQUAL_UINT16(3, frame.length);
}

void test_read_frame_rejects_invalid_header_checksum(void) {
    uint8_t buffer[64];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    // Encoded bytes with an invalid header checksum.
    // The correct checksum should be 0x25, but we use 0xFF instead.
    const std::vector<uint8_t> encoded{
        0x55, 0xAA, 0x55, 0xAA, // Preamble
        0x34, 0x12, 0x03, 0x00, 0xFF, // Header with invalid checksum
        0x10, 0x1B, 0xAA, 0x1B, 0x1B, // Escaped payload
        0x3D, 0xC3, 0x15, 0x22 // CRC
    };

    stream.feed(encoded);

    frame_t frame;
    bool parsed = false;
    for (size_t i = 0; i < encoded.size(); i++) {
        parsed = tinylink.readFrame(&frame);
        if (parsed) {
            break;
        }
    }

    TEST_ASSERT_FALSE(parsed);
}

void test_write_frame_rejects_too_large(void) {
    uint8_t buffer[8];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    uint8_t payload[10] = {0};

    frame_t frame;
    frame.length = sizeof(payload);
    frame.flags = 0;
    frame.payload = payload;

    TEST_ASSERT_FALSE(tinylink.writeFrame(&frame));
}

void test_convenience_write(void) {
    uint8_t buffer[64];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    const std::vector<uint8_t> payload{0x10, 0xAA, 0x1B};
    const uint16_t flags = 0x1234;

    TEST_ASSERT_TRUE(tinylink.write(flags, const_cast<uint8_t*>(payload.data()), static_cast<uint16_t>(payload.size())));

    // Build the expected frame bytes that should have been written to the stream.
    std::vector<uint8_t> expected;
    const uint32_t preamble = PREAMBLE;
    expected.push_back(static_cast<uint8_t>(preamble >> 0));
    expected.push_back(static_cast<uint8_t>(preamble >> 8));
    expected.push_back(static_cast<uint8_t>(preamble >> 16));
    expected.push_back(static_cast<uint8_t>(preamble >> 24));

    const uint8_t header[] = {0x34, 0x12, 0x03, 0x00, 0x25};
    expected.insert(expected.end(), header, header + sizeof(header));

    expected.push_back(0x10);
    expected.push_back(ESCAPE);
    expected.push_back(FLAG);
    expected.push_back(ESCAPE);
    expected.push_back(ESCAPE);

    const uint32_t crc = CRC32(CRC32(header, sizeof(header)), payload.data(), payload.size());
    expected.push_back(static_cast<uint8_t>(crc >> 0));
    expected.push_back(static_cast<uint8_t>(crc >> 8));
    expected.push_back(static_cast<uint8_t>(crc >> 16));
    expected.push_back(static_cast<uint8_t>(crc >> 24));

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected.size(), stream.written.size(), "Unexpected number of bytes written");
    for (size_t i = 0; i < expected.size(); i++) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], stream.written[i]);
    }
}

void test_convenience_read(void) {
    uint8_t buffer[64];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    // Encoded bytes matching the frame used in the write test.
    const std::vector<uint8_t> encoded{
        0x55, 0xAA, 0x55, 0xAA, // Preamble
        0x34, 0x12, 0x03, 0x00, 0x25, // Header
        0x10, 0x1B, 0xAA, 0x1B, 0x1B, // Escaped payload
        0x3D, 0xC3, 0x15, 0x22 // CRC
    };

    stream.feed(encoded);

    uint8_t received[3];
    bool parsed = false;
    for (size_t i = 0; i < encoded.size(); i++) {
        parsed = tinylink.read(received, sizeof(received));
        if (parsed) {
            break;
        }
    }

    TEST_ASSERT_TRUE(parsed);
    TEST_ASSERT_EQUAL_UINT8(0x10, received[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, received[1]);
    TEST_ASSERT_EQUAL_UINT8(0x1B, received[2]);
}

void test_convenience_read_rejects_too_large(void) {
    uint8_t buffer[64];
    MockStream stream;
    TinyLink tinylink(stream, buffer, sizeof(buffer));

    // Encoded frame with three bytes of payload.
    const std::vector<uint8_t> encoded{
        0x55, 0xAA, 0x55, 0xAA, // Preamble
        0x34, 0x12, 0x03, 0x00, 0x25, // Header
        0x10, 0x1B, 0xAA, 0x1B, 0x1B, // Escaped payload
        0x3D, 0xC3, 0x15, 0x22 // CRC
    };

    stream.feed(encoded);

    // Try to read into a buffer that is too small (only two bytes).
    uint8_t received[2];
    bool parsed = false;
    for (size_t i = 0; i < encoded.size(); i++) {
        parsed = tinylink.read(received, sizeof(received));
        if (parsed) {
            break;
        }
    }

    TEST_ASSERT_FALSE(parsed);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_constructor);
    RUN_TEST(test_write_frame_encodes_and_escapes);
    RUN_TEST(test_read_frame_parses_valid_frame);
    RUN_TEST(test_read_frame_wraps_buffer_when_full_without_preamble);
    RUN_TEST(test_read_frame_rejects_invalid_header_checksum);
    RUN_TEST(test_write_frame_rejects_too_large);
    RUN_TEST(test_convenience_write);
    RUN_TEST(test_convenience_read);
    RUN_TEST(test_convenience_read_rejects_too_large);

    return UNITY_END();
}
