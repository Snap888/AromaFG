#include "vulkan_backend.hpp"
#include "aroma_fg_vulkan.h"
#include <algorithm>
#include <utility>
namespace aromafg::vk {
bool validate_context(const BorrowedContext& c) noexcept {
 if(!c.instance||!c.physical_device||!c.device||!c.queue)return false;
 VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(c.physical_device,&p);
 if(p.apiVersion<VK_API_VERSION_1_1)return false;
 uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(c.physical_device,&n,nullptr);
 if(c.queue_family_index>=n)return false;
 std::vector<VkQueueFamilyProperties> q(n);vkGetPhysicalDeviceQueueFamilyProperties(c.physical_device,&n,q.data());
 return (q[c.queue_family_index].queueFlags&VK_QUEUE_COMPUTE_BIT)!=0;
}

extern "C" int aroma_fg_vulkan_validate_context(const AromaFGVulkanContextV1* c) {
 if (!c || c->struct_size < sizeof(AromaFGVulkanContextV1) ||
     c->api_version != AROMA_FG_VULKAN_INTEROP_VERSION) return 0;
 aromafg::vk::BorrowedContext borrowed{c->instance,c->physical_device,c->device,c->queue,c->queue_family_index};
 return aromafg::vk::validate_context(borrowed) ? 1 : 0;
}

extern "C" int aroma_fg_vulkan_validate_frame(const AromaFGVulkanFrameV1* f,uint32_t required_outputs) {
 if (!f || f->struct_size < sizeof(AromaFGVulkanFrameV1) ||
     f->api_version != AROMA_FG_VULKAN_INTEROP_VERSION ||
     f->command_buffer == VK_NULL_HANDLE || f->source_image == VK_NULL_HANDLE ||
     f->source_view == VK_NULL_HANDLE || f->source_format == VK_FORMAT_UNDEFINED ||
     f->source_layout == VK_IMAGE_LAYOUT_UNDEFINED || !f->extent.width || !f->extent.height ||
     required_outputs > f->output_capacity) return 0;
 if (required_outputs && (!f->output_images || !f->output_views || !f->output_layouts ||
                          f->output_format == VK_FORMAT_UNDEFINED)) return 0;
 for (uint32_t i=0;i<required_outputs;++i) {
   if (f->output_images[i] == VK_NULL_HANDLE || f->output_views[i] == VK_NULL_HANDLE ||
       f->output_layouts[i] == VK_IMAGE_LAYOUT_UNDEFINED) return 0;
 }
 return 1;
}
PipelineCache::~PipelineCache(){reset();}
PipelineCache::PipelineCache(PipelineCache&& o) noexcept:device_(std::exchange(o.device_,VK_NULL_HANDLE)),cache_(std::exchange(o.cache_,VK_NULL_HANDLE)){}
PipelineCache& PipelineCache::operator=(PipelineCache&& o) noexcept {if(this!=&o){reset();device_=std::exchange(o.device_,VK_NULL_HANDLE);cache_=std::exchange(o.cache_,VK_NULL_HANDLE);}return *this;}
VkResult PipelineCache::create(VkDevice d,const void* data,size_t size) noexcept {
 if(!d||(size&&!data))return VK_ERROR_INITIALIZATION_FAILED;reset();
 VkPipelineCacheCreateInfo i{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};i.initialDataSize=size;i.pInitialData=data;
 VkResult r=vkCreatePipelineCache(d,&i,nullptr,&cache_);if(r==VK_SUCCESS)device_=d;return r;
}
VkResult PipelineCache::export_data(std::vector<uint8_t>& data) const noexcept {
 if(!cache_)return VK_ERROR_INITIALIZATION_FAILED;size_t n=0;VkResult r=vkGetPipelineCacheData(device_,cache_,&n,nullptr);if(r!=VK_SUCCESS)return r;
 data.resize(n);r=vkGetPipelineCacheData(device_,cache_,&n,data.data());if(r==VK_SUCCESS)data.resize(n);else data.clear();return r;
}
void PipelineCache::reset() noexcept {if(cache_&&device_)vkDestroyPipelineCache(device_,cache_,nullptr);cache_=VK_NULL_HANDLE;device_=VK_NULL_HANDLE;}
DescriptorPool::~DescriptorPool(){destroy();}
DescriptorPool::DescriptorPool(DescriptorPool&& o) noexcept:device_(std::exchange(o.device_,VK_NULL_HANDLE)),pool_(std::exchange(o.pool_,VK_NULL_HANDLE)){}
DescriptorPool& DescriptorPool::operator=(DescriptorPool&& o) noexcept {if(this!=&o){destroy();device_=std::exchange(o.device_,VK_NULL_HANDLE);pool_=std::exchange(o.pool_,VK_NULL_HANDLE);}return *this;}
VkResult DescriptorPool::create(VkDevice d,uint32_t sets,uint32_t storage,uint32_t uniforms) noexcept {
 if(!d||!sets)return VK_ERROR_INITIALIZATION_FAILED;destroy();VkDescriptorPoolSize sizes[2]{};uint32_t n=0;
 if(storage){sizes[n]={VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,sets*storage};++n;}
 if(uniforms){sizes[n]={VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,sets*uniforms};++n;}
 if(!n)return VK_ERROR_INITIALIZATION_FAILED;VkDescriptorPoolCreateInfo i{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};i.maxSets=sets;i.poolSizeCount=n;i.pPoolSizes=sizes;
 VkResult r=vkCreateDescriptorPool(d,&i,nullptr,&pool_);if(r==VK_SUCCESS)device_=d;return r;
}
VkResult DescriptorPool::allocate(VkDescriptorSetLayout l,VkDescriptorSet& s) noexcept {
 if(!pool_||!l)return VK_ERROR_INITIALIZATION_FAILED;VkDescriptorSetAllocateInfo i{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};i.descriptorPool=pool_;i.descriptorSetCount=1;i.pSetLayouts=&l;return vkAllocateDescriptorSets(device_,&i,&s);
}
VkResult DescriptorPool::reset() noexcept{return pool_?vkResetDescriptorPool(device_,pool_,0):VK_ERROR_INITIALIZATION_FAILED;}
void DescriptorPool::destroy() noexcept{if(pool_&&device_)vkDestroyDescriptorPool(device_,pool_,nullptr);pool_=VK_NULL_HANDLE;device_=VK_NULL_HANDLE;}
void image_barrier(VkCommandBuffer c,VkImage image,VkImageAspectFlags aspect,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags ss,VkPipelineStageFlags ds,VkImageLayout oldl,VkImageLayout newl) noexcept {
 if(!c||!image)return;VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=src;b.dstAccessMask=dst;b.oldLayout=oldl;b.newLayout=newl;b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={aspect,0,1,0,1};vkCmdPipelineBarrier(c,ss,ds,0,0,nullptr,0,nullptr,1,&b);
}
std::vector<PyramidLevel> make_pyramid(uint32_t w,uint32_t h,uint32_t max,uint32_t min) {
 std::vector<PyramidLevel> v;if(!w||!h||!max||!min)return v;v.push_back({w,h});
 while(v.size()<max){auto p=v.back();uint32_t nw=std::max(1u,(p.width+1u)/2u),nh=std::max(1u,(p.height+1u)/2u);if(nw<min||nh<min)break;v.push_back({nw,nh});}return v;
}
std::vector<FlowPass> make_flow_schedule(uint32_t levels,uint32_t multiplier) {
 std::vector<FlowPass> p;if(!levels||multiplier<2||multiplier>4)return p;p.push_back({FlowPassKind::Luma,0,0});p.push_back({FlowPassKind::Luma,0,0});
 for(uint32_t l=1;l<levels;++l){p.push_back({FlowPassKind::Downsample,l,0});p.push_back({FlowPassKind::Downsample,l,0});}
 for(int32_t l=int32_t(levels)-1;l>=0;--l){if(uint32_t(l)!=levels-1)p.push_back({FlowPassKind::UpsampleFlow,uint32_t(l),0});p.push_back({FlowPassKind::UpsampleFlow,uint32_t(l),0});p.push_back({FlowPassKind::SearchForward,uint32_t(l),0});p.push_back({FlowPassKind::SearchBackward,uint32_t(l),0});}
 p.push_back({FlowPassKind::RefineForward,0,0});p.push_back({FlowPassKind::RefineBackward,0,0});
 for(uint32_t k=1;k<multiplier;++k)p.push_back({FlowPassKind::Synthesize,0,float(k)/float(multiplier)});return p;
}
}
