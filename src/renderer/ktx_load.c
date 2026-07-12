#include "ktx_load.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * KTX1 format values
 */
#define KTX1_GL_RGBA8                           0x8058
#define KTX1_GL_SRGB8_ALPHA8                    0x8C43
#define KTX1_GL_COMPRESSED_RGB_S3TC_DXT1_EXT    0x83F0
#define KTX1_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT   0x83F1
#define KTX1_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT   0x83F2
#define KTX1_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT   0x83F3
#define KTX1_GL_COMPRESSED_RED_GREEN_RGTC2_EXT  0x8DBD
#define KTX1_GL_COMPRESSED_RGBA_BPTC_UNORM      0x8E8C
#define KTX1_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D

/*
 * KTX2 format values (VkFormat)
 */
#define KTX2_VK_FORMAT_R8G8B8A8_UNORM           37
#define KTX2_VK_FORMAT_R8G8B8A8_SRGB            43
#define KTX2_VK_FORMAT_BC1_RGB_UNORM_BLOCK      131
#define KTX2_VK_FORMAT_BC1_RGB_SRGB_BLOCK       132
#define KTX2_VK_FORMAT_BC1_RGBA_UNORM_BLOCK     133
#define KTX2_VK_FORMAT_BC1_RGBA_SRGB_BLOCK      134
#define KTX2_VK_FORMAT_BC3_UNORM_BLOCK          137
#define KTX2_VK_FORMAT_BC3_SRGB_BLOCK           138
#define KTX2_VK_FORMAT_BC5_UNORM_BLOCK          141
#define KTX2_VK_FORMAT_BC7_UNORM_BLOCK          145
#define KTX2_VK_FORMAT_BC7_SRGB_BLOCK           146

