#include "graphics/backend/vulkan/graphics_vk_internal.h"

#if Neuron_GRAPHICS_VK_ENABLED
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int neuron_graphics_backend_ensure_descriptor_pools(
    NeuronGraphicsBackend *backend) {
  if (backend == NULL || backend->device == VK_NULL_HANDLE) {
    return 0;
  }

  for (uint32_t i = 0; i < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++i) {
    if (backend->descriptor_pools[i] != VK_NULL_HANDLE) {
      continue;
    }

    VkDescriptorPoolSize pool_sizes[3];
    VkDescriptorPoolCreateInfo pool_info;
    memset(pool_sizes, 0, sizeof(pool_sizes));
    memset(&pool_info, 0, sizeof(pool_info));

    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 128;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    pool_sizes[1].descriptorCount = 128;
    pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    pool_sizes[2].descriptorCount = 128;

    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 128;
    pool_info.poolSizeCount = 3;
    pool_info.pPoolSizes = pool_sizes;
    if (backend->vk.vkCreateDescriptorPool(backend->device, &pool_info, NULL,
                                           &backend->descriptor_pools[i]) !=
        VK_SUCCESS) {
      neuron_graphics_set_error("Failed to create graphics descriptor pool");
      return 0;
    }
  }

  return 1;
}

static VkDescriptorType neuron_graphics_vk_descriptor_type_for_binding(
    const NeuronGraphicsShaderBindingDescriptor *binding) {
  if (binding == NULL) {
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }
  switch (binding->kind) {
  case NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D:
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case NEURON_GRAPHICS_SHADER_BINDING_SAMPLER:
    return VK_DESCRIPTOR_TYPE_SAMPLER;
  default:
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }
}

static int neuron_graphics_vk_create_descriptor_set_layout(
    NeuronGraphicsBackend *backend,
    const NeuronGraphicsShaderDescriptor *shader_descriptor,
    VkDescriptorSetLayout *out_layout) {
  VkDescriptorSetLayoutBinding bindings[16];
  uint32_t binding_count = 0;
  VkDescriptorSetLayoutCreateInfo layout_info;

  if (backend == NULL || shader_descriptor == NULL || out_layout == NULL) {
    return 0;
  }
  *out_layout = VK_NULL_HANDLE;

  if (shader_descriptor->uniform_buffer_size > 0) {
    bindings[binding_count].binding = 0;
    bindings[binding_count].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[binding_count].descriptorCount = 1;
    bindings[binding_count].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[binding_count].pImmutableSamplers = NULL;
    ++binding_count;
  }

  for (uint32_t i = 0; i < shader_descriptor->binding_count; ++i) {
    const NeuronGraphicsShaderBindingDescriptor *binding =
        &shader_descriptor->bindings[i];
    VkDescriptorType descriptor_type =
        neuron_graphics_vk_descriptor_type_for_binding(binding);
    if (descriptor_type == VK_DESCRIPTOR_TYPE_MAX_ENUM ||
        binding->descriptor_binding == UINT32_MAX) {
      continue;
    }
    if (binding_count >= (uint32_t)(sizeof(bindings) / sizeof(bindings[0]))) {
      neuron_graphics_set_error("Too many graphics bindings for Vulkan layout");
      return 0;
    }
    bindings[binding_count].binding = binding->descriptor_binding;
    bindings[binding_count].descriptorType = descriptor_type;
    bindings[binding_count].descriptorCount = 1;
    bindings[binding_count].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[binding_count].pImmutableSamplers = NULL;
    ++binding_count;
  }

  if (binding_count == 0) {
    return 1;
  }

  memset(&layout_info, 0, sizeof(layout_info));
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = binding_count;
  layout_info.pBindings = bindings;
  if (backend->vk.vkCreateDescriptorSetLayout(backend->device, &layout_info,
                                              NULL, out_layout) != VK_SUCCESS) {
    neuron_graphics_set_error("Failed to create graphics descriptor set layout");
    return 0;
  }

  return 1;
}

