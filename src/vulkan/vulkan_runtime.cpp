#include "vulkan_backend.hpp"
#include "aroma_fg_vulkan.h"
#include "embedded_shaders.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace aromafg::vk {
namespace {

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
};

struct Pipeline {
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
};

struct Slot {
    VkDescriptorPool descriptors = VK_NULL_HANDLE;
    VkQueryPool timestamps = VK_NULL_HANDLE;
    bool has_timestamps = false;
    uint64_t completed_frame = 0;
};

struct Resources {
    VkExtent2D extent{};
    std::vector<PyramidLevel> levels;
    std::vector<Image> previous_luma, current_luma;
    std::vector<Image> forward_flow, backward_flow;
    std::vector<Image> forward_confidence, backward_confidence;
    std::vector<Image> forward_seed, backward_seed;
    Image zero_flow;
    Image refined_forward, refined_backward;
    Image refined_forward_confidence, refined_backward_confidence;
    bool initialized = false;
};

struct Push2D { uint32_t width, height; };
struct PushSearch {
    uint32_t width, height;
    int32_t search_radius, search_step;
    uint32_t use_seed;
    float seed_scale;
};
struct PushUpsample { uint32_t width, height; float scale_x, scale_y; };
struct PushRefine { uint32_t width, height; float consistency_scale; };
struct PushSynthesis { uint32_t width, height; float phase, confidence_floor; };

class Runtime {
public:
    explicit Runtime(const AromaFGVulkanContextV2& c, uint32_t slots)
        : context_(c), slots_(std::clamp(slots, 2u, 4u)) {}
    ~Runtime() { destroy(); }

    bool initialize(const AromaFGConfig& config, const void* initial_cache, size_t initial_cache_size) {
        config_ = config;
        if (!validate_context({context_.instance, context_.physical_device,
                               context_.device, context_.queue,
                               context_.queue_family_index})) return false;

        vkGetPhysicalDeviceProperties(context_.physical_device, &properties_);
        vkGetPhysicalDeviceMemoryProperties(context_.physical_device, &memory_properties_);
        uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context_.physical_device, &family_count, nullptr);
        if (context_.queue_family_index >= family_count) return false;
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(context_.physical_device, &family_count, families.data());
        timestamp_bits_ = families[context_.queue_family_index].timestampValidBits;
        timestamp_period_ = properties_.limits.timestampPeriod;

