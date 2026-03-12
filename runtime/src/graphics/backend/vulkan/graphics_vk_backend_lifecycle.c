#include "graphics/backend/vulkan/graphics_vk_internal.h"

#if NPP_GRAPHICS_VK_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdlib.h>
#include <string.h>

NeuronGraphicsBackend *neuron_graphics_backend_create(NeuronGraphicsWindow *window) {
  if (window == NULL || window->hwnd == NULL) {
    neuron_graphics_set_error(
        "Cannot create Vulkan backend without a Win32 window");
    return NULL;
  }

  NeuronVulkanCommonContext shared = {0};
  char shared_error[512] = {0};
  if (neuron_vk_common_acquire(NEURON_GPU_SCOPE_MODE_DEFAULT,
                               NEURON_GPU_DEVICE_CLASS_ANY, 1, &shared,
                               shared_error, sizeof(shared_error)) != 0) {
    neuron_graphics_set_error("%s",
                              shared_error[0] != '\0'
                                  ? shared_error
                                  : "Failed to acquire shared Vulkan context");
    return NULL;
  }

  NeuronGraphicsBackend *backend =
      (NeuronGraphicsBackend *)calloc(1, sizeof(NeuronGraphicsBackend));
  if (backend == NULL) {
    neuron_vk_common_release();
    neuron_graphics_set_error("Out of memory allocating graphics backend");
    return NULL;
  }

  backend->window = window;
  backend->common_context_acquired = 1;

  if (!shared.has_graphics_queue || shared.graphics_queue_family == UINT32_MAX) {
    neuron_graphics_set_error("Shared Vulkan device has no graphics queue");
    goto fail;
  }
  if (!shared.has_swapchain_extension) {
    neuron_graphics_set_error(
        "Shared Vulkan device does not support VK_KHR_swapchain");
    goto fail;
  }

  if (!neuron_vk_load_global(&backend->vk, &shared)) {
    goto fail;
  }
  backend->instance = shared.instance;
  backend->physical_device = shared.physical_device;
  backend->device = shared.device;
  backend->graphics_queue_family = shared.graphics_queue_family;
  backend->tensor_interop_source_queue_family = shared.compute_queue_family;
  backend->graphics_queue = shared.graphics_queue;

  if (!neuron_vk_load_instance(&backend->vk, backend->instance)) {
    goto fail;
  }
  if (!neuron_vk_load_device(&backend->vk, backend->device)) {
    goto fail;
  }
  backend->vk.vkGetPhysicalDeviceMemoryProperties(backend->physical_device,
                                                  &backend->memory_properties);

  VkResult result = VK_SUCCESS;
  VkWin32SurfaceCreateInfoKHR surface_info;
  memset(&surface_info, 0, sizeof(surface_info));
  surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surface_info.hinstance = GetModuleHandleW(NULL);
  surface_info.hwnd = (HWND)window->hwnd;
  result = backend->vk.vkCreateWin32SurfaceKHR(backend->instance, &surface_info,
                                               NULL, &backend->surface);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateWin32SurfaceKHR failed (%d)",
                              (int)result);
    goto fail;
  }

  if (!neuron_vk_common_pick_present_queue(
          backend->surface, &backend->present_queue_family,
          &backend->present_queue, shared_error, sizeof(shared_error))) {
    neuron_graphics_set_error("%s",
                              shared_error[0] != '\0'
                                  ? shared_error
                                  : "Failed to select a present queue");
    goto fail;
  }

  VkCommandPoolCreateInfo command_pool_info;
  memset(&command_pool_info, 0, sizeof(command_pool_info));
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.queueFamilyIndex = backend->graphics_queue_family;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  result = backend->vk.vkCreateCommandPool(backend->device, &command_pool_info,
                                           NULL, &backend->command_pool);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateCommandPool failed (%d)", (int)result);
    goto fail;
  }

  VkCommandBufferAllocateInfo cmd_alloc;
  memset(&cmd_alloc, 0, sizeof(cmd_alloc));
  cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_alloc.commandPool = backend->command_pool;
  cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_alloc.commandBufferCount = NEURON_GRAPHICS_FRAMES_IN_FLIGHT;
  result = backend->vk.vkAllocateCommandBuffers(backend->device, &cmd_alloc,
                                                backend->command_buffers);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkAllocateCommandBuffers failed (%d)",
                              (int)result);
    goto fail;
  }

  if (!neuron_graphics_backend_create_swapchain(backend)) {
    goto fail;
  }

  VkSemaphoreCreateInfo semaphore_info;
  memset(&semaphore_info, 0, sizeof(semaphore_info));
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info;
  memset(&fence_info, 0, sizeof(fence_info));
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++i) {
    result = backend->vk.vkCreateSemaphore(backend->device, &semaphore_info,
                                           NULL, &backend->image_available[i]);
    if (result != VK_SUCCESS) {
      neuron_graphics_set_error(
          "vkCreateSemaphore(image_available) failed (%d)", (int)result);
      break;
    }
    result = backend->vk.vkCreateSemaphore(backend->device, &semaphore_info,
                                           NULL, &backend->render_finished[i]);
    if (result != VK_SUCCESS) {
      neuron_graphics_set_error(
          "vkCreateSemaphore(render_finished) failed (%d)", (int)result);
      break;
    }
    result = backend->vk.vkCreateFence(backend->device, &fence_info, NULL,
                                       &backend->in_flight[i]);
    if (result != VK_SUCCESS) {
      neuron_graphics_set_error("vkCreateFence failed (%d)", (int)result);
      break;
    }
  }

  if (result != VK_SUCCESS) {
    goto fail;
  }

  if (!neuron_graphics_backend_create_pipeline(backend)) {
    goto fail;
  }

  backend->frame_index = 0;
  backend->submitted_frame_index = 0;
  backend->frame_submitted = 0;
  backend->resize_pending = 0;
  backend->tensor_interop_buffer = VK_NULL_HANDLE;
  backend->tensor_interop_size = 0;
  backend->tensor_interop_source_queue_family = backend->graphics_queue_family;
  backend->tensor_interop_requires_ownership_transfer = 0;
  backend->tensor_interop_ready = 0;
  return backend;