static void neuron_graphics_vk_fill_vertex_layout(
    const NeuronGraphicsShaderDescriptor *shader_descriptor,
    VkPipelineVertexInputStateCreateInfo *out_info,
    VkVertexInputBindingDescription *out_binding,
    VkVertexInputAttributeDescription *out_attributes,
    uint32_t *out_attribute_count) {
  uint32_t attribute_count = 0;
  memset(out_info, 0, sizeof(*out_info));
  memset(out_binding, 0, sizeof(*out_binding));
  memset(out_attributes, 0, sizeof(VkVertexInputAttributeDescription) * 3u);

  out_info->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  out_binding->binding = 0;
  out_binding->stride = sizeof(NeuronGraphicsVertex);
  out_binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  if ((shader_descriptor->vertex_layout_mask &
       NEURON_GRAPHICS_VERTEX_LAYOUT_POSITION) != 0u) {
    out_attributes[attribute_count].location = 0;
    out_attributes[attribute_count].binding = 0;
    out_attributes[attribute_count].format = VK_FORMAT_R32G32B32_SFLOAT;
    out_attributes[attribute_count].offset =
        (uint32_t)offsetof(NeuronGraphicsVertex, px);
    ++attribute_count;
  }
  if ((shader_descriptor->vertex_layout_mask & NEURON_GRAPHICS_VERTEX_LAYOUT_UV) !=
      0u) {
    out_attributes[attribute_count].location = 1;
    out_attributes[attribute_count].binding = 0;
    out_attributes[attribute_count].format = VK_FORMAT_R32G32_SFLOAT;
    out_attributes[attribute_count].offset =
        (uint32_t)offsetof(NeuronGraphicsVertex, u);
    ++attribute_count;
  }
  if ((shader_descriptor->vertex_layout_mask &
       NEURON_GRAPHICS_VERTEX_LAYOUT_NORMAL) != 0u) {
    out_attributes[attribute_count].location = 2;
    out_attributes[attribute_count].binding = 0;
    out_attributes[attribute_count].format = VK_FORMAT_R32G32B32_SFLOAT;
    out_attributes[attribute_count].offset =
        (uint32_t)offsetof(NeuronGraphicsVertex, nx);
    ++attribute_count;
  }

  out_info->vertexBindingDescriptionCount = 1;
  out_info->pVertexBindingDescriptions = out_binding;
  out_info->vertexAttributeDescriptionCount = attribute_count;
  out_info->pVertexAttributeDescriptions = out_attributes;
  *out_attribute_count = attribute_count;
}

static int neuron_graphics_vk_append_pipeline_entry(
    NeuronGraphicsBackend *backend, const NeuronGraphicsVkPipelineEntry *entry) {
  NeuronGraphicsVkPipelineEntry *next_entries = NULL;
  uint32_t next_capacity = 0;

  if (backend->pipeline_count == backend->pipeline_capacity) {
    next_capacity = backend->pipeline_capacity == 0 ? 4u
                                                    : backend->pipeline_capacity * 2u;
    next_entries = (NeuronGraphicsVkPipelineEntry *)realloc(
        backend->pipelines,
        (size_t)next_capacity * sizeof(NeuronGraphicsVkPipelineEntry));
    if (next_entries == NULL) {
      neuron_graphics_set_error("Out of memory growing graphics pipeline cache");
      return 0;
    }
    backend->pipelines = next_entries;
    backend->pipeline_capacity = next_capacity;
  }

  backend->pipelines[backend->pipeline_count++] = *entry;
  return 1;
}

int neuron_graphics_backend_create_pipeline(NeuronGraphicsBackend *backend) {
  return neuron_graphics_backend_ensure_descriptor_pools(backend);
}

