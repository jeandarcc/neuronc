#include "graphics/backend/vulkan/graphics_vk_internal.h"

#if NPP_GRAPHICS_VK_ENABLED
#include <stdlib.h>
#include <string.h>

static uint32_t neuron_graphics_vk_find_memory_type(
    const NeuronGraphicsBackend *backend, uint32_t type_mask,
    VkMemoryPropertyFlags required_flags) {
  if (backend == NULL) {
    return UINT32_MAX;
  }

  for (uint32_t i = 0; i < backend->memory_properties.memoryTypeCount; ++i) {
    const int bit_match = (type_mask & (1u << i)) != 0;
    const VkMemoryPropertyFlags flags =
        backend->memory_properties.memoryTypes[i].propertyFlags;
    if (bit_match && (flags & required_flags) == required_flags) {
      return i;
    }
  }
  return UINT32_MAX;
}

static int neuron_graphics_vk_track_transient_buffer(
    NeuronGraphicsBackend *backend, uint32_t frame,
    const NeuronGraphicsVkTransientBuffer *buffer) {
  if (backend == NULL || buffer == NULL ||
      frame >= NEURON_GRAPHICS_FRAMES_IN_FLIGHT) {
    return 0;
  }

  NeuronGraphicsVkFrameResources *resources = &backend->frame_resources[frame];
  if (resources->count == resources->capacity) {
    const uint32_t next_capacity = resources->capacity == 0
                                       ? 8u
                                       : resources->capacity * 2u;
    NeuronGraphicsVkTransientBuffer *next =
        (NeuronGraphicsVkTransientBuffer *)realloc(
            resources->buffers,
            (size_t)next_capacity * sizeof(NeuronGraphicsVkTransientBuffer));
    if (next == NULL) {
      neuron_graphics_set_error(
          "Out of memory growing Vulkan frame resources");
      return 0;
    }
    resources->buffers = next;
    resources->capacity = next_capacity;
  }

  resources->buffers[resources->count++] = *buffer;
  return 1;
}