fail:
  if (backend->device != VK_NULL_HANDLE && backend->vk.vkDeviceWaitIdle != NULL) {
    backend->vk.vkDeviceWaitIdle(backend->device);
  }
  for (uint32_t j = 0; j < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++j) {
    neuron_graphics_backend_reset_frame_resources(backend, j);
  }
  for (uint32_t j = 0; j < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++j) {
    if (backend->in_flight[j] != VK_NULL_HANDLE &&
        backend->vk.vkDestroyFence != NULL) {
      backend->vk.vkDestroyFence(backend->device, backend->in_flight[j], NULL);
    }
    if (backend->render_finished[j] != VK_NULL_HANDLE &&
        backend->vk.vkDestroySemaphore != NULL) {
      backend->vk.vkDestroySemaphore(backend->device,
                                     backend->render_finished[j], NULL);
    }
    if (backend->image_available[j] != VK_NULL_HANDLE &&
        backend->vk.vkDestroySemaphore != NULL) {
      backend->vk.vkDestroySemaphore(backend->device,
                                     backend->image_available[j], NULL);
    }
  }
  neuron_graphics_backend_destroy_swapchain(backend);
  for (uint32_t j = 0; j < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++j) {
    if (backend->descriptor_pools[j] != VK_NULL_HANDLE &&
        backend->vk.vkDestroyDescriptorPool != NULL) {
      backend->vk.vkDestroyDescriptorPool(backend->device,
                                          backend->descriptor_pools[j], NULL);
      backend->descriptor_pools[j] = VK_NULL_HANDLE;
    }
  }
  if (backend->pipelines != NULL) {
    for (uint32_t j = 0; j < backend->pipeline_count; ++j) {
      if (backend->pipelines[j].graphics_pipeline != VK_NULL_HANDLE &&
          backend->vk.vkDestroyPipeline != NULL) {
        backend->vk.vkDestroyPipeline(backend->device,
                                      backend->pipelines[j].graphics_pipeline,
                                      NULL);
      }
      if (backend->pipelines[j].pipeline_layout != VK_NULL_HANDLE &&
          backend->vk.vkDestroyPipelineLayout != NULL) {
        backend->vk.vkDestroyPipelineLayout(
            backend->device, backend->pipelines[j].pipeline_layout, NULL);
      }
      if (backend->pipelines[j].descriptor_set_layout != VK_NULL_HANDLE &&
          backend->vk.vkDestroyDescriptorSetLayout != NULL) {
        backend->vk.vkDestroyDescriptorSetLayout(
            backend->device, backend->pipelines[j].descriptor_set_layout, NULL);
      }
    }
    free(backend->pipelines);
    backend->pipelines = NULL;
    backend->pipeline_count = 0;
    backend->pipeline_capacity = 0;
  }
  if (backend->command_pool != VK_NULL_HANDLE &&
      backend->vk.vkDestroyCommandPool != NULL) {
    backend->vk.vkDestroyCommandPool(backend->device, backend->command_pool,
                                     NULL);
  }
  if (backend->surface != VK_NULL_HANDLE &&
      backend->vk.vkDestroySurfaceKHR != NULL &&
      backend->instance != VK_NULL_HANDLE) {
    backend->vk.vkDestroySurfaceKHR(backend->instance, backend->surface, NULL);
  }
  if (backend->common_context_acquired) {
    neuron_vk_common_release();
  }
  free(backend);
  return NULL;
}

