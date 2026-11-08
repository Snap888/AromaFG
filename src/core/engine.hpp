#pragma once
#include "aroma_fg.h"
#include <atomic>
#include <mutex>

struct AromaFGEngine {
    AromaFGConfig config{};
    AromaFGBackendV1 backend{};
    std::mutex config_mutex;
    std::atomic_flag processing = ATOMIC_FLAG_INIT;
    uint64_t last_frame_id = 0;       // Last successfully committed or bypassed frame.
    uint64_t pending_frame_id = 0;    // Recorded transaction awaiting host commit/cancel.
    bool transactional_backend = false;
    bool pending_transaction = false;
    bool pending_can_commit = false;
    bool initialized = false;
};
