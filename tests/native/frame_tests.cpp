#include "frame_mailbox.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <thread>
#include <tinytest.hpp>
#include <vector>

#ifndef __APPLE__
#include <jpeglib.h>
#endif

using salts_camera::FrameLayout;
using salts_camera::FrameMailbox;
using salts_camera::convert_rgba8;

namespace {

video_native_mode_t mode(int format, int width = 2, int height = 2) {
    return {width, height, 30, 1, format, 1};
}

#ifndef __APPLE__
std::vector<uint8_t> jpeg_frame() {
    jpeg_compress_struct encoder{};
    jpeg_error_mgr errors{};
    encoder.err = jpeg_std_error(&errors);
    jpeg_create_compress(&encoder);
    unsigned char *buffer = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&encoder, &buffer, &size);
    encoder.image_width = 2;
    encoder.image_height = 2;
    encoder.input_components = 3;
    encoder.in_color_space = JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_start_compress(&encoder, TRUE);
    uint8_t row[] = {220, 12, 28, 220, 12, 28};
    JSAMPROW scanline = row;
    for (int y = 0; y < 2; ++y) {
        jpeg_write_scanlines(&encoder, &scanline, 1);
    }
    jpeg_finish_compress(&encoder);
    std::vector<uint8_t> result(buffer, buffer + size);
    jpeg_destroy_compress(&encoder);
    std::free(buffer);
    return result;
}
#endif

} // namespace

suite("Salts Camera frame adapter") {
    it("rejects mismatched dimensions, padded rows and oversized modes") {
        const auto layout = FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_BGRA, 3, 1));
        check_true(layout.has_value());
        check_true(layout->accepts(3, 1, 12));
        check_false(layout->accepts(3, 1, 16));
        check_false(layout->accepts(2, 1, 12));
        check_false(FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_BGRA, 0, 1)).has_value());
        check_false(FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_BGRA, std::numeric_limits<int>::max(),
                                                std::numeric_limits<int>::max())).has_value());
        check_false(FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_I420, 3, 3)).has_value());
    }

    it("converts packed BGRA and the platform's RGB24 channel order") {
        std::array<uint8_t, 4> rgba{};
        const auto bgra = *FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_BGRA, 1, 1));
        const uint8_t bgra_pixel[] = {11, 22, 33, 44};
        check_true(convert_rgba8(bgra, bgra_pixel, sizeof(bgra_pixel), rgba.data()));
        check_eq_container(rgba, (std::array<uint8_t, 4>{33, 22, 11, 44}));

        const auto rgb24 = *FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_RGB24, 1, 1));
#ifdef _WIN32
        const uint8_t rgb_pixel[] = {11, 22, 33};
#else
        const uint8_t rgb_pixel[] = {33, 22, 11};
#endif
        check_true(convert_rgba8(rgb24, rgb_pixel, sizeof(rgb_pixel), rgba.data()));
        check_eq_container(rgba, (std::array<uint8_t, 4>{33, 22, 11, 255}));
    }

#ifndef __APPLE__
    it("converts both planar and interleaved chroma to RGBA") {
        const uint8_t red[] = {82, 82, 82, 82, 90, 240};
        for (int format : {SALTS_VIDEO_CAPTURE_FORMAT_I420, SALTS_VIDEO_CAPTURE_FORMAT_NV12}) {
            const auto layout = *FrameLayout::from_mode(mode(format));
            std::array<uint8_t, 16> rgba{};
            check_true(convert_rgba8(layout, red, sizeof(red), rgba.data()));
            for (size_t offset = 0; offset < rgba.size(); offset += 4) {
                check_greater(rgba[offset], 245);
                check_less(rgba[offset + 1], 8);
                check_less(rgba[offset + 2], 8);
                check_equal(rgba[offset + 3], 255);
            }
        }
    }

    it("decodes MJPEG and rejects malformed, truncated and wrong-sized JPEG frames") {
        const auto jpeg = jpeg_frame();
        const auto layout = *FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_MJPEG));
        std::array<uint8_t, 16> rgba{};
        check_true(convert_rgba8(layout, jpeg.data(), jpeg.size(), rgba.data()));
        check_within(static_cast<int>(rgba[0]), 220, 8);
        check_within(static_cast<int>(rgba[1]), 12, 8);
        check_within(static_cast<int>(rgba[2]), 28, 8);
        check_equal(rgba[3], 255);
        check_false(convert_rgba8(layout, jpeg.data(), jpeg.size() / 2, rgba.data()));
        const uint8_t junk[] = {0xff, 0xd8, 0x00, 0x01, 0xff, 0xd9};
        check_false(convert_rgba8(layout, junk, sizeof(junk), rgba.data()));
        const auto wrong_size = *FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_MJPEG, 1, 4));
        check_false(convert_rgba8(wrong_size, jpeg.data(), jpeg.size(), rgba.data()));
    }
