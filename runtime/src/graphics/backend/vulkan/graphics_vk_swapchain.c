#include "graphics/backend/vulkan/graphics_vk_internal.h"

#if NPP_GRAPHICS_VK_ENABLED
#include <stdlib.h>
#include <string.h>

void neuron_graphics_backend_destroy_swapchain(NeuronGraphicsBackend *backend) {
  if (backend == NULL || backend->device == VK_NULL_HANDLE) {
    return;
  }

  if (backend->framebuffers != NULL &&
      backend->vk.vkDestroyFramebuffer != NULL) {
    for (uint32_t i = 0; i < backend->swapchain_image_count; ++i) {
      if (backend->framebuffers[i] != VK_NULL_HANDLE) {
        backend->vk.vkDestroyFramebuffer(backend->device,
                                         backend->framebuffers[i], NULL);
      }
    }
  }
  free(backend->framebuffers);
  backend->framebuffers = NULL;

  if (backend->swapchain_image_views != NULL &&
      backend->vk.vkDestroyImageView != NULL) {
    for (uint32_t i = 0; i < backend->swapchain_image_count; ++i) {
      if (backend->swapchain_image_views[i] != VK_NULL_HANDLE) {
        backend->vk.vkDestroyImageView(backend->device,
                                       backend->swapchain_image_views[i], NULL);
      }
    }
  }
  free(backend->swapchain_image_views);
  backend->swapchain_image_views = NULL;

  free(backend->swapchain_images);
  backend->swapchain_images = NULL;
  backend->swapchain_image_count = 0;

  if (backend->render_pass != VK_NULL_HANDLE &&
      backend->vk.vkDestroyRenderPass != NULL) {
    backend->vk.vkDestroyRenderPass(backend->device, backend->render_pass,
                                    NULL);
    backend->render_pass = VK_NULL_HANDLE;
  }

  if (backend->swapchain != VK_NULL_HANDLE &&
      backend->vk.vkDestroySwapchainKHR != NULL) {
    backend->vk.vkDestroySwapchainKHR(backend->device, backend->swapchain,
                                      NULL);
    backend->swapchain = VK_NULL_HANDLE;
  }
}

static VkSurfaceFormatKHR
neuron_graphics_choose_surface_format(const VkSurfaceFormatKHR *formats,
                                      uint32_t count) {
  VkSurfaceFormatKHR chosen = formats[0];
  for (uint32_t i = 0; i < count; ++i) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = formats[i];
      break;
    }
  }
  return chosen;
}

static VkPresentModeKHR
neuron_graphics_choose_present_mode(const VkPresentModeKHR *modes,
                                    uint32_t count) {
  VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR;
  for (uint32_t i = 0; i < count; ++i) {
    if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      chosen = VK_PRESENT_MODE_MAILBOX_KHR;
      break;
    }
  }
  return chosen;
}

static VkExtent2D
neuron_graphics_choose_extent(const VkSurfaceCapabilitiesKHR *caps,
                              int32_t width, int32_t height) {
  VkExtent2D extent;
  if (caps->currentExtent.width != UINT32_MAX) {
    return caps->currentExtent;
  }

  extent.width = width <= 0 ? 1u : (uint32_t)width;
  extent.height = height <= 0 ? 1u : (uint32_t)height;
  if (extent.width < caps->minImageExtent.width) {
    extent.width = caps->minImageExtent.width;
  }
  if (extent.width > caps->maxImageExtent.width) {
    extent.width = caps->maxImageExtent.width;
  }
  if (extent.height < caps->minImageExtent.height) {
    extent.height = caps->minImageExtent.height;
  }
  if (extent.height > caps->maxImageExtent.height) {
    extent.height = caps->maxImageExtent.height;
  }
  return extent;
}

