#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>
#include <vector>
namespace aromafg::vk {
struct BorrowedContext { VkInstance instance=VK_NULL_HANDLE; VkPhysicalDevice physical_device=VK_NULL_HANDLE; VkDevice device=VK_NULL_HANDLE; VkQueue queue=VK_NULL_HANDLE; uint32_t queue_family_index=0; };
bool validate_context(const BorrowedContext&) noexcept;
class PipelineCache final {
public:
 PipelineCache()=default; ~PipelineCache();
 PipelineCache(const PipelineCache&)=delete; PipelineCache& operator=(const PipelineCache&)=delete;
 PipelineCache(PipelineCache&&) noexcept; PipelineCache& operator=(PipelineCache&&) noexcept;
 VkResult create(VkDevice,const void* initial_data=nullptr,size_t initial_size=0) noexcept;
 VkResult export_data(std::vector<uint8_t>&) const noexcept; void reset() noexcept;
 VkPipelineCache get() const noexcept{return cache_;}
private: VkDevice device_=VK_NULL_HANDLE; VkPipelineCache cache_=VK_NULL_HANDLE;
};
class DescriptorPool final {
public:
 DescriptorPool()=default; ~DescriptorPool();
 DescriptorPool(const DescriptorPool&)=delete; DescriptorPool& operator=(const DescriptorPool&)=delete;
 DescriptorPool(DescriptorPool&&) noexcept; DescriptorPool& operator=(DescriptorPool&&) noexcept;
 VkResult create(VkDevice,uint32_t max_sets,uint32_t storage_images_per_set,uint32_t uniform_buffers_per_set=0) noexcept;
 VkResult allocate(VkDescriptorSetLayout,VkDescriptorSet&) noexcept; VkResult reset() noexcept; void destroy() noexcept;
private: VkDevice device_=VK_NULL_HANDLE; VkDescriptorPool pool_=VK_NULL_HANDLE;
};
void image_barrier(VkCommandBuffer,VkImage,VkImageAspectFlags,VkAccessFlags,VkAccessFlags,VkPipelineStageFlags,VkPipelineStageFlags,VkImageLayout=VK_IMAGE_LAYOUT_GENERAL,VkImageLayout=VK_IMAGE_LAYOUT_GENERAL) noexcept;
struct PyramidLevel {uint32_t width=0,height=0;};
enum class FlowPassKind:uint8_t {Luma,Downsample,UpsampleFlow,SearchForward,SearchBackward,RefineForward,RefineBackward,Synthesize};
struct FlowPass {FlowPassKind kind{};uint32_t level=0;float phase=0.0f;};
std::vector<PyramidLevel> make_pyramid(uint32_t,uint32_t,uint32_t,uint32_t min_dimension=16);
std::vector<FlowPass> make_flow_schedule(uint32_t levels,uint32_t multiplier);
}
