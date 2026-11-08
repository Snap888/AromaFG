#include "vulkan/vulkan_backend.hpp"
#include "aroma_fg_vulkan.h"
#include <cassert>
#include <type_traits>
#include <cmath>
using namespace aromafg::vk;
template<class Handle> Handle fake_handle(uintptr_t value) {
 if constexpr (std::is_pointer_v<Handle>) return reinterpret_cast<Handle>(value);
 else return static_cast<Handle>(value);
}
int main() {
 AromaFGVulkanContextV1 context{};
 assert(aroma_fg_vulkan_validate_context(&context)==0);
 VkImage out_images[2]{fake_handle<VkImage>(1),fake_handle<VkImage>(2)};
 VkImageView out_views[2]{fake_handle<VkImageView>(3),fake_handle<VkImageView>(4)};
 VkImageLayout out_layouts[2]{VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL};
 AromaFGVulkanFrameV1 frame{};
 frame.struct_size=sizeof(frame);frame.api_version=AROMA_FG_VULKAN_INTEROP_VERSION;
 frame.command_buffer=fake_handle<VkCommandBuffer>(5);
 frame.source_image=fake_handle<VkImage>(6);frame.source_view=fake_handle<VkImageView>(7);
 frame.source_format=VK_FORMAT_R8G8B8A8_UNORM;frame.source_layout=VK_IMAGE_LAYOUT_GENERAL;
 frame.output_images=out_images;frame.output_views=out_views;frame.output_layouts=out_layouts;
 frame.output_capacity=2;frame.output_format=VK_FORMAT_R8G8B8A8_UNORM;frame.extent={1280,720};
 assert(aroma_fg_vulkan_validate_frame(&frame,2)==1);
 assert(aroma_fg_vulkan_validate_frame(&frame,3)==0);
 frame.source_view=VK_NULL_HANDLE;
 assert(aroma_fg_vulkan_validate_frame(&frame,2)==0);
 const auto levels=make_pyramid(1919,1079,4,32);
 assert(levels.size()==4);
 assert(levels[0].width==1919 && levels[0].height==1079);
 assert(levels[1].width==960 && levels[1].height==540);
 assert(levels[2].width==480 && levels[2].height==270);
 assert(levels[3].width==240 && levels[3].height==135);
 assert(make_pyramid(0,100,4).empty());
 assert(make_pyramid(100,100,0).empty());
 for(uint32_t m=2;m<=4;++m) {
   const auto graph=make_flow_schedule(3,m);
   assert(!graph.empty());
   uint32_t synth=0,forward=0,backward=0,up=0,luma=0,downsample=0;
   float expected=0.0f;
   for(const auto& pass:graph) {
     if(pass.kind==FlowPassKind::Luma)++luma;
     if(pass.kind==FlowPassKind::Downsample)++downsample;
     if(pass.kind==FlowPassKind::Synthesize) {
       ++synth; expected+=1.0f/static_cast<float>(m);
       assert(std::fabs(pass.phase-expected)<1.0e-5f);
     }
     if(pass.kind==FlowPassKind::SearchForward)++forward;
     if(pass.kind==FlowPassKind::SearchBackward)++backward;
     if(pass.kind==FlowPassKind::UpsampleFlow)++up;
   }
   assert(synth==m-1 && forward==3 && backward==3 && up==4 && luma==2 && downsample==4);
 }
 assert(make_flow_schedule(0,2).empty());
 assert(make_flow_schedule(3,1).empty());
 assert(make_flow_schedule(3,5).empty());
 return 0;
}
