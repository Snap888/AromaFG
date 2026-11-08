#pragma once
#include "aroma_fg.h"
#include <atomic>
#include <mutex>

struct AromaFGEngine {
    AromaFGConfig config{};
    AromaFGBackendV1 backend{};
    std::mutex config_mutex;
    std::atomic_flag processing = ATOMIC_FLAG_INIT;
    uint64_t last_frame_id = 0;
    bool initialized = false;
};