#include "aroma_fg.h"
#include "aroma_fg_vulkan.h"
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>

namespace {
struct TestImage { VkImage image=VK_NULL_HANDLE; VkDeviceMemory memory=VK_NULL_HANDLE; VkImageView view=VK_NULL_HANDLE; };
uint32_t memory_type(VkPhysicalDevice phys,uint32_t bits,VkMemoryPropertyFlags flags) {
 VkPhysicalDeviceMemoryProperties p{};vkGetPhysicalDeviceMemoryProperties(phys,&p);
 for(uint32_t i=0;i<p.memoryTypeCount;++i)
  if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags)return i;
 return UINT32_MAX;
}
bool make_image(VkPhysicalDevice phys,VkDevice dev,uint32_t w,uint32_t h,VkFormat fmt,
                VkImageUsageFlags usage,TestImage& out) {
 VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;
 ci.format=fmt;ci.extent={w,h,1};ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;
 ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 if(vkCreateImage(dev,&ci,nullptr,&out.image)!=VK_SUCCESS)return false;
 VkMemoryRequirements mr{};vkGetImageMemoryRequirements(dev,out.image,&mr);
 uint32_t mt=memory_type(phys,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
 if(mt==UINT32_MAX)return false;
 VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=mt;
 if(vkAllocateMemory(dev,&ai,nullptr,&out.memory)!=VK_SUCCESS)return false;
 if(vkBindImageMemory(dev,out.image,out.memory,0)!=VK_SUCCESS)return false;
 VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=out.image;
 vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=fmt;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
 return vkCreateImageView(dev,&vi,nullptr,&out.view)==VK_SUCCESS;
}
void destroy_image(VkDevice dev,TestImage& i) {
 if(i.view)vkDestroyImageView(dev,i.view,nullptr);
 if(i.image)vkDestroyImage(dev,i.image,nullptr);
 if(i.memory)vkFreeMemory(dev,i.memory,nullptr);
 i={};
}
bool make_readback(VkPhysicalDevice phys,VkDevice dev,VkDeviceSize size,VkBuffer& b,VkDeviceMemory& m) {
 VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=size;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
 bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 if(vkCreateBuffer(dev,&bi,nullptr,&b)!=VK_SUCCESS)return false;
 VkMemoryRequirements mr{};vkGetBufferMemoryRequirements(dev,b,&mr);
 uint32_t mt=memory_type(phys,mr.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
 if(mt==UINT32_MAX)return false;
 VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=mt;
 if(vkAllocateMemory(dev,&ai,nullptr,&m)!=VK_SUCCESS)return false;
 return vkBindBufferMemory(dev,b,m,0)==VK_SUCCESS;
}
void image_barrier(VkCommandBuffer cmd,VkImage image,VkImageLayout old_layout,VkImageLayout new_layout,
                   VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags ss,VkPipelineStageFlags ds) {
 VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=old_layout;b.newLayout=new_layout;
 b.srcAccessMask=src;b.dstAccessMask=dst;b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
 b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;
 b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
 vkCmdPipelineBarrier(cmd,ss,ds,0,0,nullptr,0,nullptr,1,&b);
}
}

int main() {
 VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="AromaFG device smoke";
 app.apiVersion=VK_API_VERSION_1_1;
 VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
 VkInstance instance=VK_NULL_HANDLE;
 if(vkCreateInstance(&ici,nullptr,&instance)!=VK_SUCCESS)return 10;
 uint32_t count=0;vkEnumeratePhysicalDevices(instance,&count,nullptr);
 if(!count){vkDestroyInstance(instance,nullptr);return 11;}
 std::vector<VkPhysicalDevice> devices(count);vkEnumeratePhysicalDevices(instance,&count,devices.data());
 VkPhysicalDevice phys=VK_NULL_HANDLE;uint32_t family=UINT32_MAX;VkPhysicalDeviceProperties props{};
 for(auto d:devices) {
  VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(d,&p);
  uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(d,&n,nullptr);
  std::vector<VkQueueFamilyProperties> q(n);vkGetPhysicalDeviceQueueFamilyProperties(d,&n,q.data());
  for(uint32_t i=0;i<n;++i)if(q[i].queueFlags&VK_QUEUE_COMPUTE_BIT){
   VkFormatProperties fp{};vkGetPhysicalDeviceFormatProperties(d,VK_FORMAT_R8G8B8A8_UNORM,&fp);
   if(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT){phys=d;family=i;props=p;break;}
  }
  if(phys)break;
 }
 if(!phys){vkDestroyInstance(instance,nullptr);return 12;}
 std::printf("DEVICE %s vendor=%04x device=%04x driver=%u api=%u.%u.%u\n",
  props.deviceName,props.vendorID,props.deviceID,props.driverVersion,
  VK_VERSION_MAJOR(props.apiVersion),VK_VERSION_MINOR(props.apiVersion),VK_VERSION_PATCH(props.apiVersion));
 float priority=1.0f;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
 qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
 VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
 VkDevice dev=VK_NULL_HANDLE;if(vkCreateDevice(phys,&dci,nullptr,&dev)!=VK_SUCCESS){vkDestroyInstance(instance,nullptr);return 13;}
 VkQueue queue=VK_NULL_HANDLE;vkGetDeviceQueue(dev,family,0,&queue);
 constexpr uint32_t W=320,H=180;
 TestImage prev{},curr{},out{};
 const VkFormat fmt=VK_FORMAT_R8G8B8A8_UNORM;
 if(!make_image(phys,dev,W,H,fmt,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,prev)||
    !make_image(phys,dev,W,H,fmt,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,curr)||
    !make_image(phys,dev,W,H,fmt,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,out))return 14;
 VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pci.queueFamilyIndex=family;
 VkCommandPool pool=VK_NULL_HANDLE;if(vkCreateCommandPool(dev,&pci,nullptr,&pool)!=VK_SUCCESS)return 15;
 VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cai.commandPool=pool;
 cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
 VkCommandBuffer cmd=VK_NULL_HANDLE;if(vkAllocateCommandBuffers(dev,&cai,&cmd)!=VK_SUCCESS)return 16;
 VkBuffer readback=VK_NULL_HANDLE;VkDeviceMemory readback_mem=VK_NULL_HANDLE;
 if(!make_readback(phys,dev,W*H*4,readback,readback_mem))return 17;
 VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
 if(vkBeginCommandBuffer(cmd,&begin)!=VK_SUCCESS)return 18;
 image_barrier(cmd,prev.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,
  VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
 image_barrier(cmd,curr.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,
  VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
 VkClearColorValue a{{0.1f,0.2f,0.3f,1.0f}},b{{0.7f,0.6f,0.5f,1.0f}};
 VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
 vkCmdClearColorImage(cmd,prev.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&a,1,&range);
 vkCmdClearColorImage(cmd,curr.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&b,1,&range);
 image_barrier(cmd,prev.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,
  VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
 image_barrier(cmd,curr.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,
  VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
 image_barrier(cmd,out.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,
  VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

 AromaFGVulkanContextV2 context{};context.struct_size=sizeof(context);
 context.api_version=AROMA_FG_VULKAN_INTEROP_VERSION;context.instance=instance;
 context.physical_device=phys;context.device=dev;context.queue=queue;context.queue_family_index=family;
 AromaFGVulkanBackendCreateInfoV1 create{};create.struct_size=sizeof(create);
 create.api_version=AROMA_FG_VULKAN_INTEROP_VERSION;create.context=context;create.in_flight_slots=2;
 AromaFGBackendV1 backend{};if(aroma_fg_vulkan_backend_create(&create,&backend)!=AROMA_FG_OK)return 19;
 AromaFGConfig config{};aroma_fg_default_config(&config);config.enabled=1;config.multiplier=2;
 AromaFGEngine* engine=aroma_fg_create(&config,&backend);if(!engine)return 20;

 VkImage outputs[1]{out.image};VkImageView output_views[1]{out.view};
 VkImageLayout output_layouts[1]{VK_IMAGE_LAYOUT_GENERAL};
 VkImageLayout output_restore[1]{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
 AromaFGVulkanFrameV2 frame{};frame.struct_size=sizeof(frame);
 frame.api_version=AROMA_FG_VULKAN_INTEROP_VERSION;frame.command_buffer=cmd;
 frame.previous_image=prev.image;frame.current_image=curr.image;
 frame.previous_view=prev.view;frame.current_view=curr.view;frame.source_format=fmt;
 frame.previous_layout=VK_IMAGE_LAYOUT_GENERAL;frame.current_layout=VK_IMAGE_LAYOUT_GENERAL;
 frame.previous_stage=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;frame.current_stage=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
 frame.previous_access=VK_ACCESS_SHADER_READ_BIT;frame.current_access=VK_ACCESS_SHADER_READ_BIT;
 frame.previous_restore_layout=VK_IMAGE_LAYOUT_GENERAL;frame.current_restore_layout=VK_IMAGE_LAYOUT_GENERAL;
 frame.previous_restore_stage=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;frame.current_restore_stage=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
 frame.previous_restore_access=VK_ACCESS_SHADER_READ_BIT;frame.current_restore_access=VK_ACCESS_SHADER_READ_BIT;
 frame.output_images=outputs;frame.output_views=output_views;frame.output_layouts=output_layouts;
 frame.output_restore_layouts=output_restore;frame.output_capacity=1;frame.output_format=fmt;
 frame.output_stage=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;frame.output_access=VK_ACCESS_SHADER_WRITE_BIT;
 frame.output_restore_stage=VK_PIPELINE_STAGE_TRANSFER_BIT;frame.output_restore_access=VK_ACCESS_TRANSFER_READ_BIT;
 frame.extent={W,H};frame.frame_slot=0;frame.flags=AROMA_FG_VULKAN_FRAME_HOST_QUIESCENT;
 AromaFGFrameInput input{};input.struct_size=sizeof(input);input.frame_id=1;input.timestamp_ns=1;
 input.width=W;input.height=H;input.format=static_cast<uint32_t>(fmt);
 input.host_frame=reinterpret_cast<uintptr_t>(&frame);
 AromaFGFrameOutput output{};output.struct_size=sizeof(output);
 const AromaFGStatus status=aroma_fg_process(engine,&input,&output);
 if(status!=AROMA_FG_OK||output.generated_frames!=1){std::printf("PROCESS_FAIL status=%d generated=%u\n",status,output.generated_frames);return 21;}
 VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
 copy.imageExtent={W,H,1};
 vkCmdCopyImageToBuffer(cmd,out.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
 VkBufferMemoryBarrier host_read{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
 host_read.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host_read.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
 host_read.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host_read.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
 host_read.buffer=readback;host_read.offset=0;host_read.size=VK_WHOLE_SIZE;
 vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host_read,0,nullptr);
 if(vkEndCommandBuffer(cmd)!=VK_SUCCESS)return 22;
 VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence=VK_NULL_HANDLE;
 if(vkCreateFence(dev,&fci,nullptr,&fence)!=VK_SUCCESS)return 23;
 VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
 if(vkQueueSubmit(queue,1,&submit,fence)!=VK_SUCCESS)return 24;
 VkResult wait=vkWaitForFences(dev,1,&fence,VK_TRUE,UINT64_MAX);
 if(wait!=VK_SUCCESS)return 25;
 AromaFGVulkanTimingV1 timing{};timing.struct_size=sizeof(timing);timing.api_version=AROMA_FG_VULKAN_INTEROP_VERSION;
 const AromaFGStatus timing_status=aroma_fg_vulkan_backend_collect_timing(&backend,0,&timing);
 void* mapped=nullptr;if(vkMapMemory(dev,readback_mem,0,W*H*4,0,&mapped)!=VK_SUCCESS)return 26;
 const auto* px=static_cast<const unsigned char*>(mapped);
 const unsigned pixel_r=px[0],pixel_g=px[1],pixel_b=px[2];
 const int expected=102;
 const bool pixel_ok=std::abs(int(pixel_r)-expected)<24&&std::abs(int(pixel_g)-expected)<24&&std::abs(int(pixel_b)-expected)<24;
 vkUnmapMemory(dev,readback_mem);
 std::printf("FG status=%s generated=%u timing_status=%d preprocess_ms=%.3f flow_ms=%.3f synth_ms=%.3f pixel=%u,%u,%u\n",
  pixel_ok?"PASS":"FAIL",output.generated_frames,timing_status,timing.preprocess_gpu_ms,timing.flow_gpu_ms,timing.synthesis_gpu_ms,
  pixel_r,pixel_g,pixel_b);
 aroma_fg_destroy(engine);
 vkDestroyFence(dev,fence,nullptr);
 vkDestroyBuffer(dev,readback,nullptr);vkFreeMemory(dev,readback_mem,nullptr);
 destroy_image(dev,prev);destroy_image(dev,curr);destroy_image(dev,out);
 vkDestroyCommandPool(dev,pool,nullptr);vkDestroyDevice(dev,nullptr);vkDestroyInstance(instance,nullptr);
 return pixel_ok?0:27;
}
