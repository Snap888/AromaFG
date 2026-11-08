#include "aroma_fg.h"
#include <cassert>

struct BackendState { unsigned calls=0, resets=0; };
static AromaFGStatus init(void*, const AromaFGConfig*) { return AROMA_FG_OK; }
static void reset(void* p, uint32_t) { ++static_cast<BackendState*>(p)->resets; }
static AromaFGStatus process(void* p, const AromaFGFrameInput*, AromaFGFrameOutput* out) {
    ++static_cast<BackendState*>(p)->calls;
    out->generated_frames = 1;
    out->flow_gpu_ms = 1.25f;
    return AROMA_FG_OK;
}
int main() {
    AromaFGConfig c{};
    aroma_fg_default_config(&c);
    BackendState state{};
    AromaFGBackendV1 b{};
    b.struct_size = sizeof(b); b.api_version = AROMA_FG_API_VERSION;
    b.user_data = &state; b.initialize = init; b.reset_history = reset; b.process_frame = process;
    auto* e = aroma_fg_create(&c, &b);
    assert(e);
    AromaFGFrameInput in{};
    in.struct_size = sizeof(in); in.frame_id = 1; in.width = 1280; in.height = 720;
    AromaFGFrameOutput out{}; out.struct_size = sizeof(out);
    assert(aroma_fg_process(e, &in, &out) == AROMA_FG_BYPASSED);
    c.enabled = 1;
    assert(aroma_fg_set_config(e, &c) == AROMA_FG_OK);
    in.frame_id = 2;
    assert(aroma_fg_process(e, &in, &out) == AROMA_FG_OK);
    assert(state.calls == 1 && out.generated_frames == 1);
    in.frame_id = 3; in.scene_cut = 1;
    assert(aroma_fg_process(e, &in, &out) == AROMA_FG_OK);
    assert(state.resets == 2);
    assert(aroma_fg_process(e, &in, &out) == AROMA_FG_ERROR_FRAME_ORDER);
    aroma_fg_destroy(e);
}