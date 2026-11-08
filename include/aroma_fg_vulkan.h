#ifndef AROMA_FG_VULKAN_H
#define AROMA_FG_VULKAN_H
#include "aroma_fg.h"
#include <vulkan/vulkan.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AROMA_FG_VULKAN_INTEROP_VERSION 1u
/* Host-borrowed handles remain valid until the process callback returns. */
typedef struct AromaFGVulkanFrameV1 {
 uint32_t struct_size, api_version;
 VkCommandBuffer command_buffer;
 VkImage source_image; VkImageView source_view;
 VkFormat source_format; VkImageLayout source_layout;
 VkImage* output_images; VkImageView* output_views; VkImageLayout* output_layouts;
 uint32_t output_capacity; VkFormat output_format; VkExtent2D extent;
} AromaFGVulkanFrameV1;
typedef struct AromaFGVulkanContextV1 {
 uint32_t struct_size, api_version;
 VkInstance instance; VkPhysicalDevice physical_device; VkDevice device; VkQueue queue;
 uint32_t queue_family_index;
 VkPhysicalDeviceProperties properties;
 VkPhysicalDeviceMemoryProperties memory_properties;
} AromaFGVulkanContextV1;
AROMA_FG_API int aroma_fg_vulkan_validate_context(const AromaFGVulkanContextV1* context);
AROMA_FG_API int aroma_fg_vulkan_validate_frame(const AromaFGVulkanFrameV1* frame, uint32_t required_outputs);
#ifdef __cplusplus
}
#endif
#endif
