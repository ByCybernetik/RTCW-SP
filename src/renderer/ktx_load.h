/*
 * KTX texture loader for the standalone viewer.
 *
 * Supports KTX1 and a basic subset of KTX2:
 *   - uncompressed RGBA8
 *   - BC1, BC3, BC5, BC7 block-compressed formats
 *   - no supercompression, no Basis Universal, no arrays/cubemaps/3D yet
 */

#ifndef KTX_LOAD_H
#define KTX_LOAD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KTX_FMT_UNKNOWN = 0,
    KTX_FMT_RGBA8,
    KTX_FMT_BC1,
    KTX_FMT_BC3,
    KTX_FMT_BC5,
    KTX_FMT_BC7,
} ktx_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    uint32_t faceCount;
    ktx_format_t format;
    int srgb;

    uint8_t *data;       /* contiguous mip data, base level first */
    size_t dataSize;

    /* offsets and sizes for each mip level (base level = 0) */
    size_t *levelOffsets;
    size_t *levelSizes;
} ktx_texture_t;

/* Load a KTX file from the filesystem. Returns 0 on success. */
int KTX_LoadFromFile(const char *path, ktx_texture_t *out);

/* Load a KTX file from a memory buffer. Returns 0 on success. */
int KTX_LoadFromMemory(const uint8_t *data, size_t size, ktx_texture_t *out);

/* Free all memory owned by a loaded texture. */
void KTX_Free(ktx_texture_t *tex);

/* Human-readable format name. */
const char *KTX_FormatName(ktx_format_t fmt);

#ifdef __cplusplus
}
#endif

#endif /* KTX_LOAD_H */