        if (properties_.limits.maxComputeWorkGroupInvocations < 64 ||
            properties_.limits.maxComputeWorkGroupSize[0] < 8 ||
            properties_.limits.maxComputeWorkGroupSize[1] < 8 ||
            properties_.limits.maxPushConstantsSize < sizeof(PushSearch) ||
            !format_support(VK_FORMAT_R16_SFLOAT, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ||
            !format_support(VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ||
            !format_support(VK_FORMAT_R16_SFLOAT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ||
            !format_support(VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) return false;

        VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.maxLod = 0.0f;
        if (vkCreateSampler(context_.device, &sampler_info, nullptr, &sampler_) != VK_SUCCESS)
            return false;

        if (cache_.create(context_.device, initial_cache, initial_cache_size) != VK_SUCCESS) return false;
        if (!create_pipeline(luma_, embedded::luma_comp, embedded::luma_comp_bytes,
                             {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, sizeof(Push2D)) ||
            !create_pipeline(downsample_, embedded::downsample_comp, embedded::downsample_comp_bytes,
                             {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, sizeof(Push2D)) ||
            !create_pipeline(search_, embedded::search_comp, embedded::search_comp_bytes,
                             {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, sizeof(PushSearch)) ||
            !create_pipeline(upsample_, embedded::upsample_comp, embedded::upsample_comp_bytes,
                             {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, sizeof(PushUpsample)) ||
            !create_pipeline(refine_, embedded::refine_comp, embedded::refine_comp_bytes,
                             {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, sizeof(PushRefine)) ||
            !create_pipeline(interp8_, embedded::interp_rgba8_comp, embedded::interp_rgba8_comp_bytes,
                             interp_bindings(), sizeof(PushSynthesis)) ||
            !create_pipeline(interp16_, embedded::interp_comp, embedded::interp_comp_bytes,
                             interp_bindings(), sizeof(PushSynthesis))) return false;

        slots_data_.resize(slots_);
        for (auto& slot : slots_data_) {
            if (!create_slot(slot)) return false;
        }
        return true;
    }

    void update_config(const AromaFGConfig& c) { config_ = c; }

    AromaFGStatus record(const AromaFGFrameInput& input, const AromaFGVulkanFrameV2& frame,
                         AromaFGFrameOutput& output) {
        const uint32_t required = config_.multiplier - 1u;
        if (!aroma_fg_vulkan_validate_frame(&frame, required) ||
            frame.frame_slot >= slots_ || frame.extent.width != input.width ||
            frame.extent.height != input.height ||
            frame.source_format == VK_FORMAT_UNDEFINED ||
            (frame.output_format != VK_FORMAT_R8G8B8A8_UNORM &&
             frame.output_format != VK_FORMAT_R16G16B16A16_SFLOAT))
            return AROMA_FG_ERROR_INVALID_ARGUMENT;
        if (!format_support(frame.source_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ||
            !format_support(frame.output_format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
            return AROMA_FG_ERROR_UNSUPPORTED;
        if (!workgroup_extent_supported(frame.extent)) return AROMA_FG_ERROR_UNSUPPORTED;

        Slot& slot = slots_data_[frame.frame_slot];
        read_timing(slot, output);
        if (!ensure_resources(frame.extent, (frame.flags & AROMA_FG_VULKAN_FRAME_HOST_QUIESCENT) != 0))
            return AROMA_FG_ERROR_BUSY;
        if (vkResetDescriptorPool(context_.device, slot.descriptors, 0) != VK_SUCCESS)
            return AROMA_FG_ERROR_BACKEND;

        VkCommandBuffer cmd = frame.command_buffer;
        if (config_.profiling_enabled && slot.timestamps)
            vkCmdResetQueryPool(cmd, slot.timestamps, 0, 6);

        prepare_resources(cmd);
        transition_input(cmd, frame.previous_image, frame.previous_layout,
                         frame.previous_stage, frame.previous_access,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        transition_input(cmd, frame.current_image, frame.current_layout,
                         frame.current_stage, frame.current_access,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        if (config_.profiling_enabled && slot.timestamps)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, slot.timestamps, 0);

        if (!record_luma(cmd, slot, frame.previous_view, resources_.previous_luma[0].view) ||
            !record_luma(cmd, slot, frame.current_view, resources_.current_luma[0].view))
            return AROMA_FG_ERROR_BACKEND;
        barrier(cmd, resources_.previous_luma[0].image);
        barrier(cmd, resources_.current_luma[0].image);

        if (!record_pyramid(cmd, slot)) return AROMA_FG_ERROR_BACKEND;
        if (config_.profiling_enabled && slot.timestamps)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, slot.timestamps, 1);

        if (config_.profiling_enabled && slot.timestamps)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, slot.timestamps, 2);
        if (!record_flow(cmd, slot)) return AROMA_FG_ERROR_BACKEND;
        if (config_.profiling_enabled && slot.timestamps)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, slot.timestamps, 3);

        Pipeline& synthesis = frame.output_format == VK_FORMAT_R8G8B8A8_UNORM ? interp8_ : interp16_;
        if (config_.profiling_enabled && slot.timestamps)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, slot.timestamps, 4);

        for (uint32_t i = 0; i < required; ++i) {
            transition_output(cmd, frame.output_images[i], frame.output_layouts[i],
                              frame.output_stage, frame.output_access, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorSet set = allocate_set(slot, synthesis);
            if (!set) return AROMA_FG_ERROR_BACKEND;
            write_sampled(set, 0, frame.previous_view);
            write_sampled(set, 1, frame.current_view);
            write_storage(set, 2, resources_.refined_forward.view);
            write_storage(set, 3, resources_.refined_backward.view);
            write_storage(set, 4, resources_.refined_forward_confidence.view);
            write_storage(set, 5, resources_.refined_backward_confidence.view);
            write_storage(set, 6, frame.output_views[i]);
            const PushSynthesis push{frame.extent.width, frame.extent.height,
                static_cast<float>(i + 1u) / static_cast<float>(config_.multiplier), 0.025f};
            bind_dispatch(cmd, synthesis, set, &push, sizeof(push), frame.extent);
            image_barrier(cmd, frame.output_images[i], VK_ACCESS_SHADER_WRITE_BIT,
                    frame.output_restore_access, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    nonzero_stage(frame.output_restore_stage), VK_IMAGE_LAYOUT_GENERAL,
                    frame.output_restore_layouts[i]);
        }
        image_barrier(cmd, frame.previous_image, VK_ACCESS_SHADER_READ_BIT, frame.previous_restore_access,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nonzero_stage(frame.previous_restore_stage),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frame.previous_restore_layout);
        image_barrier(cmd, frame.current_image, VK_ACCESS_SHADER_READ_BIT, frame.current_restore_access,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nonzero_stage(frame.current_restore_stage),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frame.current_restore_layout);
        if (config_.profiling_enabled && slot.timestamps)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, slot.timestamps, 5);
        slot.has_timestamps = config_.profiling_enabled && slot.timestamps;
        slot.completed_frame = input.frame_id;
        output.generated_frames = required;
        return AROMA_FG_OK;
    }

    VkResult export_cache(std::vector<uint8_t>& out) const { return cache_.export_data(out); }

    AromaFGStatus collect(uint32_t index, AromaFGVulkanTimingV1& out) {
        if (index >= slots_) return AROMA_FG_ERROR_INVALID_ARGUMENT;
        Slot& slot = slots_data_[index];
        if (!slot.has_timestamps || !slot.timestamps) return AROMA_FG_BYPASSED;
        uint64_t q[6]{};
        VkResult r = vkGetQueryPoolResults(context_.device, slot.timestamps, 0, 6,
            sizeof(q), q, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (r == VK_NOT_READY) return AROMA_FG_ERROR_BUSY;
        if (r != VK_SUCCESS) return AROMA_FG_ERROR_BACKEND;
        const uint64_t mask = timestamp_bits_ >= 64 ? ~uint64_t(0)
            : ((uint64_t(1) << timestamp_bits_) - 1u);
        auto elapsed = [&](uint64_t a, uint64_t b) {
            const uint64_t ticks = (b - a) & mask;
            return static_cast<float>((static_cast<double>(ticks) *
                static_cast<double>(timestamp_period_)) / 1000000.0);
        };
        out.preprocess_gpu_ms = elapsed(q[0], q[1]);
        out.flow_gpu_ms = elapsed(q[2], q[3]);
        out.synthesis_gpu_ms = elapsed(q[4], q[5]);
        out.completed_frame_id = slot.completed_frame;
        slot.has_timestamps = false;
        return AROMA_FG_OK;
    }

    void destroy() {
        if (context_.device == VK_NULL_HANDLE) return;
        for (auto& slot : slots_data_) {
            if (slot.timestamps) vkDestroyQueryPool(context_.device, slot.timestamps, nullptr);
            if (slot.descriptors) vkDestroyDescriptorPool(context_.device, slot.descriptors, nullptr);
            slot = Slot{};
        }
        slots_data_.clear();
        destroy_resources();
        destroy_pipeline(interp16_);
        destroy_pipeline(interp8_);
        destroy_pipeline(refine_);
        destroy_pipeline(upsample_);
        destroy_pipeline(search_);
        destroy_pipeline(downsample_);
        destroy_pipeline(luma_);
        cache_.reset();
        if (sampler_) vkDestroySampler(context_.device, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
        context_.device = VK_NULL_HANDLE;
    }

private:
    static std::vector<std::pair<uint32_t, VkDescriptorType>> interp_bindings() {
        return {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}};
    }

    bool format_support(VkFormat format, VkFormatFeatureFlags features) const {
        VkFormatProperties p{};
        vkGetPhysicalDeviceFormatProperties(context_.physical_device, format, &p);
        return (p.optimalTilingFeatures & features) == features;
    }

    bool workgroup_extent_supported(VkExtent2D e) const {
        const uint32_t gx = (e.width + 7u) / 8u, gy = (e.height + 7u) / 8u;
        return gx <= properties_.limits.maxComputeWorkGroupCount[0] &&
               gy <= properties_.limits.maxComputeWorkGroupCount[1];
    }

    bool create_pipeline(Pipeline& p, const uint32_t* code, size_t bytes,
                         const std::vector<std::pair<uint32_t, VkDescriptorType>>& bindings,
                         uint32_t push_size) {
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(bindings.size());
        for (const auto& b : bindings) {
            VkDescriptorSetLayoutBinding item{};
            item.binding = b.first;
            item.descriptorType = b.second;
            item.descriptorCount = 1;
            item.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_bindings.push_back(item);
        }
        VkDescriptorSetLayoutCreateInfo set_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        set_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
        set_info.pBindings = layout_bindings.data();
        if (vkCreateDescriptorSetLayout(context_.device, &set_info, nullptr, &p.set_layout) != VK_SUCCESS)
            return false;

        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset = 0;
        range.size = push_size;
        VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &p.set_layout;
        layout_info.pushConstantRangeCount = push_size ? 1u : 0u;
        layout_info.pPushConstantRanges = push_size ? &range : nullptr;
        if (vkCreatePipelineLayout(context_.device, &layout_info, nullptr, &p.layout) != VK_SUCCESS)
            return false;

        VkShaderModuleCreateInfo module_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module_info.codeSize = bytes;
        module_info.pCode = code;
        if (vkCreateShaderModule(context_.device, &module_info, nullptr, &p.module) != VK_SUCCESS)
            return false;

        VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = p.module;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = p.layout;
        return vkCreateComputePipelines(context_.device, cache_.get(), 1, &pipeline_info,
                                        nullptr, &p.pipeline) == VK_SUCCESS;
    }

    void destroy_pipeline(Pipeline& p) {
        if (p.pipeline) vkDestroyPipeline(context_.device, p.pipeline, nullptr);
        if (p.layout) vkDestroyPipelineLayout(context_.device, p.layout, nullptr);
        if (p.set_layout) vkDestroyDescriptorSetLayout(context_.device, p.set_layout, nullptr);
        if (p.module) vkDestroyShaderModule(context_.device, p.module, nullptr);
        p = Pipeline{};
    }

    bool create_slot(Slot& slot) {
        const uint32_t level_count = 8;
        const uint32_t max_sets = 6u * level_count + AROMA_FG_MAX_MULTIPLIER - 1u;
        const uint32_t storage_count = 18u * level_count + 5u * AROMA_FG_MAX_MULTIPLIER - 1u;
        const uint32_t combined_count = 2u * AROMA_FG_MAX_MULTIPLIER;
        VkDescriptorPoolSize sizes[2] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_count},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, combined_count}
        };
        VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.maxSets = max_sets;
        pool_info.poolSizeCount = 2;
        pool_info.pPoolSizes = sizes;
        if (vkCreateDescriptorPool(context_.device, &pool_info, nullptr, &slot.descriptors) != VK_SUCCESS)
            return false;
        if (timestamp_bits_) {
            VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            query_info.queryCount = 6;
            if (vkCreateQueryPool(context_.device, &query_info, nullptr, &slot.timestamps) != VK_SUCCESS)
                slot.timestamps = VK_NULL_HANDLE;
        }
        return true;
    }

    uint32_t memory_type(uint32_t bits) const {
        uint32_t fallback = UINT32_MAX;
        for (uint32_t i = 0; i < memory_properties_.memoryTypeCount; ++i) {
            if (!(bits & (1u << i))) continue;
            if (memory_properties_.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                return i;
            if (fallback == UINT32_MAX) fallback = i;
        }
        return fallback;
    }

    bool create_image(Image& out, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage) {
        out.format = format;
        out.extent = extent;
        VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = format;
        image_info.extent = {extent.width, extent.height, 1};
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = usage;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(context_.device, &image_info, nullptr, &out.image) != VK_SUCCESS)
            return false;
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(context_.device, out.image, &requirements);
        const uint32_t type = memory_type(requirements.memoryTypeBits);
        if (type == UINT32_MAX) return false;
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = type;
        if (vkAllocateMemory(context_.device, &allocation, nullptr, &out.memory) != VK_SUCCESS)
            return false;
        if (vkBindImageMemory(context_.device, out.image, out.memory, 0) != VK_SUCCESS)
            return false;
        VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = out.image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        return vkCreateImageView(context_.device, &view_info, nullptr, &out.view) == VK_SUCCESS;
    }

    void destroy_image(Image& image) {
        if (image.view) vkDestroyImageView(context_.device, image.view, nullptr);
        if (image.image) vkDestroyImage(context_.device, image.image, nullptr);
        if (image.memory) vkFreeMemory(context_.device, image.memory, nullptr);
        image = Image{};
    }

    void destroy_resources() {
        for (auto& x : resources_.previous_luma) destroy_image(x);
        for (auto& x : resources_.current_luma) destroy_image(x);
        for (auto& x : resources_.forward_flow) destroy_image(x);
        for (auto& x : resources_.backward_flow) destroy_image(x);
        for (auto& x : resources_.forward_confidence) destroy_image(x);
        for (auto& x : resources_.backward_confidence) destroy_image(x);
        for (auto& x : resources_.forward_seed) destroy_image(x);
        for (auto& x : resources_.backward_seed) destroy_image(x);
        destroy_image(resources_.zero_flow);
        destroy_image(resources_.refined_forward);
        destroy_image(resources_.refined_backward);
        destroy_image(resources_.refined_forward_confidence);
        destroy_image(resources_.refined_backward_confidence);
        resources_ = Resources{};
    }

    bool ensure_resources(VkExtent2D extent, bool host_quiescent) {
        if (resources_.extent.width == extent.width && resources_.extent.height == extent.height &&
            !resources_.levels.empty()) return true;
        if (!resources_.levels.empty() && !host_quiescent) return false;
        destroy_resources();
        resources_.extent = extent;
        resources_.levels = make_pyramid(extent.width, extent.height, 8, 16);
        if (resources_.levels.size() < 2) return false;
        const VkImageUsageFlags storage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        auto make_levels = [&](std::vector<Image>& images, VkFormat format, VkImageUsageFlags usage) {
            images.resize(resources_.levels.size());
            for (size_t i = 0; i < resources_.levels.size(); ++i)
                if (!create_image(images[i], {resources_.levels[i].width, resources_.levels[i].height},
                                  format, usage)) return false;
            return true;
        };
        if (!make_levels(resources_.previous_luma, VK_FORMAT_R16_SFLOAT, storage) ||
            !make_levels(resources_.current_luma, VK_FORMAT_R16_SFLOAT, storage) ||
            !make_levels(resources_.forward_flow, VK_FORMAT_R16G16_SFLOAT, storage) ||
            !make_levels(resources_.backward_flow, VK_FORMAT_R16G16_SFLOAT, storage) ||
            !make_levels(resources_.forward_confidence, VK_FORMAT_R16_SFLOAT, storage) ||
            !make_levels(resources_.backward_confidence, VK_FORMAT_R16_SFLOAT, storage) ||
            !make_levels(resources_.forward_seed, VK_FORMAT_R16G16_SFLOAT, storage) ||
            !make_levels(resources_.backward_seed, VK_FORMAT_R16G16_SFLOAT, storage) ||
            !create_image(resources_.zero_flow, resources_.levels.back().width ?
                VkExtent2D{resources_.levels.back().width, resources_.levels.back().height} : VkExtent2D{},
                VK_FORMAT_R16G16_SFLOAT, storage | VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
            !create_image(resources_.refined_forward, extent, VK_FORMAT_R16G16_SFLOAT, storage) ||
            !create_image(resources_.refined_backward, extent, VK_FORMAT_R16G16_SFLOAT, storage) ||
            !create_image(resources_.refined_forward_confidence, extent, VK_FORMAT_R16_SFLOAT, storage) ||
            !create_image(resources_.refined_backward_confidence, extent, VK_FORMAT_R16_SFLOAT, storage)) {
            destroy_resources();
            return false;
        }
        return true;
    }

    template<class F> void for_each_image(F&& fn) {
        auto visit = [&](std::vector<Image>& v) { for (auto& x : v) fn(x); };
        visit(resources_.previous_luma); visit(resources_.current_luma);
        visit(resources_.forward_flow); visit(resources_.backward_flow);
        visit(resources_.forward_confidence); visit(resources_.backward_confidence);
        visit(resources_.forward_seed); visit(resources_.backward_seed);
        fn(resources_.zero_flow); fn(resources_.refined_forward); fn(resources_.refined_backward);
        fn(resources_.refined_forward_confidence); fn(resources_.refined_backward_confidence);
    }

    void image_barrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags src, VkAccessFlags dst,
                       VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
                       VkImageLayout old_layout = VK_IMAGE_LAYOUT_GENERAL,
                       VkImageLayout new_layout = VK_IMAGE_LAYOUT_GENERAL) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.srcAccessMask = src; b.dstAccessMask = dst;
        b.oldLayout = old_layout; b.newLayout = new_layout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    void barrier(VkCommandBuffer cmd, VkImage image,
                 VkAccessFlags src = VK_ACCESS_SHADER_WRITE_BIT,
                 VkAccessFlags dst = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                 VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                 VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
        image_barrier(cmd, image, src, dst, src_stage, dst_stage);
    }

    void prepare_resources(VkCommandBuffer cmd) {
        if (!resources_.initialized) {
            for_each_image([&](Image& x) {
                if (x.image) image_barrier(cmd, x.image, 0,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            });
            resources_.initialized = true;
        } else {
            for_each_image([&](Image& x) {
                if (x.image) barrier(cmd, x.image);
            });
        }
        VkClearColorValue zero{};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, resources_.zero_flow.image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
        barrier(cmd, resources_.zero_flow.image, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void transition_input(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout,
                          VkPipelineStageFlags src_stage, VkAccessFlags src_access,
                          VkImageLayout new_layout) {
        image_barrier(cmd, image, src_access, VK_ACCESS_SHADER_READ_BIT,
            nonzero_stage(src_stage), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            old_layout, new_layout);
    }

    void transition_output(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout,
                           VkPipelineStageFlags src_stage, VkAccessFlags src_access,
                           VkImageLayout new_layout) {
        image_barrier(cmd, image, src_access, VK_ACCESS_SHADER_WRITE_BIT,
            nonzero_stage(src_stage), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            old_layout, new_layout);
    }

    static VkPipelineStageFlags nonzero_stage(VkPipelineStageFlags stage) {
        return stage ? stage : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    VkDescriptorSet allocate_set(Slot& slot, const Pipeline& pipeline) {
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool = slot.descriptors;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &pipeline.set_layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        return vkAllocateDescriptorSets(context_.device, &info, &set) == VK_SUCCESS ? set : VK_NULL_HANDLE;
    }

    void write_storage(VkDescriptorSet set, uint32_t binding, VkImageView view) {
        VkDescriptorImageInfo image{}; image.imageView = view; image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set; write.dstBinding = binding; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; write.pImageInfo = &image;
        vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    }

    void write_sampled(VkDescriptorSet set, uint32_t binding, VkImageView view) {
        VkDescriptorImageInfo image{}; image.sampler = sampler_; image.imageView = view;
        image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set; write.dstBinding = binding; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &image;
        vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    }

    void bind_dispatch(VkCommandBuffer cmd, const Pipeline& pipeline, VkDescriptorSet set,
                       const void* push, uint32_t push_size, VkExtent2D extent) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout,
                                0, 1, &set, 0, nullptr);
        if (push_size) vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT,
                                          0, push_size, push);
        vkCmdDispatch(cmd, (extent.width + 7u) / 8u, (extent.height + 7u) / 8u, 1);
    }

    bool record_luma(VkCommandBuffer cmd, Slot& slot, VkImageView source, VkImageView destination) {
        VkDescriptorSet set = allocate_set(slot, luma_);
        if (!set) return false;
        write_sampled(set, 0, source);
        write_storage(set, 1, destination);
        const Push2D push{resources_.extent.width, resources_.extent.height};
        bind_dispatch(cmd, luma_, set, &push, sizeof(push), resources_.extent);
        return true;
    }

    bool record_pyramid(VkCommandBuffer cmd, Slot& slot) {
        for (size_t level = 1; level < resources_.levels.size(); ++level) {
            const VkExtent2D extent{resources_.levels[level].width, resources_.levels[level].height};
            for (int direction = 0; direction < 2; ++direction) {
                auto& source = direction == 0 ? resources_.previous_luma[level-1] : resources_.current_luma[level-1];
                auto& destination = direction == 0 ? resources_.previous_luma[level] : resources_.current_luma[level];
                VkDescriptorSet set = allocate_set(slot, downsample_);
                if (!set) return false;
                write_storage(set, 0, source.view);
                write_storage(set, 1, destination.view);
                const Push2D push{extent.width, extent.height};
                bind_dispatch(cmd, downsample_, set, &push, sizeof(push), extent);
                barrier(cmd, destination.image);
            }
        }
        return true;
    }

    bool record_flow(VkCommandBuffer cmd, Slot& slot) {
        const int radius = config_.quality == AROMA_FG_QUALITY_PERFORMANCE ? 2 :
                           config_.quality == AROMA_FG_QUALITY_QUALITY ? 4 :
                           config_.quality == AROMA_FG_QUALITY_ULTRA ? 5 : 3;
        for (int level = static_cast<int>(resources_.levels.size()) - 1; level >= 0; --level) {
            const VkExtent2D extent{resources_.levels[level].width, resources_.levels[level].height};
            for (int direction = 0; direction < 2; ++direction) {
                auto& flow = direction == 0 ? resources_.forward_flow[level] : resources_.backward_flow[level];
                auto& confidence = direction == 0 ? resources_.forward_confidence[level] : resources_.backward_confidence[level];
                auto& seed = direction == 0 ? resources_.forward_seed[level] : resources_.backward_seed[level];
                VkImageView seed_view = seed.view;
                if (level == static_cast<int>(resources_.levels.size()) - 1) {
                    // Coarsest seed is explicitly cleared to zero; do not alias the output image.
                    seed_view = resources_.zero_flow.view;
                } else {
                    const auto& coarse = direction == 0 ? resources_.forward_flow[level+1] : resources_.backward_flow[level+1];
                    VkDescriptorSet up = allocate_set(slot, upsample_);
                    if (!up) return false;
                    write_storage(up, 0, coarse.view);
                    write_storage(up, 1, seed.view);
                    const PushUpsample push{extent.width, extent.height,
                        static_cast<float>(resources_.levels[level+1].width) / extent.width,
                        static_cast<float>(resources_.levels[level+1].height) / extent.height};
                    bind_dispatch(cmd, upsample_, up, &push, sizeof(push), extent);
                    barrier(cmd, seed.image);
                }

                VkDescriptorSet set = allocate_set(slot, search_);
                if (!set) return false;
                const auto& image_a = direction == 0 ? resources_.previous_luma[level] : resources_.current_luma[level];
                const auto& image_b = direction == 0 ? resources_.current_luma[level] : resources_.previous_luma[level];
                write_storage(set, 0, image_a.view);
                write_storage(set, 1, image_b.view);
                write_storage(set, 2, seed_view);
                write_storage(set, 3, flow.view);
                write_storage(set, 4, confidence.view);
                const PushSearch push{extent.width, extent.height, radius, 1, 1u, 1.0f};
                bind_dispatch(cmd, search_, set, &push, sizeof(push), extent);
                barrier(cmd, flow.image);
                barrier(cmd, confidence.image);
            }
        }

        for (int direction = 0; direction < 2; ++direction) {
            auto& flow = direction == 0 ? resources_.forward_flow[0] : resources_.backward_flow[0];
            auto& opposite = direction == 0 ? resources_.backward_flow[0] : resources_.forward_flow[0];
            auto& confidence = direction == 0 ? resources_.forward_confidence[0] : resources_.backward_confidence[0];
            auto& refined = direction == 0 ? resources_.refined_forward : resources_.refined_backward;
            auto& refined_conf = direction == 0 ? resources_.refined_forward_confidence : resources_.refined_backward_confidence;
            VkDescriptorSet set = allocate_set(slot, refine_);
            if (!set) return false;
            write_storage(set, 0, flow.view);
            write_storage(set, 1, opposite.view);
            write_storage(set, 2, confidence.view);
            write_storage(set, 3, refined.view);
            write_storage(set, 4, refined_conf.view);
            const PushRefine push{resources_.extent.width, resources_.extent.height, 0.5f};
            bind_dispatch(cmd, refine_, set, &push, sizeof(push), resources_.extent);
            barrier(cmd, refined.image);
            barrier(cmd, refined_conf.image);
        }
        return true;
    }

    void read_timing(Slot& slot, AromaFGFrameOutput& output) {
        if (!slot.has_timestamps || !slot.timestamps) return;
        uint64_t q[6]{};
        VkResult r = vkGetQueryPoolResults(context_.device, slot.timestamps, 0, 6,
            sizeof(q), q, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (r != VK_SUCCESS) return;
        const uint64_t mask = timestamp_bits_ >= 64 ? ~uint64_t(0)
            : ((uint64_t(1) << timestamp_bits_) - 1u);
        auto ms = [&](uint64_t a, uint64_t b) {
            return static_cast<float>((static_cast<double>((b-a)&mask) *
                static_cast<double>(timestamp_period_)) / 1000000.0);
        };
        output.preprocess_gpu_ms = ms(q[0],q[1]);
        output.flow_gpu_ms = ms(q[2],q[3]);
        output.synthesis_gpu_ms = ms(q[4],q[5]);
        slot.has_timestamps = false;
    }

    AromaFGVulkanContextV2 context_{};
    uint32_t slots_ = 3;
    AromaFGConfig config_{};
    VkPhysicalDeviceProperties properties_{};
    VkPhysicalDeviceMemoryProperties memory_properties_{};
    uint32_t timestamp_bits_ = 0;
    float timestamp_period_ = 0.0f;
    VkSampler sampler_ = VK_NULL_HANDLE;
    PipelineCache cache_;
    Pipeline luma_, downsample_, search_, upsample_, refine_, interp8_, interp16_;
    std::vector<Slot> slots_data_;
    Resources resources_;
};

struct BackendState {
    std::unique_ptr<Runtime> runtime;
    AromaFGVulkanContextV2 context{};
    uint32_t slots = 3;
    std::vector<uint8_t> initial_cache;
};

AromaFGStatus backend_initialize(void* user, const AromaFGConfig* config) {
    auto* state = static_cast<BackendState*>(user);
    if (!state || !config) return AROMA_FG_ERROR_INVALID_ARGUMENT;
    state->runtime = std::make_unique<Runtime>(state->context, state->slots);
    return state->runtime->initialize(*config, state->initial_cache.data(), state->initial_cache.size())
        ? AROMA_FG_OK : AROMA_FG_ERROR_UNSUPPORTED;
}
void backend_reset(void*, uint32_t) {}
AromaFGStatus backend_update(void* user, const AromaFGConfig* config) {
    auto* state = static_cast<BackendState*>(user);
    if (!state || !state->runtime || !config) return AROMA_FG_ERROR_BACKEND;
    state->runtime->update_config(*config);
    return AROMA_FG_OK;
}
AromaFGStatus backend_process(void* user, const AromaFGFrameInput* input, AromaFGFrameOutput* output) {
    auto* state = static_cast<BackendState*>(user);
    if (!state || !state->runtime || !input || !output || !input->host_frame)
        return AROMA_FG_ERROR_INVALID_ARGUMENT;
    const auto* frame = reinterpret_cast<const AromaFGVulkanFrameV2*>(input->host_frame);
    return state->runtime->record(*input, *frame, *output);
}
void backend_shutdown(void* user) { delete static_cast<BackendState*>(user); }

} // namespace

extern "C" int aroma_fg_vulkan_validate_context(const AromaFGVulkanContextV2* c) {
    if (!c || c->struct_size < sizeof(*c) ||
        c->api_version != AROMA_FG_VULKAN_INTEROP_VERSION) return 0;
    return validate_context({c->instance,c->physical_device,c->device,c->queue,c->queue_family_index}) ? 1 : 0;
}

extern "C" int aroma_fg_vulkan_validate_frame(const AromaFGVulkanFrameV2* f, uint32_t outputs) {
    if (!f || f->struct_size < sizeof(*f) ||
        f->api_version != AROMA_FG_VULKAN_INTEROP_VERSION ||
        !f->command_buffer || !f->previous_image || !f->current_image ||
        !f->previous_view || !f->current_view ||
        f->previous_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        f->current_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        f->previous_restore_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        f->current_restore_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        !f->extent.width || !f->extent.height ||
        outputs > f->output_capacity || outputs > AROMA_FG_MAX_MULTIPLIER - 1u ||
        f->output_format == VK_FORMAT_UNDEFINED ||
        (outputs && (!f->output_images || !f->output_views || !f->output_layouts ||
                     !f->output_restore_layouts))) return 0;
    for (uint32_t i=0;i<outputs;++i) {
        if (!f->output_images[i] || !f->output_views[i] ||
            f->output_layouts[i] == VK_IMAGE_LAYOUT_UNDEFINED ||
            f->output_restore_layouts[i] == VK_IMAGE_LAYOUT_UNDEFINED) return 0;
    }
    return 1;
}

extern "C" AromaFGStatus aroma_fg_vulkan_backend_create(
    const AromaFGVulkanBackendCreateInfoV1* info, AromaFGBackendV1* backend) {
    if (!info || !backend || info->struct_size < sizeof(*info) ||
        info->api_version != AROMA_FG_VULKAN_INTEROP_VERSION ||
        !aroma_fg_vulkan_validate_context(&info->context)) return AROMA_FG_ERROR_INVALID_ARGUMENT;
    auto* state = new (std::nothrow) BackendState();
    if (!state) return AROMA_FG_ERROR_BACKEND;
    state->context = info->context;
    state->slots = info->in_flight_slots ? info->in_flight_slots : 3u;
    if (info->pipeline_cache_size && !info->pipeline_cache_data) { delete state; return AROMA_FG_ERROR_INVALID_ARGUMENT; }
    if (info->pipeline_cache_size) {
        const auto* begin = static_cast<const uint8_t*>(info->pipeline_cache_data);
        state->initial_cache.assign(begin, begin + info->pipeline_cache_size);
    }
    if (state->slots < 2 || state->slots > 4) { delete state; return AROMA_FG_ERROR_INVALID_ARGUMENT; }
    std::memset(backend, 0, sizeof(*backend));
    backend->struct_size = sizeof(*backend);
    backend->api_version = AROMA_FG_API_VERSION;
    backend->user_data = state;
    backend->initialize = backend_initialize;
    backend->reset_history = backend_reset;
    backend->update_config = backend_update;
    backend->process_frame = backend_process;
    backend->shutdown = backend_shutdown;
    return AROMA_FG_OK;
}

extern "C" AromaFGStatus aroma_fg_vulkan_backend_collect_timing(
    AromaFGBackendV1* backend, uint32_t slot, AromaFGVulkanTimingV1* timing) {
    if (!backend || backend->struct_size < sizeof(*backend) ||
        backend->api_version != AROMA_FG_API_VERSION || !backend->user_data ||
        !timing || timing->struct_size < sizeof(*timing) ||
        timing->api_version != AROMA_FG_VULKAN_INTEROP_VERSION)
        return AROMA_FG_ERROR_INVALID_ARGUMENT;
    auto* state = static_cast<BackendState*>(backend->user_data);
    if (!state->runtime) return AROMA_FG_ERROR_BACKEND;
    return state->runtime->collect(slot, *timing);
}

extern "C" AromaFGStatus aroma_fg_vulkan_backend_export_pipeline_cache(
    AromaFGBackendV1* backend, void* data, size_t* inout_size) {
    if (!backend || backend->struct_size < sizeof(*backend) ||
        backend->api_version != AROMA_FG_API_VERSION || !backend->user_data || !inout_size)
        return AROMA_FG_ERROR_INVALID_ARGUMENT;
    auto* state = static_cast<aromafg::vk::BackendState*>(backend->user_data);
    if (!state->runtime) return AROMA_FG_ERROR_BACKEND;
    std::vector<uint8_t> bytes;
    const VkResult result = state->runtime->export_cache(bytes);
    if (result != VK_SUCCESS) return AROMA_FG_ERROR_BACKEND;
    if (!data) { *inout_size = bytes.size(); return AROMA_FG_OK; }
    if (*inout_size < bytes.size()) { *inout_size = bytes.size(); return AROMA_FG_ERROR_INVALID_ARGUMENT; }
    std::memcpy(data, bytes.data(), bytes.size());
    *inout_size = bytes.size();
    return AROMA_FG_OK;
}