int neuron_graphics_backend_create_swapchain(NeuronGraphicsBackend *backend) {
  if (backend == NULL || backend->device == VK_NULL_HANDLE ||
      backend->surface == VK_NULL_HANDLE) {
    return 0;
  }

  VkSurfaceCapabilitiesKHR caps;
  VkResult result = backend->vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      backend->physical_device, backend->surface, &caps);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error(
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed (%d)", (int)result);
    return 0;
  }

  uint32_t format_count = 0;
  result = backend->vk.vkGetPhysicalDeviceSurfaceFormatsKHR(
      backend->physical_device, backend->surface, &format_count, NULL);
  if (result != VK_SUCCESS || format_count == 0) {
    neuron_graphics_set_error("Failed to query surface formats (%d)",
                              (int)result);
    return 0;
  }

  VkSurfaceFormatKHR *formats =
      (VkSurfaceFormatKHR *)calloc(format_count, sizeof(VkSurfaceFormatKHR));
  if (formats == NULL) {
    neuron_graphics_set_error("Out of memory allocating surface formats");
    return 0;
  }
  result = backend->vk.vkGetPhysicalDeviceSurfaceFormatsKHR(
      backend->physical_device, backend->surface, &format_count, formats);
  if (result != VK_SUCCESS) {
    free(formats);
    neuron_graphics_set_error("Failed to read surface formats (%d)",
                              (int)result);
    return 0;
  }

  uint32_t present_mode_count = 0;
  result = backend->vk.vkGetPhysicalDeviceSurfacePresentModesKHR(
      backend->physical_device, backend->surface, &present_mode_count, NULL);
  if (result != VK_SUCCESS || present_mode_count == 0) {
    free(formats);
    neuron_graphics_set_error("Failed to query present modes (%d)",
                              (int)result);
    return 0;
  }

  VkPresentModeKHR *present_modes =
      (VkPresentModeKHR *)calloc(present_mode_count, sizeof(VkPresentModeKHR));
  if (present_modes == NULL) {
    free(formats);
    neuron_graphics_set_error("Out of memory allocating present modes");
    return 0;
  }
  result = backend->vk.vkGetPhysicalDeviceSurfacePresentModesKHR(
      backend->physical_device, backend->surface, &present_mode_count,
      present_modes);
  if (result != VK_SUCCESS) {
    free(present_modes);
    free(formats);
    neuron_graphics_set_error("Failed to read present modes (%d)", (int)result);
    return 0;
  }

  VkSurfaceFormatKHR surface_format =
      neuron_graphics_choose_surface_format(formats, format_count);
  VkPresentModeKHR present_mode =
      neuron_graphics_choose_present_mode(present_modes, present_mode_count);
  VkExtent2D extent = neuron_graphics_choose_extent(
      &caps, backend->window->width, backend->window->height);

  free(present_modes);
  free(formats);

  uint32_t image_count = caps.minImageCount + 1u;
  if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
    image_count = caps.maxImageCount;
  }

  uint32_t queue_families[2];
  uint32_t queue_family_count = 0;
  queue_families[queue_family_count++] = backend->graphics_queue_family;
  if (backend->present_queue_family != backend->graphics_queue_family) {
    queue_families[queue_family_count++] = backend->present_queue_family;
  }

  VkSwapchainCreateInfoKHR swapchain_info;
  memset(&swapchain_info, 0, sizeof(swapchain_info));
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.surface = backend->surface;
  swapchain_info.minImageCount = image_count;
  swapchain_info.imageFormat = surface_format.format;
  swapchain_info.imageColorSpace = surface_format.colorSpace;
  swapchain_info.imageExtent = extent;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (queue_family_count > 1) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = queue_family_count;
    swapchain_info.pQueueFamilyIndices = queue_families;
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  swapchain_info.preTransform = caps.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = present_mode;
  swapchain_info.clipped = VK_TRUE;
  swapchain_info.oldSwapchain = VK_NULL_HANDLE;

  result = backend->vk.vkCreateSwapchainKHR(backend->device, &swapchain_info,
                                            NULL, &backend->swapchain);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateSwapchainKHR failed (%d)", (int)result);
    return 0;
  }

  result = backend->vk.vkGetSwapchainImagesKHR(
      backend->device, backend->swapchain, &image_count, NULL);
  if (result != VK_SUCCESS || image_count == 0) {
    neuron_graphics_set_error("vkGetSwapchainImagesKHR(count) failed (%d)",
                              (int)result);
    return 0;
  }

  backend->swapchain_images = (VkImage *)calloc(image_count, sizeof(VkImage));
  backend->swapchain_image_views =
      (VkImageView *)calloc(image_count, sizeof(VkImageView));
  backend->framebuffers =
      (VkFramebuffer *)calloc(image_count, sizeof(VkFramebuffer));
  if (backend->swapchain_images == NULL ||
      backend->swapchain_image_views == NULL || backend->framebuffers == NULL) {
    neuron_graphics_set_error("Out of memory allocating swapchain resources");
    return 0;
  }

  result = backend->vk.vkGetSwapchainImagesKHR(backend->device,
                                               backend->swapchain, &image_count,
                                               backend->swapchain_images);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkGetSwapchainImagesKHR(data) failed (%d)",
                              (int)result);
    return 0;
  }

  backend->swapchain_image_count = image_count;
  backend->swapchain_format = surface_format.format;
  backend->swapchain_extent = extent;

  for (uint32_t i = 0; i < image_count; ++i) {
    VkImageViewCreateInfo view_info;
    memset(&view_info, 0, sizeof(view_info));
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = backend->swapchain_images[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = backend->swapchain_format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    result = backend->vk.vkCreateImageView(backend->device, &view_info, NULL,
                                           &backend->swapchain_image_views[i]);
    if (result != VK_SUCCESS) {
      neuron_graphics_set_error("vkCreateImageView failed (%d)", (int)result);
      return 0;
    }
  }

  VkAttachmentDescription color_attachment;
  memset(&color_attachment, 0, sizeof(color_attachment));
  color_attachment.format = backend->swapchain_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_ref;
  memset(&color_ref, 0, sizeof(color_ref));
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass;
  memset(&subpass, 0, sizeof(subpass));
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

  VkSubpassDependency dependency;
  memset(&dependency, 0, sizeof(dependency));
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo pass_info;
  memset(&pass_info, 0, sizeof(pass_info));
  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass_info.attachmentCount = 1;
  pass_info.pAttachments = &color_attachment;
  pass_info.subpassCount = 1;
  pass_info.pSubpasses = &subpass;
  pass_info.dependencyCount = 1;
  pass_info.pDependencies = &dependency;
  result = backend->vk.vkCreateRenderPass(backend->device, &pass_info, NULL,
                                          &backend->render_pass);
  if (result != VK_SUCCESS) {
    neuron_graphics_set_error("vkCreateRenderPass failed (%d)", (int)result);
    return 0;
  }

  for (uint32_t i = 0; i < image_count; ++i) {
    VkImageView attachments[] = {backend->swapchain_image_views[i]};
    VkFramebufferCreateInfo fb_info;
    memset(&fb_info, 0, sizeof(fb_info));
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = backend->render_pass;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = attachments;
    fb_info.width = backend->swapchain_extent.width;
    fb_info.height = backend->swapchain_extent.height;
    fb_info.layers = 1;
    result = backend->vk.vkCreateFramebuffer(backend->device, &fb_info, NULL,
                                             &backend->framebuffers[i]);
    if (result != VK_SUCCESS) {
      neuron_graphics_set_error("vkCreateFramebuffer failed (%d)", (int)result);
      return 0;
    }
  }

  return 1;
}

int neuron_graphics_backend_recreate_swapchain(NeuronGraphicsBackend *backend) {
  if (backend == NULL || backend->device == VK_NULL_HANDLE) {
    return 0;
  }
  if (backend->window->width <= 0 || backend->window->height <= 0) {
    return 1;
  }

  backend->vk.vkDeviceWaitIdle(backend->device);
  if (backend->pipelines != NULL) {
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
  neuron_graphics_backend_destroy_swapchain(backend);
  if (!neuron_graphics_backend_create_swapchain(backend)) {
    return 0;
  }
  if (!neuron_graphics_backend_create_pipeline(backend)) {
    return 0;
  }
  backend->resize_pending = 0;
  return 1;
}
#endif