static const uint8_t ktx1_identifier[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static const uint8_t ktx2_identifier[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static uint32_t swap32(uint32_t v) {
    return ((v & 0xFF000000) >> 24) |
           ((v & 0x00FF0000) >>  8) |
           ((v & 0x0000FF00) <<  8) |
           ((v & 0x000000FF) << 24);
}

static uint64_t swap64(uint64_t v) {
    return ((v & 0xFF00000000000000ULL) >> 56) |
           ((v & 0x00FF000000000000ULL) >> 40) |
           ((v & 0x0000FF0000000000ULL) >> 24) |
           ((v & 0x000000FF00000000ULL) >>  8) |
           ((v & 0x00000000FF000000ULL) <<  8) |
           ((v & 0x0000000000FF0000ULL) << 24) |
           ((v & 0x000000000000FF00ULL) << 40) |
           ((v & 0x00000000000000FFULL) << 56);
}

static int is_ktx1(const uint8_t *id) {
    return memcmp(id, ktx1_identifier, 12) == 0;
}

static int is_ktx2(const uint8_t *id) {
    return memcmp(id, ktx2_identifier, 12) == 0;
}

static void detect_format_ktx1(uint32_t glInternalFormat, ktx_format_t *fmt, int *srgb) {
    *srgb = 0;
    switch (glInternalFormat) {
        case KTX1_GL_RGBA8:
            *fmt = KTX_FMT_RGBA8;
            break;
        case KTX1_GL_SRGB8_ALPHA8:
            *fmt = KTX_FMT_RGBA8;
            *srgb = 1;
            break;
        case KTX1_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case KTX1_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            *fmt = KTX_FMT_BC1;
            break;
        case KTX1_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case KTX1_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            *fmt = KTX_FMT_BC3;
            break;
        case KTX1_GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
            *fmt = KTX_FMT_BC5;
            break;
        case KTX1_GL_COMPRESSED_RGBA_BPTC_UNORM:
            *fmt = KTX_FMT_BC7;
            break;
        case KTX1_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
            *fmt = KTX_FMT_BC7;
            *srgb = 1;
            break;
        default:
            *fmt = KTX_FMT_UNKNOWN;
            break;
    }
}

static void detect_format_ktx2(uint32_t vkFormat, ktx_format_t *fmt, int *srgb) {
    *srgb = 0;
    switch (vkFormat) {
        case KTX2_VK_FORMAT_R8G8B8A8_UNORM:
            *fmt = KTX_FMT_RGBA8;
            break;
        case KTX2_VK_FORMAT_R8G8B8A8_SRGB:
            *fmt = KTX_FMT_RGBA8;
            *srgb = 1;
            break;
        case KTX2_VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case KTX2_VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            *fmt = KTX_FMT_BC1;
            break;
        case KTX2_VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case KTX2_VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            *fmt = KTX_FMT_BC1;
            *srgb = 1;
            break;
        case KTX2_VK_FORMAT_BC3_UNORM_BLOCK:
            *fmt = KTX_FMT_BC3;
            break;
        case KTX2_VK_FORMAT_BC3_SRGB_BLOCK:
            *fmt = KTX_FMT_BC3;
            *srgb = 1;
            break;
        case KTX2_VK_FORMAT_BC5_UNORM_BLOCK:
            *fmt = KTX_FMT_BC5;
            break;
        case KTX2_VK_FORMAT_BC7_UNORM_BLOCK:
            *fmt = KTX_FMT_BC7;
            break;
        case KTX2_VK_FORMAT_BC7_SRGB_BLOCK:
            *fmt = KTX_FMT_BC7;
            *srgb = 1;
            break;
        default:
            *fmt = KTX_FMT_UNKNOWN;
            break;
    }
}

static int block_size_for_format(ktx_format_t fmt) {
    switch (fmt) {
        case KTX_FMT_BC1: return 8;
        case KTX_FMT_BC3:
        case KTX_FMT_BC5:
        case KTX_FMT_BC7: return 16;
        default: return 0;
    }
}

static int is_compressed(ktx_format_t fmt) {
    return block_size_for_format(fmt) != 0;
}

static size_t level_size(ktx_format_t fmt, uint32_t w, uint32_t h) {
    if (is_compressed(fmt)) {
        uint32_t bw = (w + 3) / 4;
        uint32_t bh = (h + 3) / 4;
        return (size_t)bw * (size_t)bh * (size_t)block_size_for_format(fmt);
    } else {
        return (size_t)w * (size_t)h * 4;
    }
}

static size_t align4(size_t n) {
    return (n + 3) & ~3;
}

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    uint8_t *data = NULL;
    size_t size;

    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) goto fail;
    size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) goto fail;

    data = (uint8_t *)malloc(size);
    if (!data) goto fail;
    if (fread(data, 1, size, f) != size) {
        free(data);
        data = NULL;
    }

fail:
    fclose(f);
    *out_size = data ? size : 0;
    return data;
}

