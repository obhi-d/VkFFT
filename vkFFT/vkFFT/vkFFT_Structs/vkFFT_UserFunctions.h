// This file is part of VkFFT
//
// Copyright (C) 2021 - present Dmitrii Tolmachev <dtolm96@gmail.com>
//
// Permission is hereby granted free of charge to any person obtaining a copy
// of this software and associated documentation files (the "Software") to deal
// in the Software without restriction including without limitation the rights
// to use copy modify merge publish distribute sublicense and/or sell
// copies of the Software and to permit persons to whom the Software is
// furnished to do so subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND EXPRESS OR
// IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM DAMAGES OR OTHER
// LIABILITY WHETHER IN AN ACTION OF CONTRACT TORT OR OTHERWISE ARISING FROM
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef VKFFT_USER_FUNCTIONS_H
#define VKFFT_USER_FUNCTIONS_H

#if(VKFFT_BACKEND==0)
#include "vulkan/vulkan.h"
#include "glslang_c_interface.h"

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory deviceMemory;
  VkDeviceSize deviceMemoryOffset;
  void* userData;
}VkFFTBuffer;

struct VkFFTApplication_t;
/** User function that allows allocation of a buffer, the allocation may return a pointer in `userAllocData` that will later be passed on DestroyBufferVulkan function  */
typedef VkResult(*pfCreateBufferVulkan)(VkFFTApplication_t* app, VkFFTBuffer* buffer, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceSize size);
typedef void(*pfDestroyBufferVulkan)(VkFFTApplication_t* app, const VkFFTBuffer* buffer);
typedef VkResult(*pfMapBuffer)(VkFFTApplication_t* app, const VkFFTBuffer* buffer, VkDeviceSize size, void** pData);
typedef void(*pfUnmapBuffer)(VkFFTApplication_t* app, const VkFFTBuffer* buffer, VkDeviceSize size);


/**
* @brief Structure to define customization points for vulkan functions and memory allocation.
* For example for applications using VMA they can use these customization points to allow VMA to allocate memory
*/
typedef struct {

  // Possibly vulkan dispatch functions, or custom intercept points for vulkan functions
  // Default inited to global vulkan function calls.
  PFN_vkGetPhysicalDeviceProperties       vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;

  // Possibly vulkan dispatch functions, or custom intercept points for vulkan functions
  // Default inited to global vulkan function calls.
  PFN_vkQueueSubmit                 vkQueueSubmit;
  PFN_vkResetFences                 vkResetFences;
  PFN_vkWaitForFences               vkWaitForFences;
  PFN_vkCreateShaderModule          vkCreateShaderModule;
  PFN_vkDestroyShaderModule         vkDestroyShaderModule;
  PFN_vkCreateComputePipelines      vkCreateComputePipelines;
  PFN_vkDestroyPipeline             vkDestroyPipeline;
  PFN_vkCreatePipelineLayout        vkCreatePipelineLayout;
  PFN_vkDestroyPipelineLayout       vkDestroyPipelineLayout;
  PFN_vkCreateDescriptorSetLayout   vkCreateDescriptorSetLayout;
  PFN_vkDestroyDescriptorSetLayout  vkDestroyDescriptorSetLayout;
  PFN_vkCreateDescriptorPool        vkCreateDescriptorPool;
  PFN_vkDestroyDescriptorPool       vkDestroyDescriptorPool;
  PFN_vkAllocateDescriptorSets      vkAllocateDescriptorSets;
  PFN_vkUpdateDescriptorSets        vkUpdateDescriptorSets;
  PFN_vkAllocateCommandBuffers      vkAllocateCommandBuffers;
  PFN_vkFreeCommandBuffers          vkFreeCommandBuffers;
  PFN_vkBeginCommandBuffer          vkBeginCommandBuffer;
  PFN_vkEndCommandBuffer            vkEndCommandBuffer;
  PFN_vkCmdBindPipeline             vkCmdBindPipeline;
  PFN_vkCmdBindDescriptorSets       vkCmdBindDescriptorSets;
  PFN_vkCmdDispatch                 vkCmdDispatch;
  PFN_vkCmdCopyBuffer               vkCmdCopyBuffer;
  PFN_vkCmdPipelineBarrier          vkCmdPipelineBarrier;
  PFN_vkCmdPushConstants            vkCmdPushConstants;
  PFN_vkCreateBuffer                vkCreateBuffer;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkAllocateMemory              vkAllocateMemory;
  PFN_vkBindBufferMemory            vkBindBufferMemory;
  PFN_vkDestroyBuffer               vkDestroyBuffer;
  PFN_vkFreeMemory                  vkFreeMemory;
  PFN_vkMapMemory                   vkMapMemory;
  PFN_vkUnmapMemory                 vkUnmapMemory;

  // User functions for buffer creation and destruction
  // Default inited to function implementation managed by vkFFT.
  pfCreateBufferVulkan  fnAllocateBuffer;
  pfDestroyBufferVulkan fnDestroyBuffer;
  pfMapBuffer           fnMapBuffer;
  pfUnmapBuffer         fnUnmapBuffer;

  // Data that can be accessed from app parameter of custom user functions
  void* pUserData;
}VkFFTUserFunctions;

#endif

#endif
