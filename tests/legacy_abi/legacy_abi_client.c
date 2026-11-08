#include "legacy_aroma_fg_v1.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

static AromaFGStatus process_frame(void* user, const AromaFGFrameInput* input, AromaFGFrameOutput* output) {
    unsigned* calls = (unsigned*)user;
    (void)input;
    ++*calls;
    output->generated_frames = 1;
    return AROMA_FG_OK;
}

int main(void) {
    unsigned calls = 0;
    AromaFGConfig config;
    memset(&config, 0, sizeof(config));
    aroma_fg_default_config(&config);
    config.enabled = 1;

    AromaFGBackendV1 backend;
    memset(&backend, 0, sizeof(backend));
    backend.struct_size = (uint32_t)sizeof(backend);
    backend.api_version = 1;
    backend.user_data = &calls;
    backend.process_frame = process_frame;

    /* This client is compiled against the frozen old header, not today's header. */
    AromaFGEngine* engine = aroma_fg_create(&config, &backend);
    assert(engine != NULL);

    AromaFGFrameInput input;
    memset(&input, 0, sizeof(input));
    input.struct_size = (uint32_t)sizeof(input);
    input.frame_id = 1;
    input.width = 64;
    input.height = 64;

    AromaFGFrameOutput output;
    memset(&output, 0, sizeof(output));
    output.struct_size = (uint32_t)sizeof(output);
    assert(aroma_fg_process(engine, &input, &output) == AROMA_FG_OK);
    assert(calls == 1 && output.generated_frames == 1);

    aroma_fg_destroy(engine);
    return 0;
}