static int load_ktx1(const uint8_t *data, size_t size, ktx_texture_t *out) {
    uint32_t endian, glType, glTypeSize, glFormat, glInternalFormat;
    uint32_t pixelWidth, pixelHeight, pixelDepth;
    uint32_t numberOfArrayElements, numberOfFaces, numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
    int need_swap;
    size_t pos;
    uint32_t i;

    endian = *(const uint32_t *)(data + 12);
    need_swap = (endian == 0x01020304);

#define READ32(offset) (need_swap ? swap32(*(const uint32_t *)(data + (offset))) : *(const uint32_t *)(data + (offset)))

    glType = READ32(16);
    glTypeSize = READ32(20);
    glFormat = READ32(24);
    glInternalFormat = READ32(28);
    /* glBaseInternalFormat at 32 */
    pixelWidth = READ32(36);
    pixelHeight = READ32(40);
    pixelDepth = READ32(44);
    numberOfArrayElements = READ32(48);
    numberOfFaces = READ32(52);
    numberOfMipmapLevels = READ32(56);
    bytesOfKeyValueData = READ32(60);

    if (pixelWidth == 0 || pixelDepth > 1 || numberOfArrayElements > 1 || numberOfFaces > 1) {
        return -1; /* unsupported for now */
    }

    memset(out, 0, sizeof(*out));
    out->width = pixelWidth;
    out->height = pixelHeight ? pixelHeight : 1;
    out->depth = 1;
    out->arrayLayers = 1;
    out->faceCount = 1;
    out->mipLevels = numberOfMipmapLevels ? numberOfMipmapLevels : 1;

    detect_format_ktx1(glInternalFormat, &out->format, &out->srgb);
    if (out->format == KTX_FMT_UNKNOWN) {
        return -1;
    }

    pos = 64 + bytesOfKeyValueData;

    out->levelOffsets = (size_t *)calloc(out->mipLevels, sizeof(size_t));
    out->levelSizes = (size_t *)calloc(out->mipLevels, sizeof(size_t));
    if (!out->levelOffsets || !out->levelSizes) return -1;

    /* First pass: compute total size */
    for (i = 0; i < out->mipLevels; i++) {
        uint32_t lw = out->width >> i;
        uint32_t lh = out->height >> i;
        if (lw < 1) lw = 1;
        if (lh < 1) lh = 1;
        out->levelSizes[i] = level_size(out->format, lw, lh);
        out->dataSize += out->levelSizes[i];
    }

    out->data = (uint8_t *)malloc(out->dataSize);
    if (!out->data) return -1;

    /* Second pass: read mip data */
    for (i = 0; i < out->mipLevels; i++) {
        uint32_t imageSize;
        size_t paddedSize;

        if (pos + 4 > size) return -1;
        imageSize = *(const uint32_t *)(data + pos);
        if (need_swap) imageSize = swap32(imageSize);
        pos += 4;

        {
            size_t current = 0;
            uint32_t j;
            for (j = 0; j < i; j++) current += out->levelSizes[j];
            out->levelOffsets[i] = current;
        }

        if (pos + imageSize > size) return -1;
        memcpy(out->data + out->levelOffsets[i], data + pos, out->levelSizes[i]);
        pos += imageSize;

        paddedSize = align4(imageSize);
        if (paddedSize > imageSize) {
            pos += paddedSize - imageSize;
        }
    }

    return 0;
}