#endif

    it("owns callback bytes and retains only the latest unpublished frame") {
        FrameMailbox mailbox;
        check_true(mailbox.prepare(*FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_BGRA, 1, 1))));
        std::array<uint8_t, 4> source{1, 2, 3, 4};
        mailbox.publish(source.data(), source.size(), 1, 1, 1);
        source[0] = 9;
        mailbox.publish(source.data(), source.size(), 1, 1, 2);
        source[0] = 42;
        const auto *pinned = mailbox.take_latest();
        check_true(pinned != nullptr);
        check_equal(pinned->bytes[0], 9);
        check_equal(pinned->timestamp_us, uint64_t{2});
        check_true(mailbox.take_latest() == nullptr);
        for (uint64_t stamp = 3; stamp < 20; ++stamp) {
            mailbox.publish(source.data(), source.size(), 1, 1, stamp);
        }
        check_equal(pinned->bytes[0], 9);
        check_equal(mailbox.take_latest()->timestamp_us, uint64_t{19});
        check_equal(mailbox.captured(), uint64_t{19});
        check_equal(mailbox.dropped(), uint64_t{17});
        mailbox.discard();
        check_true(mailbox.latest() == nullptr);
        check_equal(mailbox.captured(), uint64_t{19});
    }

    it("keeps pinned frames coherent through concurrent publication and restart") {
        FrameMailbox mailbox;
        const auto layout = *FrameLayout::from_mode(mode(SALTS_VIDEO_CAPTURE_FORMAT_BGRA, 32, 16));
        check_true(mailbox.prepare(layout));
        std::vector<uint8_t> bytes(layout.capacity);
        std::atomic<bool> finished{false};
        constexpr uint64_t frames = 4096;
        std::thread producer([&] {
            for (uint64_t timestamp = 1; timestamp <= frames; ++timestamp) {
                std::fill(bytes.begin(), bytes.end(), static_cast<uint8_t>(timestamp));
                mailbox.publish(bytes.data(), bytes.size(), layout.width, layout.height, timestamp);
            }
            finished.store(true, std::memory_order_release);
        });
        uint64_t consumed = 0;
        uint64_t latest = 0;
        bool coherent = true;
        const auto consume = [&](const FrameMailbox::Frame &frame) {
            coherent &= frame.timestamp_us > latest && frame.size == layout.capacity &&
                        std::all_of(frame.bytes, frame.bytes + frame.size, [&](uint8_t byte) {
                            return byte == static_cast<uint8_t>(frame.timestamp_us);
                        });
            latest = frame.timestamp_us;
            ++consumed;
        };
        do {
            if (const auto *frame = mailbox.take_latest()) {
                consume(*frame);
            }
            std::this_thread::yield();
        } while (!finished.load(std::memory_order_acquire));
        producer.join();
        if (const auto *frame = mailbox.take_latest()) {
            consume(*frame);
        }
        check_true(coherent);
        check_greater(consumed, uint64_t{0});
        check_equal(mailbox.captured(), frames);
        check_equal(consumed + mailbox.dropped(), frames);
        mailbox.discard();
        check_true(mailbox.take_latest() == nullptr);
        mailbox.reset();
        mailbox.publish(bytes.data(), bytes.size() - 1, layout.width, layout.height, 1);
        check_equal(mailbox.dropped(), uint64_t{1});
        mailbox.publish(bytes.data(), bytes.size(), layout.width, layout.height, 2);
        check_equal(mailbox.take_latest()->timestamp_us, uint64_t{2});
    }
}