static int neuron_graphics_vk_create_host_visible_buffer(
    NeuronGraphicsBackend *backend, uint32_t frame, const void *data,
    VkDeviceSize byte_size, VkBufferUsageFlags usage,
    NeuronGraphicsVkTransientBuffer *out_buffer) {
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements requirements;
  VkMemoryAllocateInfo allocate_info;
  uint32_t memory_type_index = UINT32_MAX;
  void *mapped = NULL;

  if (backend == NULL || out_buffer == NULL || data == NULL || byte_size == 0) {
    return 0;
  }

  memset(out_buffer, 0, sizeof(*out_buffer));
  memset(&buffer_info, 0, sizeof(buffer_info));
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = byte_size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (backend->vk.vkCreateBuffer(backend->device, &buffer_info, NULL,
                                 &out_buffer->buffer) != VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateBuffer failed for transient graphics buffer");
    return 0;
  }

  memset(&requirements, 0, sizeof(requirements));
  backend->vk.vkGetBufferMemoryRequirements(backend->device, out_buffer->buffer,
                                            &requirements);
  memory_type_index = neuron_graphics_vk_find_memory_type(
      backend, requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (memory_type_index == UINT32_MAX) {
    neuron_graphics_set_error(
        "No Vulkan host-visible memory type for transient graphics buffer");
    backend->vk.vkDestroyBuffer(backend->device, out_buffer->buffer, NULL);
    out_buffer->buffer = VK_NULL_HANDLE;
    return 0;
  }

  memset(&allocate_info, 0, sizeof(allocate_info));
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = requirements.size;
  allocate_info.memoryTypeIndex = memory_type_index;
  if (backend->vk.vkAllocateMemory(backend->device, &allocate_info, NULL,
                                   &out_buffer->memory) != VK_SUCCESS) {
    neuron_graphics_set_error(
        "vkAllocateMemory failed for transient graphics buffer");
    backend->vk.vkDestroyBuffer(backend->device, out_buffer->buffer, NULL);
    out_buffer->buffer = VK_NULL_HANDLE;
    return 0;
  }

  if (backend->vk.vkBindBufferMemory(backend->device, out_buffer->buffer,
                                     out_buffer->memory, 0) != VK_SUCCESS) {
    neuron_graphics_set_error(
        "vkBindBufferMemory failed for transient graphics buffer");
    backend->vk.vkFreeMemory(backend->device, out_buffer->memory, NULL);
    backend->vk.vkDestroyBuffer(backend->device, out_buffer->buffer, NULL);
    memset(out_buffer, 0, sizeof(*out_buffer));
    return 0;
  }

  if (backend->vk.vkMapMemory(backend->device, out_buffer->memory, 0, byte_size,
                              0, &mapped) != VK_SUCCESS ||
      mapped == NULL) {
    neuron_graphics_set_error("vkMapMemory failed for transient graphics buffer");
    backend->vk.vkFreeMemory(backend->device, out_buffer->memory, NULL);
    backend->vk.vkDestroyBuffer(backend->device, out_buffer->buffer, NULL);
    memset(out_buffer, 0, sizeof(*out_buffer));
    return 0;
  }

  memcpy(mapped, data, (size_t)byte_size);
  backend->vk.vkUnmapMemory(backend->device, out_buffer->memory);
  if (!neuron_graphics_vk_track_transient_buffer(backend, frame, out_buffer)) {
    backend->vk.vkFreeMemory(backend->device, out_buffer->memory, NULL);
    backend->vk.vkDestroyBuffer(backend->device, out_buffer->buffer, NULL);
    memset(out_buffer, 0, sizeof(*out_buffer));
    return 0;
  }

  return 1;
}

static void neuron_graphics_vk_transition_image_layout(
    NeuronGraphicsBackend *backend, VkCommandBuffer command_buffer,
    VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier;
  memset(&barrier, 0, sizeof(barrier));
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    backend->vk.vkCmdPipelineBarrier(
        command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    return;
  }

  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  backend->vk.vkCmdPipelineBarrier(
      command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}

static int neuron_graphics_vk_create_sampled_image(
    NeuronGraphicsBackend *backend, VkCommandBuffer command_buffer,
    uint32_t frame, const uint8_t *pixels, uint32_t width, uint32_t height,
    VkImage *out_image, VkDeviceMemory *out_memory, VkImageView *out_view) {
  VkImageCreateInfo image_info;
  VkMemoryRequirements requirements;
  VkMemoryAllocateInfo allocate_info;
  VkImageViewCreateInfo view_info;
  VkBufferImageCopy copy_region;
  NeuronGraphicsVkTransientBuffer staging_buffer;
  uint32_t memory_type_index = UINT32_MAX;
  VkDeviceSize byte_size = 0;

  if (backend == NULL || command_buffer == VK_NULL_HANDLE || pixels == NULL ||
      width == 0 || height == 0 || out_image == NULL || out_memory == NULL ||
      out_view == NULL) {
    return 0;
  }

  *out_image = VK_NULL_HANDLE;
  *out_memory = VK_NULL_HANDLE;
  *out_view = VK_NULL_HANDLE;
  byte_size = (VkDeviceSize)width * (VkDeviceSize)height * 4u;
  memset(&staging_buffer, 0, sizeof(staging_buffer));
  if (!neuron_graphics_vk_create_host_visible_buffer(
          backend, frame, pixels, byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          &staging_buffer)) {
    return 0;
  }

  memset(&image_info, 0, sizeof(image_info));
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (backend->vk.vkCreateImage(backend->device, &image_info, NULL, out_image) !=
      VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateImage failed for graphics texture");
    return 0;
  }

  memset(&requirements, 0, sizeof(requirements));
  backend->vk.vkGetImageMemoryRequirements(backend->device, *out_image,
                                           &requirements);
  memory_type_index = neuron_graphics_vk_find_memory_type(
      backend, requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (memory_type_index == UINT32_MAX) {
    neuron_graphics_set_error("No Vulkan device-local memory type for texture");
    backend->vk.vkDestroyImage(backend->device, *out_image, NULL);
    *out_image = VK_NULL_HANDLE;
    return 0;
  }

  memset(&allocate_info, 0, sizeof(allocate_info));
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = requirements.size;
  allocate_info.memoryTypeIndex = memory_type_index;
  if (backend->vk.vkAllocateMemory(backend->device, &allocate_info, NULL,
                                   out_memory) != VK_SUCCESS) {
    neuron_graphics_set_error("vkAllocateMemory failed for graphics texture");
    backend->vk.vkDestroyImage(backend->device, *out_image, NULL);
    *out_image = VK_NULL_HANDLE;
    return 0;
  }

  if (backend->vk.vkBindImageMemory(backend->device, *out_image, *out_memory,
                                    0) != VK_SUCCESS) {
    neuron_graphics_set_error("vkBindImageMemory failed for graphics texture");
    backend->vk.vkFreeMemory(backend->device, *out_memory, NULL);
    backend->vk.vkDestroyImage(backend->device, *out_image, NULL);
    *out_image = VK_NULL_HANDLE;
    *out_memory = VK_NULL_HANDLE;
    return 0;
  }

  neuron_graphics_vk_transition_image_layout(
      backend, command_buffer, *out_image, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  memset(&copy_region, 0, sizeof(copy_region));
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent.width = width;
  copy_region.imageExtent.height = height;
  copy_region.imageExtent.depth = 1;
  backend->vk.vkCmdCopyBufferToImage(
      command_buffer, staging_buffer.buffer, *out_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
  neuron_graphics_vk_transition_image_layout(
      backend, command_buffer, *out_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  memset(&view_info, 0, sizeof(view_info));
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = *out_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  if (backend->vk.vkCreateImageView(backend->device, &view_info, NULL,
                                    out_view) != VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateImageView failed for graphics texture");
    backend->vk.vkFreeMemory(backend->device, *out_memory, NULL);
    backend->vk.vkDestroyImage(backend->device, *out_image, NULL);
    *out_image = VK_NULL_HANDLE;
    *out_memory = VK_NULL_HANDLE;
    return 0;
  }

  return 1;
}

static const NeuronGraphicsMaterialBinding *
neuron_graphics_vk_find_binding_by_descriptor_binding(
    const NeuronGraphicsMaterial *material, uint32_t descriptor_binding,
    uint32_t kind) {
  if (material == NULL || material->bindings == NULL) {
    return NULL;
  }
  for (uint32_t i = 0; i < material->binding_count; ++i) {
    if (material->bindings[i].descriptor != NULL &&
        material->bindings[i].descriptor->kind == kind &&
        material->bindings[i].descriptor->descriptor_binding ==
            descriptor_binding) {
      return &material->bindings[i];
    }
  }
  return NULL;
}

static int neuron_graphics_vk_ensure_sampler(
    NeuronGraphicsBackend *backend, NeuronGraphicsSampler *sampler) {
  VkSamplerCreateInfo sampler_info;
  if (backend == NULL || sampler == NULL) {
    return 0;
  }
  if (sampler->gpu.gpu_ready && sampler->gpu.native_sampler != 0u) {
    return 1;
  }

  memset(&sampler_info, 0, sizeof(sampler_info));
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.maxLod = 1.0f;
  if (backend->vk.vkCreateSampler(backend->device, &sampler_info, NULL,
                                  (VkSampler *)&sampler->gpu.native_sampler) !=
      VK_SUCCESS) {
    neuron_graphics_set_error("Failed to create Vulkan graphics sampler");
    return 0;
  }

  sampler->gpu.gpu_ready = 1;
  return 1;
}

static int neuron_graphics_vk_ensure_texture(
    NeuronGraphicsBackend *backend, VkCommandBuffer command_buffer,
    uint32_t frame, NeuronGraphicsTexture *texture) {
  if (backend == NULL || texture == NULL || texture->pixels == NULL ||
      texture->width == 0 || texture->height == 0) {
    return 0;
  }
  if (texture->gpu.gpu_ready && texture->gpu.image_view != 0u) {
    return 1;
  }

  if (!neuron_graphics_vk_create_sampled_image(
          backend, command_buffer, frame, texture->pixels, texture->width,
          texture->height, (VkImage *)&texture->gpu.image,
          (VkDeviceMemory *)&texture->gpu.image_memory,
          (VkImageView *)&texture->gpu.image_view)) {
    return 0;
  }

  texture->gpu.gpu_ready = 1;
  return 1;
}

static int neuron_graphics_vk_bind_material_descriptor(
    NeuronGraphicsBackend *backend, VkCommandBuffer command_buffer,
    uint32_t frame, const NeuronGraphicsVkPipelineEntry *pipeline,
    const NeuronGraphicsMaterial *material) {
  VkDescriptorSetAllocateInfo alloc_info;
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  NeuronGraphicsVkTransientBuffer uniform_buffer;
  VkDescriptorBufferInfo buffer_info;
  VkDescriptorImageInfo image_infos[8];
  VkDescriptorImageInfo sampler_infos[8];
  VkWriteDescriptorSet writes[16];
  uint32_t write_count = 0;

  if (backend == NULL || material == NULL || pipeline == NULL) {
    return 0;
  }
  if (pipeline->descriptor_set_layout == VK_NULL_HANDLE) {
    return 1;
  }

  memset(&alloc_info, 0, sizeof(alloc_info));
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = backend->descriptor_pools[frame];
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &pipeline->descriptor_set_layout;
  if (backend->vk.vkAllocateDescriptorSets(backend->device, &alloc_info,
                                           &descriptor_set) != VK_SUCCESS) {
    neuron_graphics_set_error("vkAllocateDescriptorSets failed for graphics material");
    return 0;
  }

  memset(writes, 0, sizeof(writes));
  memset(&uniform_buffer, 0, sizeof(uniform_buffer));

  if (material->shader_descriptor != NULL &&
      material->shader_descriptor->uniform_buffer_size > 0) {
    if (material->uniform_data == NULL) {
      neuron_graphics_set_error("Material is missing uniform buffer data");
      return 0;
    }
    if (!neuron_graphics_vk_create_host_visible_buffer(
            backend, frame, material->uniform_data,
            material->shader_descriptor->uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniform_buffer)) {
      return 0;
    }
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.buffer = uniform_buffer.buffer;
    buffer_info.range = material->shader_descriptor->uniform_buffer_size;
    writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[write_count].dstSet = descriptor_set;
    writes[write_count].dstBinding = 0;
    writes[write_count].descriptorCount = 1;
    writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[write_count].pBufferInfo = &buffer_info;
    ++write_count;
  }

  for (uint32_t i = 0; i < material->binding_count; ++i) {
    const NeuronGraphicsMaterialBinding *binding = &material->bindings[i];
    if (binding->descriptor == NULL ||
        binding->descriptor->descriptor_binding == UINT32_MAX) {
      continue;
    }

    if (binding->descriptor->kind == NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D) {
      if (binding->texture_value == NULL) {
        neuron_graphics_set_error("Material is missing a required Texture2D binding");
        return 0;
      }
      if (!neuron_graphics_vk_ensure_texture(backend, command_buffer, frame,
                                             binding->texture_value)) {
        return 0;
      }
      memset(&image_infos[write_count], 0, sizeof(VkDescriptorImageInfo));
      image_infos[write_count].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_infos[write_count].imageView =
          (VkImageView)binding->texture_value->gpu.image_view;
      writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[write_count].dstSet = descriptor_set;
      writes[write_count].dstBinding = binding->descriptor->descriptor_binding;
      writes[write_count].descriptorCount = 1;
      writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      writes[write_count].pImageInfo = &image_infos[write_count];
      ++write_count;
      continue;
    }

    if (binding->descriptor->kind == NEURON_GRAPHICS_SHADER_BINDING_SAMPLER) {
      if (binding->sampler_value == NULL) {
        neuron_graphics_set_error("Material is missing a required Sampler binding");
        return 0;
      }
      if (!neuron_graphics_vk_ensure_sampler(backend, binding->sampler_value)) {
        return 0;
      }
      memset(&sampler_infos[write_count], 0, sizeof(VkDescriptorImageInfo));
      sampler_infos[write_count].sampler =
          (VkSampler)binding->sampler_value->gpu.native_sampler;
      writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[write_count].dstSet = descriptor_set;
      writes[write_count].dstBinding = binding->descriptor->descriptor_binding;
      writes[write_count].descriptorCount = 1;
      writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      writes[write_count].pImageInfo = &sampler_infos[write_count];
      ++write_count;
    }
  }

  if (write_count > 0) {
    backend->vk.vkUpdateDescriptorSets(backend->device, write_count, writes, 0,
                                       NULL);
  }

  backend->vk.vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout,
      0, 1, &descriptor_set, 0, NULL);
  return 1;
}

void neuron_graphics_backend_reset_frame_resources(NeuronGraphicsBackend *backend,
                                                   uint32_t frame) {
  if (backend == NULL || backend->device == VK_NULL_HANDLE ||
      frame >= NEURON_GRAPHICS_FRAMES_IN_FLIGHT) {
    return;
  }

  NeuronGraphicsVkFrameResources *resources = &backend->frame_resources[frame];
  if (resources->buffers != NULL) {
    for (uint32_t i = 0; i < resources->count; ++i) {
      if (resources->buffers[i].buffer != VK_NULL_HANDLE &&
          backend->vk.vkDestroyBuffer != NULL) {
        backend->vk.vkDestroyBuffer(backend->device, resources->buffers[i].buffer,
                                    NULL);
      }
      if (resources->buffers[i].memory != VK_NULL_HANDLE &&
          backend->vk.vkFreeMemory != NULL) {
        backend->vk.vkFreeMemory(backend->device, resources->buffers[i].memory,
                                 NULL);
      }
    }
  }

  free(resources->buffers);
  resources->buffers = NULL;
  resources->count = 0;
  resources->capacity = 0;
}

static int neuron_graphics_vk_record_draws(NeuronGraphicsBackend *backend,
                                           VkCommandBuffer command_buffer,
                                           uint32_t frame) {
  if (backend == NULL || g_active_canvas == NULL) {
    return 1;
  }
  VkViewport viewport;
  VkRect2D scissor;
  memset(&viewport, 0, sizeof(viewport));
  viewport.width = (float)backend->window->width;
  viewport.height = (float)backend->window->height;
  viewport.maxDepth = 1.0f;
  backend->vk.vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  memset(&scissor, 0, sizeof(scissor));
  scissor.extent = backend->swapchain_extent;
  backend->vk.vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  for (size_t i = 0; i < g_active_canvas->draw_command_count; ++i) {
    const NeuronGraphicsDrawCommand *command =
        &g_active_canvas->draw_commands[i];
    NeuronGraphicsVkPipelineEntry *pipeline = NULL;
    NeuronGraphicsVkTransientBuffer vertex_buffer;
    VkDeviceSize offset = 0;
    if (command->mesh == NULL || command->material == NULL ||
        command->mesh->vertices == NULL || command->mesh->vertex_count == 0) {
      neuron_graphics_set_error("Graphics draw command is missing mesh data");
      continue;
    }

    pipeline = neuron_graphics_backend_get_pipeline(
        backend, command->material->shader_descriptor);
    if (pipeline == NULL || pipeline->graphics_pipeline == VK_NULL_HANDLE) {
      return 0;
    }
    backend->vk.vkCmdBindPipeline(command_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->graphics_pipeline);

    if (!neuron_graphics_vk_bind_material_descriptor(
            backend, command_buffer, frame, pipeline, command->material)) {
      return 0;
    }

    if (!neuron_graphics_vk_create_host_visible_buffer(
            backend, frame, command->mesh->vertices,
            (VkDeviceSize)(command->mesh->vertex_count *
                           sizeof(NeuronGraphicsVertex)),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertex_buffer)) {
      return 0;
    }

    backend->vk.vkCmdBindVertexBuffers(command_buffer, 0, 1,
                                       &vertex_buffer.buffer, &offset);

    if (command->kind == NEURON_GRAPHICS_DRAW_KIND_INDEXED &&
        command->mesh->indices != NULL && command->mesh->index_count > 0) {
      NeuronGraphicsVkTransientBuffer index_buffer;
      if (!neuron_graphics_vk_create_host_visible_buffer(
              backend, frame, command->mesh->indices,
              (VkDeviceSize)(command->mesh->index_count * sizeof(uint32_t)),
              VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &index_buffer)) {
        return 0;
      }
      backend->vk.vkCmdBindIndexBuffer(command_buffer, index_buffer.buffer, 0,
                                       VK_INDEX_TYPE_UINT32);
      backend->vk.vkCmdDrawIndexed(command_buffer,
                                   (uint32_t)command->mesh->index_count, 1, 0,
                                   0, 0);
      continue;
    } else if (command->kind == NEURON_GRAPHICS_DRAW_KIND_INDEXED) {
      neuron_graphics_set_error(
          "cmd.DrawIndexed requires a mesh with index data");
      continue;
    }

    backend->vk.vkCmdDraw(
        command_buffer, (uint32_t)command->mesh->vertex_count,
        command->kind == NEURON_GRAPHICS_DRAW_KIND_INSTANCED &&
                command->instance_count > 0
            ? (uint32_t)command->instance_count
            : 1u,
        0, 0);
  }

  return 1;
}

int neuron_graphics_backend_begin_frame(NeuronGraphicsBackend *backend) {
  uint32_t frame = 0;
  VkResult result = VK_SUCCESS;
  VkCommandBuffer command_buffer;
  VkCommandBufferBeginInfo begin_info;
  if (backend == NULL || backend->device == VK_NULL_HANDLE) {
    return 0;
  }
  if (backend->window->width <= 0 || backend->window->height <= 0) {
    return 0;
  }

  if (backend->resize_pending) {
    if (!neuron_graphics_backend_recreate_swapchain(backend)) {
      return 0;
    }
  }

  frame = backend->frame_index % NEURON_GRAPHICS_FRAMES_IN_FLIGHT;
  result = backend->vk.vkWaitForFences(backend->device, 1,
                                       &backend->in_flight[frame], VK_TRUE,
                                       UINT64_MAX);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkWaitForFences failed (%d)", (int)result);
    return 0;
  }
  neuron_graphics_backend_reset_frame_resources(backend, frame);
  if (backend->descriptor_pools[frame] != VK_NULL_HANDLE &&
      backend->vk.vkResetDescriptorPool != NULL) {
    backend->vk.vkResetDescriptorPool(backend->device,
                                      backend->descriptor_pools[frame], 0);
  }
  result =
      backend->vk.vkResetFences(backend->device, 1, &backend->in_flight[frame]);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkResetFences failed (%d)", (int)result);
    return 0;
  }

  result = backend->vk.vkAcquireNextImageKHR(
      backend->device, backend->swapchain, UINT64_MAX,
      backend->image_available[frame], VK_NULL_HANDLE,
      &backend->acquired_image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    backend->resize_pending = 1;
    return 0;
  }
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    neuron_graphics_set_error("vkAcquireNextImageKHR failed (%d)", (int)result);
    return 0;
  }
  if (result == VK_SUBOPTIMAL_KHR) {
    backend->resize_pending = 1;
  }

  command_buffer = backend->command_buffers[frame];
  backend->vk.vkResetCommandBuffer(command_buffer, 0);
  memset(&begin_info, 0, sizeof(begin_info));
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  result = backend->vk.vkBeginCommandBuffer(command_buffer, &begin_info);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkBeginCommandBuffer failed (%d)", (int)result);
    return 0;
  }

  backend->submitted_frame_index = frame;
  backend->frame_submitted = 0;
  return 1;
}

int neuron_graphics_backend_present(NeuronGraphicsBackend *backend) {
  uint32_t frame = 0;
  VkCommandBuffer command_buffer;
  VkClearValue clear_value;
  VkRenderPassBeginInfo pass_info;
  VkResult result = VK_SUCCESS;
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info;
  VkPresentInfoKHR present_info;
  if (backend == NULL || backend->device == VK_NULL_HANDLE) {
    return 0;
  }

  frame = backend->submitted_frame_index;
  command_buffer = backend->command_buffers[frame];

  memset(&clear_value, 0, sizeof(clear_value));
  clear_value.color.float32[0] =
      (g_active_canvas != NULL && g_active_canvas->has_clear_color)
          ? g_active_canvas->clear_color.red
          : 0.0f;
  clear_value.color.float32[1] =
      (g_active_canvas != NULL && g_active_canvas->has_clear_color)
          ? g_active_canvas->clear_color.green
          : 0.0f;
  clear_value.color.float32[2] =
      (g_active_canvas != NULL && g_active_canvas->has_clear_color)
          ? g_active_canvas->clear_color.blue
          : 0.0f;
  clear_value.color.float32[3] =
      (g_active_canvas != NULL && g_active_canvas->has_clear_color)
          ? g_active_canvas->clear_color.alpha
          : 1.0f;

  memset(&pass_info, 0, sizeof(pass_info));
  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  pass_info.renderPass = backend->render_pass;
  pass_info.framebuffer = backend->framebuffers[backend->acquired_image_index];
  pass_info.renderArea.extent = backend->swapchain_extent;
  pass_info.clearValueCount = 1;
  pass_info.pClearValues = &clear_value;
  backend->vk.vkCmdBeginRenderPass(command_buffer, &pass_info,
                                   VK_SUBPASS_CONTENTS_INLINE);
  if (!neuron_graphics_vk_record_draws(backend, command_buffer, frame)) {
    backend->vk.vkCmdEndRenderPass(command_buffer);
    return 0;
  }
  backend->vk.vkCmdEndRenderPass(command_buffer);

  result = backend->vk.vkEndCommandBuffer(command_buffer);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkEndCommandBuffer failed (%d)", (int)result);
    return 0;
  }

  memset(&submit_info, 0, sizeof(submit_info));
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &backend->image_available[frame];
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &backend->render_finished[frame];
  result = backend->vk.vkQueueSubmit(backend->graphics_queue, 1, &submit_info,
                                     backend->in_flight[frame]);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkQueueSubmit failed (%d)", (int)result);
    return 0;
  }

  backend->frame_submitted = 1;
  memset(&present_info, 0, sizeof(present_info));
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &backend->render_finished[frame];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &backend->swapchain;
  present_info.pImageIndices = &backend->acquired_image_index;
  result =
      backend->vk.vkQueuePresentKHR(backend->present_queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    backend->resize_pending = 1;
    result = VK_SUCCESS;
  }
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkQueuePresentKHR failed (%d)", (int)result);
    return 0;
  }

  backend->frame_submitted = 0;
  backend->frame_index = (frame + 1u) % NEURON_GRAPHICS_FRAMES_IN_FLIGHT;
  return 1;
}
#endif
