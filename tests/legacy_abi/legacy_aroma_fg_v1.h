#ifndef AROMA_FG_LEGACY_V1_H
#define AROMA_FG_LEGACY_V1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AromaFGEngine AromaFGEngine;
typedef enum AromaFGStatus {
    AROMA_FG_OK = 0,
    AROMA_FG_BYPASSED = 1,
    AROMA_FG_ERROR_INVALID_ARGUMENT = -1,
    AROMA_FG_ERROR_VERSION_MISMATCH = -2,
    AROMA_FG_ERROR_BACKEND = -3,
    AROMA_FG_ERROR_BUSY = -4,
    AROMA_FG_ERROR_FRAME_ORDER = -5,
    AROMA_FG_ERROR_UNSUPPORTED = -6
} AromaFGStatus;
typedef enum AromaFGQuality {
    AROMA_FG_QUALITY_PERFORMANCE = 0,
    AROMA_FG_QUALITY_BALANCED = 1,
    AROMA_FG_QUALITY_QUALITY = 2,
    AROMA_FG_QUALITY_ULTRA = 3
} AromaFGQuality;
typedef struct AromaFGConfig {
    uint32_t struct_size, api_version, enabled, multiplier;
    AromaFGQuality quality;
    uint32_t flow_scale_percent, profiling_enabled, diagnostics_enabled;
} AromaFGConfig;
typedef struct AromaFGFrameInput {
    uint32_t struct_size;
    uint64_t frame_id, timestamp_ns;
    uint32_t width, height, format, scene_cut;
    uintptr_t host_frame;
} AromaFGFrameInput;
typedef struct AromaFGFrameOutput {
    uint32_t struct_size, generated_frames, flags;
    float preprocess_gpu_ms, flow_gpu_ms, synthesis_gpu_ms;
} AromaFGFrameOutput;

/* Frozen V1 layout as shipped before transactional callbacks were appended. */
typedef struct AromaFGBackendV1 {
    uint32_t struct_size, api_version;
    void* user_data;
    AromaFGStatus (*initialize)(void*, const AromaFGConfig*);
    void (*reset_history)(void*, uint32_t);
    AromaFGStatus (*update_config)(void*, const AromaFGConfig*);
    AromaFGStatus (*process_frame)(void*, const AromaFGFrameInput*, AromaFGFrameOutput*);
    void (*shutdown)(void*);
} AromaFGBackendV1;

void aroma_fg_default_config(AromaFGConfig*);
AromaFGEngine* aroma_fg_create(const AromaFGConfig*, const AromaFGBackendV1*);
void aroma_fg_destroy(AromaFGEngine*);
AromaFGStatus aroma_fg_process(AromaFGEngine*, const AromaFGFrameInput*, AromaFGFrameOutput*);

#ifdef __cplusplus
}
#endif
#endif
