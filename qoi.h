#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    if (width == 0 || height == 0 || (channels != 3 && channels != 4) || colorspace > 1 ||
        static_cast<uint64_t>(width) * height > 400000000u) {
        return false;
    }

    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    int run = 0;
    const uint64_t px_num = static_cast<uint64_t>(width) * height;

    uint8_t history[64][4] = {};

    uint8_t r = 0, g = 0, b = 0, a = 255;
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;

    for (uint64_t i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();
        if (!std::cin) return false;

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            ++run;
            if (run == 62 || i + 1 == px_num) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }

            const int index = QoiColorHash(r, g, b, a);
            if (history[index][0] == r && history[index][1] == g &&
                history[index][2] == b && history[index][3] == a) {
                QoiWriteU8(QOI_OP_INDEX_TAG | static_cast<uint8_t>(index));
            } else {
                history[index][0] = r;
                history[index][1] = g;
                history[index][2] = b;
                history[index][3] = a;

                if (a != pre_a) {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                    QoiWriteU8(a);
                } else {
                    const int dr = static_cast<int>(r) - pre_r;
                    const int dg = static_cast<int>(g) - pre_g;
                    const int db = static_cast<int>(b) - pre_b;
                    const int dr_dg = dr - dg;
                    const int db_dg = db - dg;

                    if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 &&
                        db >= -2 && db <= 1) {
                        QoiWriteU8(QOI_OP_DIFF_TAG | static_cast<uint8_t>((dr + 2) << 4) |
                                   static_cast<uint8_t>((dg + 2) << 2) |
                                   static_cast<uint8_t>(db + 2));
                    } else if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 &&
                               db_dg >= -8 && db_dg <= 7) {
                        QoiWriteU8(QOI_OP_LUMA_TAG | static_cast<uint8_t>(dg + 32));
                        QoiWriteU8(static_cast<uint8_t>((dr_dg + 8) << 4) |
                                   static_cast<uint8_t>(db_dg + 8));
                    } else {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    for (size_t i = 0; i < sizeof(QOI_PADDING); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return static_cast<bool>(std::cout);
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (!std::cin || c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    colorspace = QoiReadU8();
    if (!std::cin || width == 0 || height == 0 || (channels != 3 && channels != 4) ||
        colorspace > 1 || static_cast<uint64_t>(width) * height > 400000000u) {
        return false;
    }

    int run = 0;
    const uint64_t px_num = static_cast<uint64_t>(width) * height;

    uint8_t history[64][4] = {};

    uint8_t r = 0, g = 0, b = 0, a = 255;

    for (uint64_t i = 0; i < px_num; ++i) {
        if (run > 0) {
            --run;
        } else {
            const uint8_t tag = QoiReadU8();
            if (!std::cin) return false;

            if (tag == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (tag == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else {
                switch (tag & QOI_MASK_2) {
                    case QOI_OP_INDEX_TAG: {
                        const uint8_t *pixel = history[tag & 0x3f];
                        r = pixel[0];
                        g = pixel[1];
                        b = pixel[2];
                        a = pixel[3];
                        break;
                    }
                    case QOI_OP_DIFF_TAG:
                        r = static_cast<uint8_t>(r + ((tag >> 4) & 0x03) - 2);
                        g = static_cast<uint8_t>(g + ((tag >> 2) & 0x03) - 2);
                        b = static_cast<uint8_t>(b + (tag & 0x03) - 2);
                        break;
                    case QOI_OP_LUMA_TAG: {
                        const uint8_t second = QoiReadU8();
                        const int dg = (tag & 0x3f) - 32;
                        r = static_cast<uint8_t>(r + dg + (second >> 4) - 8);
                        g = static_cast<uint8_t>(g + dg);
                        b = static_cast<uint8_t>(b + dg + (second & 0x0f) - 8);
                        break;
                    }
                    case QOI_OP_RUN_TAG:
                        run = tag & 0x3f;
                        if (i + static_cast<uint64_t>(run) >= px_num) return false;
                        break;
                }
            }
            if (!std::cin) return false;
        }

        const int index = QoiColorHash(r, g, b, a);
        history[index][0] = r;
        history[index][1] = g;
        history[index][2] = b;
        history[index][3] = a;

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    for (size_t i = 0; i < sizeof(QOI_PADDING); ++i) {
        if (QoiReadU8() != QOI_PADDING[i] || !std::cin) return false;
    }

    return static_cast<bool>(std::cout);
}

#endif // QOI_FORMAT_CODEC_QOI_H_