void neuron_graphics_backend_destroy(NeuronGraphicsBackend *backend) {
  if (backend == NULL) {
    return;
  }

  if (backend->device != VK_NULL_HANDLE &&
      backend->vk.vkDeviceWaitIdle != NULL) {
    backend->vk.vkDeviceWaitIdle(backend->device);
  }
  for (uint32_t i = 0; i < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++i) {
    neuron_graphics_backend_reset_frame_resources(backend, i);
  }

  if (backend->device != VK_NULL_HANDLE) {
    for (uint32_t i = 0; i < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++i) {
      if (backend->in_flight[i] != VK_NULL_HANDLE &&
          backend->vk.vkDestroyFence != NULL) {
        backend->vk.vkDestroyFence(backend->device, backend->in_flight[i],
                                   NULL);
      }
      if (backend->render_finished[i] != VK_NULL_HANDLE &&
          backend->vk.vkDestroySemaphore != NULL) {
        backend->vk.vkDestroySemaphore(backend->device,
                                       backend->render_finished[i], NULL);
      }
      if (backend->image_available[i] != VK_NULL_HANDLE &&
          backend->vk.vkDestroySemaphore != NULL) {
        backend->vk.vkDestroySemaphore(backend->device,
                                       backend->image_available[i], NULL);
      }
    }
  }

  neuron_graphics_backend_destroy_swapchain(backend);

  for (uint32_t i = 0; i < NEURON_GRAPHICS_FRAMES_IN_FLIGHT; ++i) {
    if (backend->descriptor_pools[i] != VK_NULL_HANDLE &&
        backend->vk.vkDestroyDescriptorPool != NULL &&
        backend->device != VK_NULL_HANDLE) {
      backend->vk.vkDestroyDescriptorPool(backend->device,
                                          backend->descriptor_pools[i], NULL);
      backend->descriptor_pools[i] = VK_NULL_HANDLE;
    }
  }
  if (backend->pipelines != NULL && backend->device != VK_NULL_HANDLE) {
    for (uint32_t i = 0; i < backend->pipeline_count; ++i) {
      if (backend->pipelines[i].graphics_pipeline != VK_NULL_HANDLE &&
          backend->vk.vkDestroyPipeline != NULL) {
        backend->vk.vkDestroyPipeline(backend->device,
                                      backend->pipelines[i].graphics_pipeline,
                                      NULL);
      }
      if (backend->pipelines[i].pipeline_layout != VK_NULL_HANDLE &&
          backend->vk.vkDestroyPipelineLayout != NULL) {
        backend->vk.vkDestroyPipelineLayout(
            backend->device, backend->pipelines[i].pipeline_layout, NULL);
      }
      if (backend->pipelines[i].descriptor_set_layout != VK_NULL_HANDLE &&
          backend->vk.vkDestroyDescriptorSetLayout != NULL) {
        backend->vk.vkDestroyDescriptorSetLayout(
            backend->device, backend->pipelines[i].descriptor_set_layout, NULL);
      }
    }
    free(backend->pipelines);
    backend->pipelines = NULL;
    backend->pipeline_count = 0;
    backend->pipeline_capacity = 0;
  }

  if (backend->command_pool != VK_NULL_HANDLE &&
      backend->vk.vkDestroyCommandPool != NULL &&
      backend->device != VK_NULL_HANDLE) {
    backend->vk.vkDestroyCommandPool(backend->device, backend->command_pool,
                                     NULL);
    backend->command_pool = VK_NULL_HANDLE;
  }

  if (backend->surface != VK_NULL_HANDLE &&
      backend->vk.vkDestroySurfaceKHR != NULL &&
      backend->instance != VK_NULL_HANDLE) {
    backend->vk.vkDestroySurfaceKHR(backend->instance, backend->surface, NULL);
    backend->surface = VK_NULL_HANDLE;
  }
  if (backend->common_context_acquired) {
    neuron_vk_common_release();
    backend->common_context_acquired = 0;
  }
  backend->device = VK_NULL_HANDLE;
  backend->instance = VK_NULL_HANDLE;
  free(backend);
}

void neuron_graphics_backend_mark_resize(NeuronGraphicsBackend *backend) {
  if (backend != NULL) {
    backend->resize_pending = 1;
  }
}
#endif
