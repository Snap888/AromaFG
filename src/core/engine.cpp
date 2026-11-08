#include "engine.hpp"
#include <cmath>
#include <cstring>
#include <new>

namespace {
bool valid_config(const AromaFGConfig* c) {
    return c && c->struct_size >= sizeof(AromaFGConfig)
        && c->api_version == AROMA_FG_API_VERSION
        && c->multiplier >= 1 && c->multiplier <= AROMA_FG_MAX_MULTIPLIER
        && c->quality >= AROMA_FG_QUALITY_PERFORMANCE
        && c->quality <= AROMA_FG_QUALITY_ULTRA
        && c->flow_scale_percent >= 25 && c->flow_scale_percent <= 100;
}
bool valid_backend(const AromaFGBackendV1* b) {
    return b && b->struct_size >= sizeof(AromaFGBackendV1)
        && b->api_version == AROMA_FG_API_VERSION && b->process_frame;
}
struct BusyGuard {
    std::atomic_flag& flag;
    ~BusyGuard() { flag.clear(std::memory_order_release); }
};
bool valid_output(const AromaFGFrameOutput& o, uint32_t max_frames) {
    return o.generated_frames <= max_frames
        && std::isfinite(o.preprocess_gpu_ms) && o.preprocess_gpu_ms >= 0.0f
        && std::isfinite(o.flow_gpu_ms) && o.flow_gpu_ms >= 0.0f
        && std::isfinite(o.synthesis_gpu_ms) && o.synthesis_gpu_ms >= 0.0f;
}
}

extern "C" void aroma_fg_default_config(AromaFGConfig* c) {
    if (!c) return;
    std::memset(c, 0, sizeof(*c));
    c->struct_size = sizeof(*c);
    c->api_version = AROMA_FG_API_VERSION;
    c->multiplier = 2;
    c->quality = AROMA_FG_QUALITY_BALANCED;
    c->flow_scale_percent = 75;
    c->profiling_enabled = 1;
}

extern "C" AromaFGEngine* aroma_fg_create(
    const AromaFGConfig* config, const AromaFGBackendV1* backend) {
    AromaFGConfig c{};
    if (config) c = *config; else aroma_fg_default_config(&c);
    if (!valid_config(&c) || !valid_backend(backend)) return nullptr;
    auto* e = new (std::nothrow) AromaFGEngine();
    if (!e) return nullptr;
    e->config = c;
    e->backend = *backend;
    if (e->backend.initialize &&
        e->backend.initialize(e->backend.user_data, &e->config) != AROMA_FG_OK) {
        delete e;
        return nullptr;
    }
    e->initialized = true;
    return e;
}

extern "C" void aroma_fg_destroy(AromaFGEngine* e) {
    if (!e) return;
    while (e->processing.test_and_set(std::memory_order_acquire)) {}
    if (e->initialized && e->backend.shutdown)
        e->backend.shutdown(e->backend.user_data);
    e->initialized = false;
    e->processing.clear(std::memory_order_release);
    delete e;
}

extern "C" AromaFGStatus aroma_fg_set_config(
    AromaFGEngine* e, const AromaFGConfig* c) {
    if (!e || !valid_config(c)) return AROMA_FG_ERROR_INVALID_ARGUMENT;
    if (e->processing.test_and_set(std::memory_order_acquire))
        return AROMA_FG_ERROR_BUSY;
    BusyGuard guard{e->processing};
    std::lock_guard<std::mutex> lock(e->config_mutex);
    if (e->backend.update_config) {
        const auto status = e->backend.update_config(e->backend.user_data, c);
        if (status != AROMA_FG_OK) return status;
    } else if (e->config.multiplier != c->multiplier ||
               e->config.quality != c->quality ||
               e->config.flow_scale_percent != c->flow_scale_percent) {
        return AROMA_FG_ERROR_BACKEND;
    }
    const bool enabled_changed = e->config.enabled != c->enabled;
    e->config = *c;
    if (enabled_changed && e->backend.reset_history)
        e->backend.reset_history(e->backend.user_data, 1u);
    return AROMA_FG_OK;
}

extern "C" AromaFGStatus aroma_fg_process(
    AromaFGEngine* e, const AromaFGFrameInput* in, AromaFGFrameOutput* out) {
    if (!e || !in || !out || in->struct_size < sizeof(*in)
        || out->struct_size < sizeof(*out) || !in->frame_id
        || !in->width || !in->height)
        return AROMA_FG_ERROR_INVALID_ARGUMENT;
    if (e->processing.test_and_set(std::memory_order_acquire))
        return AROMA_FG_ERROR_BUSY;
    BusyGuard guard{e->processing};
    AromaFGConfig c{};
    {
        std::lock_guard<std::mutex> lock(e->config_mutex);
        c = e->config;
    }
    const uint32_t output_size = out->struct_size;
    std::memset(reinterpret_cast<char*>(out) + sizeof(uint32_t), 0,
        (output_size < sizeof(*out) ? output_size : sizeof(*out)) - sizeof(uint32_t));
    if (in->frame_id <= e->last_frame_id && e->last_frame_id)
        return AROMA_FG_ERROR_FRAME_ORDER;
    if (in->scene_cut && e->backend.reset_history)
        e->backend.reset_history(e->backend.user_data, 2u);
    e->last_frame_id = in->frame_id;
    if (!c.enabled || c.multiplier == 1)
        return AROMA_FG_BYPASSED;
    out->struct_size = sizeof(*out);
    const auto status = e->backend.process_frame(e->backend.user_data, in, out);
    if (status != AROMA_FG_OK) return status;
    if (!valid_output(*out, c.multiplier - 1u))
        return AROMA_FG_ERROR_BACKEND;
    return AROMA_FG_OK;
}

extern "C" uint32_t aroma_fg_get_api_version(void) {
    return AROMA_FG_API_VERSION;
}