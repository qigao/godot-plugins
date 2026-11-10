#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include <jpeglib.h>

struct jpeg_recovery {
    struct jpeg_error_mgr errors;
    jmp_buf point;
};

static void jpeg_failed(j_common_ptr decoder) {
    struct jpeg_recovery *recovery = (struct jpeg_recovery *)decoder->err;
    longjmp(recovery->point, 1);
}

static void jpeg_message(j_common_ptr decoder, int level) {
    if (level < 0) {
        jpeg_failed(decoder);
    }
}

static bool decode(struct jpeg_decompress_struct *decoder, struct jpeg_recovery *recovery,
                   const uint8_t *source, size_t size, int width, int height, uint8_t *destination) {
    if (setjmp(recovery->point)) {
        return false;
    }
    jpeg_create_decompress(decoder);
    jpeg_mem_src(decoder, source, (unsigned long)size);
    if (jpeg_read_header(decoder, TRUE) != JPEG_HEADER_OK ||
        decoder->image_width != (JDIMENSION)width || decoder->image_height != (JDIMENSION)height) {
        return false;
    }
    decoder->out_color_space = JCS_EXT_RGBA;
    if (!jpeg_start_decompress(decoder)) {
        return false;
    }
    while (decoder->output_scanline < decoder->output_height) {
        JSAMPROW row = destination + (size_t)decoder->output_scanline * (size_t)width * 4;
        if (jpeg_read_scanlines(decoder, &row, 1) != 1) {
            return false;
        }
    }
    return jpeg_finish_decompress(decoder) != FALSE;
}

bool salts_camera_decode_jpeg(const uint8_t *source, size_t size, int width, int height,
                              uint8_t *destination) {
    struct jpeg_decompress_struct decoder = {0};
    struct jpeg_recovery recovery;
    decoder.err = jpeg_std_error(&recovery.errors);
    recovery.errors.error_exit = jpeg_failed;
    recovery.errors.emit_message = jpeg_message;
    const bool result = decode(&decoder, &recovery, source, size, width, height, destination);
    jpeg_destroy_decompress(&decoder);
    return result;
}
