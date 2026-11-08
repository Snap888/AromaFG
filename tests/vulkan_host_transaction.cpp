#include "aroma_fg.h"
#include <vulkan/vulkan.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
struct HostPayload { VkCommandBuffer command_buffer; VkBuffer buffer; };
struct BackendState { uint32_t process=0, commits=0, cancels=0; uint64_t last_id=0; };
AromaFGStatus process_frame(void* user, const AromaFGFrameInput* input, AromaFGFrameOutput* output) {
    auto& state=*static_cast<BackendState*>(user);
    auto* host=reinterpret_cast<HostPayload*>(input->host_frame);
    if (!host || !host->command_buffer || !host->buffer) return AROMA_FG_ERROR_INVALID_ARGUMENT;
    vkCmdFillBuffer(host->command_buffer,host->buffer,0,VK_WHOLE_SIZE,static_cast<uint32_t>(input->frame_id));
    ++state.process;
    state.last_id=input->frame_id;
    output->generated_frames=1;
    return AROMA_FG_OK;
}
AromaFGStatus commit_frame(void* user,uint64_t id) {
    auto& s=*static_cast<BackendState*>(user);
    if (s.last_id!=id) return AROMA_FG_ERROR_FRAME_ORDER;
    ++s.commits;
    return AROMA_FG_OK;
}
AromaFGStatus cancel_frame(void* user,uint64_t id) {
    auto& s=*static_cast<BackendState*>(user);
    if (s.last_id!=id) return AROMA_FG_ERROR_FRAME_ORDER;
    ++s.cancels;
    return AROMA_FG_OK;
}
uint32_t find_memory(VkPhysicalDevice phys,uint32_t bits,VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p{}; vkGetPhysicalDeviceMemoryProperties(phys,&p);
    for(uint32_t i=0;i<p.memoryTypeCount;++i)
        if((bits&(1u<<i)) && (p.memoryTypes[i].propertyFlags&flags)==flags) return i;
    return UINT32_MAX;
}
}

