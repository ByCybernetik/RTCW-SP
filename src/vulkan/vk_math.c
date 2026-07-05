#include "vk_local.h"
#include <string.h>
#include <math.h>

void VK_MatrixPerspective(float *dst, float fovY, float aspect, float zNear, float zFar) {
    float f = 1.0f / tanf(fovY * (float)M_PI / 360.0f);

    memset(dst, 0, sizeof(float) * 16);
    dst[0] = f / aspect;
    dst[5] = -f;
    dst[10] = zFar / (zNear - zFar);
    dst[11] = -1.0f;
    dst[14] = (zFar * zNear) / (zNear - zFar);
}

void VK_MatrixView(float *dst, const vec3_t origin, const vec3_t axis[3]) {
    dst[0] = axis[0][0]; dst[1] = axis[1][0]; dst[2] = axis[2][0]; dst[3] = 0;
    dst[4] = axis[0][1]; dst[5] = axis[1][1]; dst[6] = axis[2][1]; dst[7] = 0;
    dst[8] = axis[0][2]; dst[9] = axis[1][2]; dst[10] = axis[2][2]; dst[11] = 0;
    dst[12] = -DotProduct(origin, axis[0]);
    dst[13] = -DotProduct(origin, axis[1]);
    dst[14] = -DotProduct(origin, axis[2]);
    dst[15] = 1;
}

void VK_MatrixMul(float *dst, const float *a, const float *b) {
    float r[16];
    int col, row;

    /* OpenGL/GLSL column-major layout (same as bsp_viewer_vk Mat4Mul). */
    for (col = 0; col < 4; col++) {
        for (row = 0; row < 4; row++) {
            r[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    memcpy(dst, r, sizeof(r));
}

void VK_MatrixMulQ3Clip(float *dst, const float *proj, const float *mv) {
    float r[16];
    int i, j, k;

    /* Match R_TransformModelToClip: clip[i] = sum_j proj[i+j*4] * eye[j],
     * eye[j] = sum_k mv[j+k*4] * src[k]. */
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 4; k++) {
            float sum = 0.0f;

            for (j = 0; j < 4; j++) {
                sum += proj[i + j * 4] * mv[j + k * 4];
            }
            r[k * 4 + i] = sum;
        }
    }
    memcpy(dst, r, sizeof(r));
}

void VK_TransposeMatrix(float *dst, const float *src) {
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            dst[j * 4 + i] = src[i * 4 + j];
        }
    }
}

void VK_MatrixIdentity(float *dst) {
    memset(dst, 0, sizeof(float) * 16);
    dst[0] = 1; dst[5] = 1; dst[10] = 1; dst[15] = 1;
}

void VK_ConvertProjectionDepthToVulkan(float m[16]) {
    float tmp[16];
    int col;

    /* Remap clip Z from OpenGL [-w,w] to Vulkan [0,w] without touching the W row. */
    memcpy(tmp, m, sizeof(tmp));
    for (col = 0; col < 4; col++) {
        m[2 + col * 4] = 0.5f * tmp[2 + col * 4] + 0.5f * tmp[3 + col * 4];
    }
}

void VK_ConvertProjectionMatrixToVulkan(float m[16]) {
    m[5] *= -1.0f;
    VK_ConvertProjectionDepthToVulkan(m);
}
