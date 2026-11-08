#ifndef AROMA_FG_H
#define AROMA_FG_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(AROMA_FG_BUILD)
#    define AROMA_FG_API __declspec(dllexport)
#  else
#    define AROMA_FG_API __declspec(dllimport)
#  endif
#else
#  define AROMA_FG_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AROMA_FG_API_VERSION 1u
#define AROMA_FG_MAX_MULTIPLIER 4u

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
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t enabled;
    uint32_t multiplier;       /* 1 = bypass; 2..4 = up to M-1 generated phases */
    AromaFGQuality quality;
    uint32_t flow_scale_percent;
    uint32_t profiling_enabled;
    uint32_t diagnostics_enabled;
} AromaFGConfig;

typedef struct AromaFGFrameInput {
    uint32_t struct_size;
    uint64_t frame_id;
    uint64_t timestamp_ns;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t scene_cut;
    uintptr_t host_frame;      /* Opaque token; core never dereferences it. */
} AromaFGFrameInput;

typedef struct AromaFGFrameOutput {
    uint32_t struct_size;
    uint32_t generated_frames;
    uint32_t flags;
    float preprocess_gpu_ms;
    float flow_gpu_ms;
    float synthesis_gpu_ms;
} AromaFGFrameOutput;

/*
 * Backend contract:
 * - host owns Vulkan device, queue, command buffers, swapchain and WSI sync;
 * - backend records work into host-provided resources only;
 * - backend never submits host command buffers or waits synchronously;
 * - at most multiplier-1 distinct chronological phases may be generated;
 * - borrowed handles are valid only for the callback duration.
 */
typedef struct AromaFGBackendV1 {
    uint32_t struct_size;
    uint32_t api_version;
    void* user_data;
    AromaFGStatus (*initialize)(void* user_data, const AromaFGConfig* config);
    void (*reset_history)(void* user_data, uint32_t reason);
    AromaFGStatus (*update_config)(void* user_data, const AromaFGConfig* config);
    AromaFGStatus (*process_frame)(void* user_data,
        const AromaFGFrameInput* input, AromaFGFrameOutput* output);
    void (*shutdown)(void* user_data);
} AromaFGBackendV1;

AROMA_FG_API void aroma_fg_default_config(AromaFGConfig* config);
AROMA_FG_API AromaFGEngine* aroma_fg_create(
    const AromaFGConfig* config, const AromaFGBackendV1* backend);
AROMA_FG_API void aroma_fg_destroy(AromaFGEngine* engine);
AROMA_FG_API AromaFGStatus aroma_fg_set_config(
    AromaFGEngine* engine, const AromaFGConfig* config);
AROMA_FG_API AromaFGStatus aroma_fg_process(
    AromaFGEngine* engine, const AromaFGFrameInput* input,
    AromaFGFrameOutput* output);
AROMA_FG_API uint32_t aroma_fg_get_api_version(void);

#ifdef __cplusplus
}
#endif
#endif