NeuronGraphicsVkPipelineEntry *neuron_graphics_backend_get_pipeline(
    NeuronGraphicsBackend *backend,
    const NeuronGraphicsShaderDescriptor *shader_descriptor) {
  VkShaderModuleCreateInfo vert_info;
  VkShaderModuleCreateInfo frag_info;
  VkShaderModule vert_module = VK_NULL_HANDLE;
  VkShaderModule frag_module = VK_NULL_HANDLE;
  VkPipelineShaderStageCreateInfo shader_stages[2];
  VkPipelineVertexInputStateCreateInfo vertex_input_info;
  VkVertexInputBindingDescription vertex_binding;
  VkVertexInputAttributeDescription vertex_attributes[3];
  uint32_t vertex_attribute_count = 0;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineColorBlendAttachmentState color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo color_blending;
  VkPipelineLayoutCreateInfo pipeline_layout_info;
  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state;
  VkGraphicsPipelineCreateInfo pipeline_info;
  VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline graphics_pipeline = VK_NULL_HANDLE;
  NeuronGraphicsVkPipelineEntry entry;

  if (backend == NULL || shader_descriptor == NULL ||
      backend->device == VK_NULL_HANDLE || backend->render_pass == VK_NULL_HANDLE ||
      shader_descriptor->vertex_spirv_words == NULL ||
      shader_descriptor->vertex_spirv_word_count == 0 ||
      shader_descriptor->fragment_spirv_words == NULL ||
      shader_descriptor->fragment_spirv_word_count == 0) {
    neuron_graphics_set_error("Graphics pipeline requires embedded shader artifacts");
    return NULL;
  }

  for (uint32_t i = 0; i < backend->pipeline_count; ++i) {
    if (backend->pipelines[i].shader_descriptor == shader_descriptor) {
      return &backend->pipelines[i];
    }
  }

  if (!neuron_graphics_backend_ensure_descriptor_pools(backend) ||
      !neuron_graphics_vk_create_descriptor_set_layout(
          backend, shader_descriptor, &descriptor_set_layout)) {
    return NULL;
  }

  memset(&vert_info, 0, sizeof(vert_info));
  vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  vert_info.codeSize =
      (size_t)shader_descriptor->vertex_spirv_word_count * sizeof(uint32_t);
  vert_info.pCode = shader_descriptor->vertex_spirv_words;
  if (backend->vk.vkCreateShaderModule(backend->device, &vert_info, NULL,
                                       &vert_module) != VK_SUCCESS) {
    neuron_graphics_set_error("Failed to create graphics vertex shader module");
    goto fail;
  }

  memset(&frag_info, 0, sizeof(frag_info));
  frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  frag_info.codeSize =
      (size_t)shader_descriptor->fragment_spirv_word_count * sizeof(uint32_t);
  frag_info.pCode = shader_descriptor->fragment_spirv_words;
  if (backend->vk.vkCreateShaderModule(backend->device, &frag_info, NULL,
                                       &frag_module) != VK_SUCCESS) {
    neuron_graphics_set_error("Failed to create graphics fragment shader module");
    goto fail;
  }

  memset(shader_stages, 0, sizeof(shader_stages));
  shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stages[0].module = vert_module;
  shader_stages[0].pName = "main";
  shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stages[1].module = frag_module;
  shader_stages[1].pName = "main";

  neuron_graphics_vk_fill_vertex_layout(shader_descriptor, &vertex_input_info,
                                        &vertex_binding, vertex_attributes,
                                        &vertex_attribute_count);

  memset(&input_assembly, 0, sizeof(input_assembly));
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  memset(&viewport, 0, sizeof(viewport));
  viewport.width = (float)backend->window->width;
  viewport.height = (float)backend->window->height;
  viewport.maxDepth = 1.0f;

  memset(&scissor, 0, sizeof(scissor));
  scissor.extent = backend->swapchain_extent;

  memset(&viewport_state, 0, sizeof(viewport_state));
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  memset(&rasterizer, 0, sizeof(rasterizer));
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  memset(&multisampling, 0, sizeof(multisampling));
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  memset(&color_blend_attachment, 0, sizeof(color_blend_attachment));
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  memset(&color_blending, 0, sizeof(color_blending));
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  memset(&pipeline_layout_info, 0, sizeof(pipeline_layout_info));
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  if (descriptor_set_layout != VK_NULL_HANDLE) {
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
  }
  if (backend->vk.vkCreatePipelineLayout(backend->device, &pipeline_layout_info,
                                         NULL, &pipeline_layout) != VK_SUCCESS) {
    neuron_graphics_set_error("Failed to create graphics pipeline layout");
    goto fail;
  }

  memset(&dynamic_state, 0, sizeof(dynamic_state));
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic_states;

  memset(&pipeline_info, 0, sizeof(pipeline_info));
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = backend->render_pass;

  if (backend->vk.vkCreateGraphicsPipelines(backend->device, VK_NULL_HANDLE, 1,
                                            &pipeline_info, NULL,
                                            &graphics_pipeline) != VK_SUCCESS) {
    neuron_graphics_set_error("Failed to create graphics pipeline");
    goto fail;
  }

  memset(&entry, 0, sizeof(entry));
  entry.shader_descriptor = shader_descriptor;
  entry.descriptor_set_layout = descriptor_set_layout;
  entry.pipeline_layout = pipeline_layout;
  entry.graphics_pipeline = graphics_pipeline;

  backend->vk.vkDestroyShaderModule(backend->device, vert_module, NULL);
  backend->vk.vkDestroyShaderModule(backend->device, frag_module, NULL);
  vert_module = VK_NULL_HANDLE;
  frag_module = VK_NULL_HANDLE;

  if (!neuron_graphics_vk_append_pipeline_entry(backend, &entry)) {
    goto fail;
  }
  return &backend->pipelines[backend->pipeline_count - 1];

fail:
  if (vert_module != VK_NULL_HANDLE) {
    backend->vk.vkDestroyShaderModule(backend->device, vert_module, NULL);
  }
  if (frag_module != VK_NULL_HANDLE) {
    backend->vk.vkDestroyShaderModule(backend->device, frag_module, NULL);
  }
  if (graphics_pipeline != VK_NULL_HANDLE) {
    backend->vk.vkDestroyPipeline(backend->device, graphics_pipeline, NULL);
  }
  if (pipeline_layout != VK_NULL_HANDLE) {
    backend->vk.vkDestroyPipelineLayout(backend->device, pipeline_layout, NULL);
  }
  if (descriptor_set_layout != VK_NULL_HANDLE) {
    backend->vk.vkDestroyDescriptorSetLayout(backend->device, descriptor_set_layout,
                                             NULL);
  }
  return NULL;
}
#endif