static int load_ktx2(const uint8_t *data, size_t size, ktx_texture_t *out) {
    uint32_t vkFormat;
    uint32_t typeSize;
    uint32_t pixelWidth, pixelHeight, pixelDepth;
    uint32_t layerCount, faceCount, levelCount;
    uint32_t supercompressionScheme;
    uint32_t dfdByteOffset, dfdByteLength;
    uint32_t kvdByteOffset, kvdByteLength;
    uint64_t sgdByteOffset, sgdByteLength;
    size_t levelIndexOffset;
    uint32_t i;
    int need_swap = 0;

    /* KTX2 header after 12-byte identifier */
    /* Byte layout (little-endian by spec): */
    vkFormat = *(const uint32_t *)(data + 12);
    typeSize = *(const uint32_t *)(data + 16);
    pixelWidth = *(const uint32_t *)(data + 20);
    pixelHeight = *(const uint32_t *)(data + 24);
    pixelDepth = *(const uint32_t *)(data + 28);
    layerCount = *(const uint32_t *)(data + 32);
    faceCount = *(const uint32_t *)(data + 36);
    levelCount = *(const uint32_t *)(data + 40);
    supercompressionScheme = *(const uint32_t *)(data + 44);

    dfdByteOffset = *(const uint32_t *)(data + 48);
    dfdByteLength = *(const uint32_t *)(data + 52);
    kvdByteOffset = *(const uint32_t *)(data + 56);
    kvdByteLength = *(const uint32_t *)(data + 60);
    sgdByteOffset = *(const uint64_t *)(data + 64);
    sgdByteLength = *(const uint64_t *)(data + 72);

    (void)typeSize;
    (void)dfdByteOffset;
    (void)dfdByteLength;
    (void)kvdByteOffset;
    (void)kvdByteLength;
    (void)sgdByteOffset;
    (void)sgdByteLength;

    if (need_swap) {
        vkFormat = swap32(vkFormat);
        pixelWidth = swap32(pixelWidth);
        pixelHeight = swap32(pixelHeight);
        pixelDepth = swap32(pixelDepth);
        layerCount = swap32(layerCount);
        faceCount = swap32(faceCount);
        levelCount = swap32(levelCount);
        supercompressionScheme = swap32(supercompressionScheme);
    }

    if (pixelWidth == 0 || pixelDepth > 1 || layerCount > 1 || faceCount > 1) {
        return -1;
    }
    if (supercompressionScheme != 0) {
        return -1; /* no supercompression support yet */
    }

    memset(out, 0, sizeof(*out));
    out->width = pixelWidth;
    out->height = pixelHeight ? pixelHeight : 1;
    out->depth = 1;
    out->arrayLayers = 1;
    out->faceCount = 1;
    out->mipLevels = levelCount ? levelCount : 1;

    detect_format_ktx2(vkFormat, &out->format, &out->srgb);
    if (out->format == KTX_FMT_UNKNOWN) {
        return -1;
    }

    {
        size_t *fileOffsets = (size_t *)calloc(out->mipLevels, sizeof(size_t));
        out->levelSizes = (size_t *)calloc(out->mipLevels, sizeof(size_t));
        out->levelOffsets = (size_t *)calloc(out->mipLevels, sizeof(size_t));
        if (!fileOffsets || !out->levelSizes || !out->levelOffsets) {
            free(fileOffsets);
            free(out->levelSizes);
            free(out->levelOffsets);
            return -1;
        }

        levelIndexOffset = 80; /* after header */

        for (i = 0; i < out->mipLevels; i++) {
            uint64_t byteOffset, byteLength, uncompressedByteLength;
            const uint8_t *p = data + levelIndexOffset + i * 24; /* level index: level 0 first */
            uint32_t lw = out->width >> i;
            uint32_t lh = out->height >> i;
            if (lw < 1) lw = 1;
            if (lh < 1) lh = 1;

            byteOffset = *(const uint64_t *)(p + 0);
            byteLength = *(const uint64_t *)(p + 8);
            uncompressedByteLength = *(const uint64_t *)(p + 16);

            if (need_swap) {
                byteOffset = swap64(byteOffset);
                byteLength = swap64(byteLength);
            }

            fileOffsets[i] = (size_t)byteOffset;
            out->levelSizes[i] = (size_t)byteLength;
            out->dataSize += out->levelSizes[i];

            (void)uncompressedByteLength;
            (void)lw;
            (void)lh;
        }

        out->data = (uint8_t *)malloc(out->dataSize);
        if (!out->data) {
            free(fileOffsets);
            free(out->levelSizes);
            free(out->levelOffsets);
            return -1;
        }

        {
            size_t current = 0;
            for (i = 0; i < out->mipLevels; i++) {
                out->levelOffsets[i] = current;
                if (fileOffsets[i] + out->levelSizes[i] > size) {
                    free(fileOffsets);
                    return -1;
                }
                memcpy(out->data + current, data + fileOffsets[i], out->levelSizes[i]);
                current += out->levelSizes[i];
            }
        }

        free(fileOffsets);
    }

    return 0;
}

int KTX_LoadFromMemory(const uint8_t *data, size_t size, ktx_texture_t *out) {
    if (!data || size < 12) {
        return -1;
    }

    if (is_ktx1(data)) {
        return load_ktx1(data, size, out);
    } else if (is_ktx2(data)) {
        return load_ktx2(data, size, out);
    }

    return -1;
}

int KTX_LoadFromFile(const char *path, ktx_texture_t *out) {
    uint8_t *data;
    size_t size;
    int ret;

    data = read_file(path, &size);
    if (!data || size < 12) {
        free(data);
        return -1;
    }

    ret = KTX_LoadFromMemory(data, size, out);
    free(data);
    return ret;
}

void KTX_Free(ktx_texture_t *tex) {
    if (!tex) return;
    free(tex->data);
    free(tex->levelOffsets);
    free(tex->levelSizes);
    memset(tex, 0, sizeof(*tex));
}

const char *KTX_FormatName(ktx_format_t fmt) {
    switch (fmt) {
        case KTX_FMT_RGBA8: return "RGBA8";
        case KTX_FMT_BC1:   return "BC1 / DXT1";
        case KTX_FMT_BC3:   return "BC3 / DXT5";
        case KTX_FMT_BC5:   return "BC5 / RGTC2";
        case KTX_FMT_BC7:   return "BC7 / BPTC";
        default:            return "UNKNOWN";
    }
}
