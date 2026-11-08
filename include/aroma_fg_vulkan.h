#ifndef AROMA_FG_VULKAN_H
#define AROMA_FG_VULKAN_H
#include "aroma_fg.h"
#include <vulkan/vulkan.h>
#ifdef __cplusplus
extern "C" {
#endif

#define AROMA_FG_VULKAN_INTEROP_VERSION 2u
#define AROMA_FG_VULKAN_FRAME_HOST_QUIESCENT (1u << 0)

/* Host-borrowed Vulkan context. AromaFG never destroys these handles. */
typedef struct AromaFGVulkanContextV2 {
    uint32_t struct_size, api_version;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family_index;
} AromaFGVulkanContextV2;

/*
 * The host supplies a previous/current real-frame pair and M-1 writable output
 * images. Inputs are transitioned to SHADER_READ_ONLY_OPTIMAL and restored.
 * Outputs are transitioned to GENERAL and restored to output_restore_layouts.
 * The host must submit command buffers to this queue in callback order.
 * Before reusing frame_slot, the host MUST wait for its submission fence.
 */
typedef struct AromaFGVulkanFrameV2 {
    uint32_t struct_size, api_version;
    VkCommandBuffer command_buffer;
    VkImage previous_image, current_image;
    VkImageView previous_view, current_view;
    VkFormat source_format;
    VkImageLayout previous_layout, current_layout;
    VkPipelineStageFlags previous_stage, current_stage;
    VkAccessFlags previous_access, current_access;
    VkImageLayout previous_restore_layout, current_restore_layout;
    VkPipelineStageFlags previous_restore_stage, current_restore_stage;
    VkAccessFlags previous_restore_access, current_restore_access;
    VkImage* output_images;
    VkImageView* output_views;
    VkImageLayout* output_layouts;          /* layout before AromaFG access */
    VkImageLayout* output_restore_layouts;  /* layout expected after AromaFG */
    uint32_t output_capacity;
    VkFormat output_format;                 /* RGBA8_UNORM or RGBA16F */
    VkPipelineStageFlags output_stage;
    VkAccessFlags output_access;
    VkExtent2D extent;
    uint32_t frame_slot;
    uint32_t flags;
} AromaFGVulkanFrameV2;

typedef struct AromaFGVulkanBackendCreateInfoV1 {
    uint32_t struct_size, api_version;
    AromaFGVulkanContextV2 context;
    uint32_t in_flight_slots; /* 2..4; default 3 */
} AromaFGVulkanBackendCreateInfoV1;

typedef struct AromaFGVulkanTimingV1 {
    uint32_t struct_size, api_version;
    float preprocess_gpu_ms;
    float flow_gpu_ms;
    float synthesis_gpu_ms;
    uint64_t completed_frame_id;
} AromaFGVulkanTimingV1;

AROMA_FG_API int aroma_fg_vulkan_validate_context(const AromaFGVulkanContextV2* context);
AROMA_FG_API int aroma_fg_vulkan_validate_frame(const AromaFGVulkanFrameV2* frame, uint32_t required_outputs);

/* Creates an AromaFGBackendV1 adapter. Pass it to aroma_fg_create(). */
AROMA_FG_API AromaFGStatus aroma_fg_vulkan_backend_create(
    const AromaFGVulkanBackendCreateInfoV1* info, AromaFGBackendV1* backend);
/* Call only after the host has waited for all submitted AromaFG command buffers. */
AROMA_FG_API AromaFGStatus aroma_fg_vulkan_backend_collect_timing(
    AromaFGBackendV1* backend, uint32_t frame_slot, AromaFGVulkanTimingV1* timing);

#ifdef __cplusplus
}
#endif
#endif
