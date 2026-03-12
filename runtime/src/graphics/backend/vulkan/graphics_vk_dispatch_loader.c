#include "graphics/backend/vulkan/graphics_vk_internal.h"

#if NPP_GRAPHICS_VK_ENABLED
#include <string.h>

int neuron_vk_load_global(NeuronVulkanDispatch *vk,
                          const NeuronVulkanCommonContext *shared) {
  if (vk == NULL || shared == NULL || shared->vkGetInstanceProcAddr == NULL) {
    neuron_graphics_set_error("Vulkan common dispatch is unavailable");
    return 0;
  }

  memset(vk, 0, sizeof(*vk));
  vk->vkGetInstanceProcAddr = shared->vkGetInstanceProcAddr;
  vk->vkGetDeviceProcAddr = shared->vkGetDeviceProcAddr;
  return 1;
}

int neuron_vk_load_instance(NeuronVulkanDispatch *vk, VkInstance instance) {
  if (vk == NULL || instance == VK_NULL_HANDLE) {
    return 0;
  }

#define LOAD_VK_INSTANCE(name)                                                \
  do {                                                                        \
    vk->name = (PFN_##name)vk->vkGetInstanceProcAddr(instance, #name);        \
    if (vk->name == NULL) {                                                   \
      neuron_graphics_set_error("Missing Vulkan instance symbol: %s", #name); \
      return 0;                                                               \
    }                                                                         \
  } while (0)

  LOAD_VK_INSTANCE(vkDestroyInstance);
  LOAD_VK_INSTANCE(vkCreateWin32SurfaceKHR);
  LOAD_VK_INSTANCE(vkDestroySurfaceKHR);
  LOAD_VK_INSTANCE(vkEnumeratePhysicalDevices);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceProperties);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceMemoryProperties);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
  LOAD_VK_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
  LOAD_VK_INSTANCE(vkCreateDevice);
  LOAD_VK_INSTANCE(vkGetDeviceProcAddr);

#undef LOAD_VK_INSTANCE
  return 1;
}

int neuron_vk_load_device(NeuronVulkanDispatch *vk, VkDevice device) {
  if (vk == NULL || device == VK_NULL_HANDLE ||
      vk->vkGetDeviceProcAddr == NULL) {
    return 0;
  }

#define LOAD_VK_DEVICE(name)                                                \
  do {                                                                      \
    vk->name = (PFN_##name)vk->vkGetDeviceProcAddr(device, #name);          \
    if (vk->name == NULL) {                                                 \
      neuron_graphics_set_error("Missing Vulkan device symbol: %s", #name); \
      return 0;                                                             \
    }                                                                       \
  } while (0)

  LOAD_VK_DEVICE(vkDestroyDevice);
  LOAD_VK_DEVICE(vkGetDeviceQueue);
  LOAD_VK_DEVICE(vkCreateSwapchainKHR);
  LOAD_VK_DEVICE(vkDestroySwapchainKHR);
  LOAD_VK_DEVICE(vkGetSwapchainImagesKHR);
  LOAD_VK_DEVICE(vkCreateImage);
  LOAD_VK_DEVICE(vkDestroyImage);
  LOAD_VK_DEVICE(vkCreateImageView);
  LOAD_VK_DEVICE(vkDestroyImageView);
  LOAD_VK_DEVICE(vkGetImageMemoryRequirements);
  LOAD_VK_DEVICE(vkBindImageMemory);
  LOAD_VK_DEVICE(vkCreateRenderPass);
  LOAD_VK_DEVICE(vkDestroyRenderPass);
  LOAD_VK_DEVICE(vkCreateFramebuffer);
  LOAD_VK_DEVICE(vkDestroyFramebuffer);
  LOAD_VK_DEVICE(vkCreateCommandPool);
  LOAD_VK_DEVICE(vkDestroyCommandPool);
  LOAD_VK_DEVICE(vkAllocateCommandBuffers);
  LOAD_VK_DEVICE(vkResetCommandBuffer);
  LOAD_VK_DEVICE(vkBeginCommandBuffer);
  LOAD_VK_DEVICE(vkEndCommandBuffer);
  LOAD_VK_DEVICE(vkCmdBeginRenderPass);
  LOAD_VK_DEVICE(vkCmdEndRenderPass);
  LOAD_VK_DEVICE(vkQueueSubmit);
  LOAD_VK_DEVICE(vkQueuePresentKHR);
  LOAD_VK_DEVICE(vkAcquireNextImageKHR);
  LOAD_VK_DEVICE(vkCreateSemaphore);
  LOAD_VK_DEVICE(vkDestroySemaphore);
  LOAD_VK_DEVICE(vkCreateFence);
  LOAD_VK_DEVICE(vkDestroyFence);
  LOAD_VK_DEVICE(vkWaitForFences);
  LOAD_VK_DEVICE(vkResetFences);
  LOAD_VK_DEVICE(vkDeviceWaitIdle);
  LOAD_VK_DEVICE(vkCreateShaderModule);
  LOAD_VK_DEVICE(vkDestroyShaderModule);
  LOAD_VK_DEVICE(vkCreateBuffer);
  LOAD_VK_DEVICE(vkDestroyBuffer);
  LOAD_VK_DEVICE(vkGetBufferMemoryRequirements);
  LOAD_VK_DEVICE(vkAllocateMemory);
  LOAD_VK_DEVICE(vkFreeMemory);
  LOAD_VK_DEVICE(vkBindBufferMemory);
  LOAD_VK_DEVICE(vkMapMemory);
  LOAD_VK_DEVICE(vkUnmapMemory);
  LOAD_VK_DEVICE(vkCreateDescriptorSetLayout);
  LOAD_VK_DEVICE(vkDestroyDescriptorSetLayout);
  LOAD_VK_DEVICE(vkCreateDescriptorPool);
  LOAD_VK_DEVICE(vkDestroyDescriptorPool);
  LOAD_VK_DEVICE(vkResetDescriptorPool);
  LOAD_VK_DEVICE(vkAllocateDescriptorSets);
  LOAD_VK_DEVICE(vkUpdateDescriptorSets);
  LOAD_VK_DEVICE(vkCreatePipelineLayout);
  LOAD_VK_DEVICE(vkDestroyPipelineLayout);
  LOAD_VK_DEVICE(vkCreateGraphicsPipelines);
  LOAD_VK_DEVICE(vkDestroyPipeline);
  LOAD_VK_DEVICE(vkCmdBindPipeline);
  LOAD_VK_DEVICE(vkCmdBindDescriptorSets);
  LOAD_VK_DEVICE(vkCmdBindVertexBuffers);
  LOAD_VK_DEVICE(vkCmdBindIndexBuffer);
  LOAD_VK_DEVICE(vkCmdCopyBufferToImage);
  LOAD_VK_DEVICE(vkCmdPipelineBarrier);
  LOAD_VK_DEVICE(vkCmdDraw);
  LOAD_VK_DEVICE(vkCmdDrawIndexed);
  LOAD_VK_DEVICE(vkCmdSetViewport);
  LOAD_VK_DEVICE(vkCmdSetScissor);
  LOAD_VK_DEVICE(vkCreateSampler);
  LOAD_VK_DEVICE(vkDestroySampler);

#undef LOAD_VK_DEVICE
  return 1;
}
#endif
