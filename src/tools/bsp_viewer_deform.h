/* bsp_viewer_deform.h - Vertex deformation (deformVertexes) for walls.shader */

#ifndef BSP_VIEWER_DEFORM_H
#define BSP_VIEWER_DEFORM_H

#include <stdint.h>

/* Deformation types */
enum {
    BV_DEFORM_NONE = 0,
    BV_DEFORM_WAVE,
    BV_DEFORM_NORMAL,
    BV_DEFORM_MOVE,
    BV_DEFORM_AUTOSPRITE,
    BV_DEFORM_AUTOSPRITE2,
    BV_DEFORM_PROJECTION_SHADOW
};

/* deformVertexes wave parameters */
typedef struct {
    int type;               /* DEFORM_* enum */
    
    /* For DEFORM_WAVE */
    int waveDiv;            /* Tessellation divisor (64, 32, 100, etc.) */
    int waveFunc;           /* 0=sin, 1=triangle, 2=sawtooth, 3=inversesawtooth */
    float waveBase;         /* Base offset */
    float waveAmp;          /* Amplitude */
    float wavePhase;        /* Phase shift */
    float waveFreq;         /* Frequency (cycles per second) */
} deform_state_t;

/* Parse deformVertexes directive.
 * Supports:
 *   deformVertexes wave <div> <func> <base> <amp> <phase> <freq>
 *   deformVertexes normal <div> <func> <base> <amp> <phase> <freq>
 *   deformVertexes move <x> <y> <z> <func> <base> <amp> <phase> <freq>
 *   deformVertexes autosprite
 *   deformVertexes autosprite2
 * Returns 1 on success, 0 if not deformVertexes line */
int ParseDeformVertexes(const char *line, deform_state_t *out);

/* Reset deform state to none */
void ResetDeform(deform_state_t *state);

/* Apply vertex deformation to a single vertex.
 * Inputs:
 *   state - deformation parameters
 *   timeSec - current time for wave animation
 *   position - input vertex position
 *   normal - vertex normal
 * Output:
 *   outPosition - modified position */
void ApplyDeform(const deform_state_t *state, float timeSec,
                 const float position[3], const float normal[3],
                 float outPosition[3]);

/* Wave function evaluation for deform (shared with rgbgen) */
float DeformEvalWave(int waveFunc, float x);

/* Get wave function name as string for debugging */
const char* WaveFuncName(int waveFunc);

#endif /* BSP_VIEWER_DEFORM_H */