int main() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="AromaFG host transaction test"; app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app;
    VkInstance instance=VK_NULL_HANDLE;
    if(vkCreateInstance(&ici,nullptr,&instance)!=VK_SUCCESS) return 10;

    uint32_t count=0; vkEnumeratePhysicalDevices(instance,&count,nullptr);
    if(!count) { vkDestroyInstance(instance,nullptr); return 11; }
    std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance,&count,devices.data());
    VkPhysicalDevice physical=VK_NULL_HANDLE; uint32_t family=UINT32_MAX;
    for(auto d:devices) {
        uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(d,&n,nullptr);
        std::vector<VkQueueFamilyProperties> q(n); vkGetPhysicalDeviceQueueFamilyProperties(d,&n,q.data());
        for(uint32_t i=0;i<n;++i) if(q[i].queueFlags&VK_QUEUE_COMPUTE_BIT) { physical=d; family=i; break; }
        if(physical) break;
    }
    if(!physical) { vkDestroyInstance(instance,nullptr); return 12; }

    float priority=1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex=family; qci.queueCount=1; qci.pQueuePriorities=&priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci;
    VkDevice device=VK_NULL_HANDLE;
    if(vkCreateDevice(physical,&dci,nullptr,&device)!=VK_SUCCESS) { vkDestroyInstance(instance,nullptr); return 13; }
    VkQueue queue=VK_NULL_HANDLE; vkGetDeviceQueue(device,family,0,&queue);

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size=256; bci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT; bci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer=VK_NULL_HANDLE;
    if(vkCreateBuffer(device,&bci,nullptr,&buffer)!=VK_SUCCESS) return 14;
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(device,buffer,&mr);
    uint32_t mt=find_memory(physical,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(mt==UINT32_MAX) mt=find_memory(physical,mr.memoryTypeBits,0);
    if(mt==UINT32_MAX) return 15;
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    if(vkAllocateMemory(device,&mai,nullptr,&memory)!=VK_SUCCESS) return 16;
    if(vkBindBufferMemory(device,buffer,memory,0)!=VK_SUCCESS) return 17;

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pci.queueFamilyIndex=family;
    VkCommandPool pool=VK_NULL_HANDLE;
    if(vkCreateCommandPool(device,&pci,nullptr,&pool)!=VK_SUCCESS) return 18;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool=pool; cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount=1;
    VkCommandBuffer cmd=VK_NULL_HANDLE;
    if(vkAllocateCommandBuffers(device,&cai,&cmd)!=VK_SUCCESS) return 19;
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence=VK_NULL_HANDLE;
    if(vkCreateFence(device,&fci,nullptr,&fence)!=VK_SUCCESS) return 20;

    BackendState state{};
    AromaFGBackendV1 backend{};
    backend.struct_size=sizeof(backend); backend.api_version=AROMA_FG_API_VERSION;
    backend.user_data=&state; backend.process_frame=process_frame;
    backend.commit_frame=commit_frame; backend.cancel_frame=cancel_frame;
    AromaFGConfig config{}; aroma_fg_default_config(&config); config.enabled=1;
    AromaFGEngine* engine=aroma_fg_create(&config,&backend);
    if(!engine) return 21;

    HostPayload payload{cmd,buffer};
    AromaFGFrameInput input{}; input.struct_size=sizeof(input); input.frame_id=1;
    input.width=64; input.height=64; input.host_frame=reinterpret_cast<uintptr_t>(&payload);
    AromaFGFrameOutput output{}; output.struct_size=sizeof(output);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    // Record real Vulkan work, then abandon/reset it before cancel.
    assert(vkBeginCommandBuffer(cmd,&begin)==VK_SUCCESS);
    assert(aroma_fg_process(engine,&input,&output)==AROMA_FG_OK);
    assert(vkEndCommandBuffer(cmd)==VK_SUCCESS);
    assert(vkResetCommandPool(device,pool,0)==VK_SUCCESS);
    assert(aroma_fg_cancel_frame(engine,1)==AROMA_FG_OK);
    assert(state.cancels==1 && state.commits==0);

    // Retry the exact same frame ID; submit first, then commit.
    assert(vkBeginCommandBuffer(cmd,&begin)==VK_SUCCESS);
    output={}; output.struct_size=sizeof(output);
    assert(aroma_fg_process(engine,&input,&output)==AROMA_FG_OK);
    assert(vkEndCommandBuffer(cmd)==VK_SUCCESS);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
    assert(vkQueueSubmit(queue,1,&submit,fence)==VK_SUCCESS);
    assert(aroma_fg_commit_frame(engine,1)==AROMA_FG_OK);
    assert(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX)==VK_SUCCESS);
    assert(state.commits==1);

    // Fence completion permits command-buffer/frame-slot reuse for the next frame.
    assert(vkResetFences(device,1,&fence)==VK_SUCCESS);
    assert(vkResetCommandPool(device,pool,0)==VK_SUCCESS);
    input.frame_id=2; output={}; output.struct_size=sizeof(output);
    assert(vkBeginCommandBuffer(cmd,&begin)==VK_SUCCESS);
    assert(aroma_fg_process(engine,&input,&output)==AROMA_FG_OK);
    assert(vkEndCommandBuffer(cmd)==VK_SUCCESS);
    assert(vkQueueSubmit(queue,1,&submit,fence)==VK_SUCCESS);
    assert(aroma_fg_commit_frame(engine,2)==AROMA_FG_OK);
    assert(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX)==VK_SUCCESS);
    assert(state.process==3 && state.commits==2 && state.cancels==1);

    aroma_fg_destroy(engine);
    vkDestroyFence(device,fence,nullptr);
    vkDestroyCommandPool(device,pool,nullptr);
    vkDestroyBuffer(device,buffer,nullptr); vkFreeMemory(device,memory,nullptr);
    vkDestroyDevice(device,nullptr); vkDestroyInstance(instance,nullptr);
    std::puts("HOST_TRANSACTION_TEST=PASS abandon/cancel/retry/submit/commit/fence/slot-reuse");
    return 0;
}
