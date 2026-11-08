#include "aroma_fg.h"
#include <cassert>

struct BackendState {
    unsigned calls=0, resets=0, commits=0, cancels=0;
    AromaFGStatus next_status=AROMA_FG_OK;
};
static AromaFGStatus init(void*, const AromaFGConfig*) { return AROMA_FG_OK; }
static void reset(void* p, uint32_t) { ++static_cast<BackendState*>(p)->resets; }
static AromaFGStatus process(void* p, const AromaFGFrameInput*, AromaFGFrameOutput* out) {
    auto& s=*static_cast<BackendState*>(p);
    ++s.calls;
    if (s.next_status != AROMA_FG_OK) {
        const auto result=s.next_status;
        s.next_status=AROMA_FG_OK;
        return result;
    }
    out->generated_frames = 1;
    out->flow_gpu_ms = 1.25f;
    return AROMA_FG_OK;
}
static AromaFGStatus commit(void* p, uint64_t) {
    ++static_cast<BackendState*>(p)->commits;
    return AROMA_FG_OK;
}
static AromaFGStatus cancel(void* p, uint64_t) {
    ++static_cast<BackendState*>(p)->cancels;
    return AROMA_FG_OK;
}
static AromaFGBackendV1 make_backend(BackendState& state, bool transactional=false) {
    AromaFGBackendV1 b{};
    b.struct_size=sizeof(b); b.api_version=AROMA_FG_API_VERSION;
    b.user_data=&state; b.initialize=init; b.reset_history=reset; b.process_frame=process;
    if (transactional) { b.commit_frame=commit; b.cancel_frame=cancel; }
    return b;
}
static AromaFGFrameInput frame(uint64_t id) {
    AromaFGFrameInput in{};
    in.struct_size=sizeof(in); in.frame_id=id; in.width=1280; in.height=720;
    return in;
}
static AromaFGFrameOutput output() {
    AromaFGFrameOutput out{}; out.struct_size=sizeof(out); return out;
}

int main() {
    // Legacy backend: successful process calls auto-commit for compatibility.
    AromaFGConfig c{}; aroma_fg_default_config(&c);
    BackendState legacy_state{};
    auto legacy_backend=make_backend(legacy_state);
    auto* legacy=aroma_fg_create(&c,&legacy_backend);
    assert(legacy);
    auto in=frame(1); auto out=output();
    assert(aroma_fg_process(legacy,&in,&out)==AROMA_FG_BYPASSED);
    c.enabled=1;
    assert(aroma_fg_set_config(legacy,&c)==AROMA_FG_OK);
    in=frame(2);
    assert(aroma_fg_process(legacy,&in,&out)==AROMA_FG_OK);
    assert(legacy_state.calls==1 && out.generated_frames==1);
    in=frame(3); in.scene_cut=1;
    assert(aroma_fg_process(legacy,&in,&out)==AROMA_FG_OK);
    assert(legacy_state.resets==1);
    assert(aroma_fg_process(legacy,&in,&out)==AROMA_FG_ERROR_FRAME_ORDER);
    assert(aroma_fg_commit_frame(legacy,3)==AROMA_FG_ERROR_UNSUPPORTED);
    aroma_fg_destroy(legacy);

    // Transactional backend: a failed/abandoned recording does not consume frame_id.
    aroma_fg_default_config(&c); c.enabled=1;
    BackendState tx_state{};
    auto tx_backend=make_backend(tx_state,true);
    auto* tx=aroma_fg_create(&c,&tx_backend);
    assert(tx);
    in=frame(10); out=output();
    tx_state.next_status=AROMA_FG_ERROR_BUSY;
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_ERROR_BUSY);
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_ERROR_BUSY); // pending transaction
    assert(aroma_fg_set_config(tx,&c)==AROMA_FG_ERROR_BUSY);
    assert(aroma_fg_cancel_frame(tx,10)==AROMA_FG_OK);
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_OK);
    assert(aroma_fg_commit_frame(tx,10)==AROMA_FG_OK);
    assert(tx_state.commits==1 && tx_state.cancels==1);

    in=frame(11); out=output();
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_OK);
    assert(aroma_fg_commit_frame(tx,11)==AROMA_FG_OK);
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_ERROR_FRAME_ORDER);

    in=frame(12); out=output();
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_OK);
    assert(aroma_fg_cancel_frame(tx,12)==AROMA_FG_OK);
    assert(aroma_fg_process(tx,&in,&out)==AROMA_FG_OK);
    assert(aroma_fg_commit_frame(tx,12)==AROMA_FG_OK);
    assert(tx_state.commits==3 && tx_state.cancels==2);
    aroma_fg_destroy(tx);
}
