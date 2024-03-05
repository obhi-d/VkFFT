// This file is part of VkFFT
//
// Copyright (C) 2021 - present Dmitrii Tolmachev <dtolm96@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#ifndef VKFFT_MANAGEMEMORY_H
#define VKFFT_MANAGEMEMORY_H
#include "vkFFT/vkFFT_Structs/vkFFT_Structs.h"
#include "vkFFT/vkFFT_AppManagement/vkFFT_DeleteApp.h"

#if(VKFFT_BACKEND==0)

static inline VkFFTResult findMemoryType(VkFFTApplication* app, pfUINT memoryTypeBits, pfUINT memorySize, VkMemoryPropertyFlags properties, uint32_t* memoryTypeIndex) {
	VkPhysicalDeviceMemoryProperties memoryProperties = { 0 };

	app->dispatcher.vkGetPhysicalDeviceMemoryProperties(app->configuration.physicalDevice[0], &memoryProperties);

	for (pfUINT i = 0; i < memoryProperties.memoryTypeCount; ++i) {
		if ((memoryTypeBits & ((pfUINT)1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) && (memoryProperties.memoryHeaps[memoryProperties.memoryTypes[i].heapIndex].size >= memorySize))
		{
			memoryTypeIndex[0] = (uint32_t)i;
			return VKFFT_SUCCESS;
		}
	}
	return VKFFT_ERROR_FAILED_TO_FIND_MEMORY;
}

inline VkResult allocateBufferVulkan(VkFFTApplication* app, VkBuffer* buffer, VkDeviceMemory* memory, VkDeviceSize* offset, void** userData, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceSize size) {
	VkFFTBuffer fftBuffer;
	VkResult res = app->dispatcher.fnAllocateBuffer(app, &fftBuffer, usageFlags, propertyFlags, size);
	if (res != VK_SUCCESS)
		return res;

	*buffer = fftBuffer.buffer;
	*memory = fftBuffer.deviceMemory;
	*offset = fftBuffer.deviceMemoryOffset;
	*userData = fftBuffer.userData;
	return VK_SUCCESS;
}

inline void destroyBufferVulkan(VkFFTApplication* app, VkBuffer* buffer, VkDeviceMemory* memory, VkDeviceSize* offset, void** userData)
{
	VkFFTBuffer fftBuffer = {
		*buffer,
		*memory,
		*offset,
		*userData
	};
	app->dispatcher.fnDestroyBuffer(app, &fftBuffer);
	*buffer = 0;
	*memory = 0;
  *offset = 0;
	*userData = 0;
}

static VkResult allocateBufferVulkanImpl(VkFFTApplication* app, VkFFTBuffer* buffer, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceSize size) {
	VkFFTResult resFFT = VKFFT_SUCCESS;
	VkResult res = VK_SUCCESS;
	uint32_t queueFamilyIndices;
	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndices;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	res = app->dispatcher.vkCreateBuffer(app->configuration.device[0], &bufferCreateInfo, 0, &buffer->buffer);
	if (res != VK_SUCCESS) return res;
	VkMemoryRequirements memoryRequirements = { 0 };
	app->dispatcher.vkGetBufferMemoryRequirements(app->configuration.device[0], buffer->buffer, &memoryRequirements);
	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	resFFT = findMemoryType(app, memoryRequirements.memoryTypeBits, memoryRequirements.size, propertyFlags, &memoryAllocateInfo.memoryTypeIndex);
	if (resFFT != VKFFT_SUCCESS) return VK_ERROR_UNKNOWN;
	res = app->dispatcher.vkAllocateMemory(app->configuration.device[0], &memoryAllocateInfo, 0, &buffer->deviceMemory);
	if (res != VK_SUCCESS) return res;
	res = app->dispatcher.vkBindBufferMemory(app->configuration.device[0], buffer->buffer, buffer->deviceMemory, 0);
	return res;
}
static void destroyBufferVulkanImpl(VkFFTApplication* app, const VkFFTBuffer* buffer) {
	app->dispatcher.vkDestroyBuffer(app->configuration.device[0], buffer->buffer, 0);
	app->dispatcher.vkFreeMemory(app->configuration.device[0], buffer->deviceMemory, 0);
}
static VkResult mapBufferImpl(VkFFTApplication_t* app, const VkFFTBuffer* buffer, VkDeviceSize size, void** pData)
{
	return app->dispatcher.vkMapMemory(app->configuration.device[0], buffer->deviceMemory, buffer->deviceMemoryOffset, size, 0, pData);
}
static void unmapBufferImpl(VkFFTApplication_t* app, const VkFFTBuffer* buffer, VkDeviceSize size)
{
	app->dispatcher.vkUnmapMemory(app->configuration.device[0], buffer->deviceMemory);
}

#endif

static inline VkFFTResult VkFFT_TransferDataFromCPU(VkFFTApplication* app, void* cpu_arr, void* input_buffer, pfUINT transferSize) {
	VkFFTResult resFFT = VKFFT_SUCCESS;
#if(VKFFT_BACKEND==0)
	VkBuffer* buffer = (VkBuffer*)input_buffer;
	VkDeviceSize bufferSize = transferSize;
	VkResult res = VK_SUCCESS;
	VkDeviceSize stagingBufferSize = bufferSize;
	VkFFTBuffer stagingBuffer = { 0 };
	if (!app->configuration.stagingBuffer){
		res = app->dispatcher.fnAllocateBuffer(app, &stagingBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferSize);
		if (res != VK_SUCCESS) return resFFT;
	}else{
		stagingBuffer.buffer = *app->configuration.stagingBuffer;
		stagingBuffer.deviceMemory = *app->configuration.stagingBufferMemory;
		stagingBuffer.deviceMemoryOffset = app->configuration.stagingBufferMemoryOffset ? *app->configuration.stagingBufferMemoryOffset : 0;
	}
	void* data;
	res = app->dispatcher.fnMapBuffer(app, &stagingBuffer, stagingBufferSize, &data);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_MAP_MEMORY;
	memcpy(data, cpu_arr, stagingBufferSize);
	app->dispatcher.fnUnmapBuffer(app, &stagingBuffer, stagingBufferSize);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	commandBufferAllocateInfo.commandPool = app->configuration.commandPool[0];
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = { 0 };
	res = app->dispatcher.vkAllocateCommandBuffers(app->configuration.device[0], &commandBufferAllocateInfo, &commandBuffer);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_ALLOCATE_COMMAND_BUFFERS;
	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	res = app->dispatcher.vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_BEGIN_COMMAND_BUFFER;
	VkBufferCopy copyRegion = { 0 };
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBufferSize;
	app->dispatcher.vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, buffer[0], 1, &copyRegion);
	res = app->dispatcher.vkEndCommandBuffer(commandBuffer);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_END_COMMAND_BUFFER;
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	res = app->dispatcher.vkQueueSubmit(app->configuration.queue[0], 1, &submitInfo, app->configuration.fence[0]);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_SUBMIT_QUEUE;
	res = app->dispatcher.vkWaitForFences(app->configuration.device[0], 1, app->configuration.fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_WAIT_FOR_FENCES;
	res = app->dispatcher.vkResetFences(app->configuration.device[0], 1, app->configuration.fence);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_RESET_FENCES;
	app->dispatcher.vkFreeCommandBuffers(app->configuration.device[0], app->configuration.commandPool[0], 1, &commandBuffer);
	if (!app->configuration.stagingBuffer){
		app->dispatcher.fnDestroyBuffer(app, &stagingBuffer);
	}
#elif(VKFFT_BACKEND==1)
	cudaError_t res = cudaSuccess;
	void* buffer = ((void**)input_buffer)[0];
	res = cudaMemcpy(buffer, cpu_arr, transferSize, cudaMemcpyHostToDevice);
	if (res != cudaSuccess) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
#elif(VKFFT_BACKEND==2)
	hipError_t res = hipSuccess;
	void* buffer = ((void**)input_buffer)[0];
	res = hipMemcpy(buffer, cpu_arr, transferSize, hipMemcpyHostToDevice);
	if (res != hipSuccess) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
#elif(VKFFT_BACKEND==3)
	cl_int res = CL_SUCCESS;
	cl_mem* buffer = (cl_mem*)input_buffer;
	cl_command_queue commandQueue = clCreateCommandQueue(app->configuration.context[0], app->configuration.device[0], 0, &res);
	if (res != CL_SUCCESS) return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_QUEUE;
	res = clEnqueueWriteBuffer(commandQueue, buffer[0], CL_TRUE, 0, transferSize, cpu_arr, 0, NULL, NULL);
	if (res != CL_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
	res = clReleaseCommandQueue(commandQueue);
	if (res != CL_SUCCESS) return VKFFT_ERROR_FAILED_TO_RELEASE_COMMAND_QUEUE;
#elif(VKFFT_BACKEND==4)
	ze_result_t res = ZE_RESULT_SUCCESS;
	void* buffer = ((void**)input_buffer)[0];
	ze_command_queue_desc_t commandQueueCopyDesc = {
			ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
			0,
			app->configuration.commandQueueID,
			0, // index
			0, // flags
			ZE_COMMAND_QUEUE_MODE_DEFAULT,
			ZE_COMMAND_QUEUE_PRIORITY_NORMAL
	};
	ze_command_list_handle_t copyCommandList;
	res = zeCommandListCreateImmediate(app->configuration.context[0], app->configuration.device[0], &commandQueueCopyDesc, &copyCommandList);
	if (res != ZE_RESULT_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_LIST;
	}
	res = zeCommandListAppendMemoryCopy(copyCommandList, buffer, cpu_arr, transferSize, 0, 0, 0);
	if (res != ZE_RESULT_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
	res = zeCommandQueueSynchronize(app->configuration.commandQueue[0], UINT32_MAX);
	if (res != ZE_RESULT_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_SYNCHRONIZE;
	}
#elif(VKFFT_BACKEND==5)
	MTL::Buffer* stagingBuffer = app->configuration.device->newBuffer(cpu_arr, transferSize, MTL::ResourceStorageModeShared);
	MTL::CommandBuffer* copyCommandBuffer = app->configuration.queue->commandBuffer();
	if (copyCommandBuffer == 0) return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_LIST;
	MTL::BlitCommandEncoder* blitCommandEncoder = copyCommandBuffer->blitCommandEncoder();
	if (blitCommandEncoder == 0) return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_LIST;
	MTL::Buffer* buffer = ((MTL::Buffer**)input_buffer)[0];
	blitCommandEncoder->copyFromBuffer((MTL::Buffer*)stagingBuffer, 0, (MTL::Buffer*)buffer, 0, transferSize);
	blitCommandEncoder->endEncoding();
	copyCommandBuffer->commit();
	copyCommandBuffer->waitUntilCompleted();
	blitCommandEncoder->release();
	copyCommandBuffer->release();
	stagingBuffer->release();
#endif
	return resFFT;
}
static inline VkFFTResult VkFFT_TransferDataToCPU(VkFFTApplication* app, void* cpu_arr, void* output_buffer, pfUINT transferSize) {
	VkFFTResult resFFT = VKFFT_SUCCESS;
#if(VKFFT_BACKEND==0)
	VkBuffer* buffer = (VkBuffer*)output_buffer;
	VkDeviceSize bufferSize = transferSize;
	VkResult res = VK_SUCCESS;
	pfUINT stagingBufferSize = bufferSize;
	VkFFTBuffer stagingBuffer = { 0 };
	if (!app->configuration.stagingBuffer){
		res = app->dispatcher.fnAllocateBuffer(app, &stagingBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferSize);
		if (res != VKFFT_SUCCESS) return VKFFT_ERROR_FAILED_TO_ALLOCATE;
	}else{
		stagingBuffer.buffer = *app->configuration.stagingBuffer;
		stagingBuffer.deviceMemory = *app->configuration.stagingBufferMemory;
		stagingBuffer.deviceMemoryOffset = app->configuration.stagingBufferMemoryOffset ? *app->configuration.stagingBufferMemoryOffset : 0;
	}
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	commandBufferAllocateInfo.commandPool = app->configuration.commandPool[0];
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = { 0 };
	res = app->dispatcher.vkAllocateCommandBuffers(app->configuration.device[0], &commandBufferAllocateInfo, &commandBuffer);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_ALLOCATE_COMMAND_BUFFERS;
	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	res = app->dispatcher.vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_BEGIN_COMMAND_BUFFER;
	VkBufferCopy copyRegion = { 0 };
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBufferSize;
	app->dispatcher.vkCmdCopyBuffer(commandBuffer, buffer[0], stagingBuffer.buffer, 1, &copyRegion);
	res = app->dispatcher.vkEndCommandBuffer(commandBuffer);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_END_COMMAND_BUFFER;
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	res = app->dispatcher.vkQueueSubmit(app->configuration.queue[0], 1, &submitInfo, app->configuration.fence[0]);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_SUBMIT_QUEUE;
	res = app->dispatcher.vkWaitForFences(app->configuration.device[0], 1, app->configuration.fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_WAIT_FOR_FENCES;
	res = app->dispatcher.vkResetFences(app->configuration.device[0], 1, app->configuration.fence);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_RESET_FENCES;
	app->dispatcher.vkFreeCommandBuffers(app->configuration.device[0], app->configuration.commandPool[0], 1, &commandBuffer);
	void* data;
	res = app->dispatcher.fnMapBuffer(app, &stagingBuffer, stagingBufferSize, &data);
	if (resFFT != VKFFT_SUCCESS) return resFFT;
	memcpy(cpu_arr, data, stagingBufferSize);
	app->dispatcher.fnUnmapBuffer(app, &stagingBuffer, stagingBufferSize);
	if (!app->configuration.stagingBuffer){
		app->dispatcher.fnDestroyBuffer(app, &stagingBuffer);
	}
#elif(VKFFT_BACKEND==1)
	cudaError_t res = cudaSuccess;
	void* buffer = ((void**)output_buffer)[0];
	res = cudaMemcpy(cpu_arr, buffer, transferSize, cudaMemcpyDeviceToHost);
	if (res != cudaSuccess) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
#elif(VKFFT_BACKEND==2)
	hipError_t res = hipSuccess;
	void* buffer = ((void**)output_buffer)[0];
	res = hipMemcpy(cpu_arr, buffer, transferSize, hipMemcpyDeviceToHost);
	if (res != hipSuccess) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
#elif(VKFFT_BACKEND==3)
	cl_int res = CL_SUCCESS;
	cl_mem* buffer = (cl_mem*)output_buffer;
	cl_command_queue commandQueue = clCreateCommandQueue(app->configuration.context[0], app->configuration.device[0], 0, &res);
	if (res != CL_SUCCESS) return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_QUEUE;
	res = clEnqueueReadBuffer(commandQueue, buffer[0], CL_TRUE, 0, transferSize, cpu_arr, 0, NULL, NULL);
	if (res != CL_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
	res = clReleaseCommandQueue(commandQueue);
	if (res != CL_SUCCESS) return VKFFT_ERROR_FAILED_TO_RELEASE_COMMAND_QUEUE;
#elif(VKFFT_BACKEND==4)
	ze_result_t res = ZE_RESULT_SUCCESS;
	void* buffer = ((void**)output_buffer)[0];
	ze_command_queue_desc_t commandQueueCopyDesc = {
			ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
			0,
			app->configuration.commandQueueID,
			0, // index
			0, // flags
			ZE_COMMAND_QUEUE_MODE_DEFAULT,
			ZE_COMMAND_QUEUE_PRIORITY_NORMAL
	};
	ze_command_list_handle_t copyCommandList;
	res = zeCommandListCreateImmediate(app->configuration.context[0], app->configuration.device[0], &commandQueueCopyDesc, &copyCommandList);
	if (res != ZE_RESULT_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_LIST;
	}
	res = zeCommandListAppendMemoryCopy(copyCommandList, cpu_arr, buffer, transferSize, 0, 0, 0);
	if (res != ZE_RESULT_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_COPY;
	}
	res = zeCommandQueueSynchronize(app->configuration.commandQueue[0], UINT32_MAX);
	if (res != ZE_RESULT_SUCCESS) {
		return VKFFT_ERROR_FAILED_TO_SYNCHRONIZE;
	}
#elif(VKFFT_BACKEND==5)
	MTL::Buffer* stagingBuffer = app->configuration.device->newBuffer(transferSize, MTL::ResourceStorageModeShared);
	MTL::CommandBuffer* copyCommandBuffer = app->configuration.queue->commandBuffer();
	if (copyCommandBuffer == 0) return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_LIST;
	MTL::BlitCommandEncoder* blitCommandEncoder = copyCommandBuffer->blitCommandEncoder();
	if (blitCommandEncoder == 0) return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_LIST;
	MTL::Buffer* buffer = ((MTL::Buffer**)output_buffer)[0];
	blitCommandEncoder->copyFromBuffer((MTL::Buffer*)buffer, 0, (MTL::Buffer*)stagingBuffer, 0, transferSize);
	blitCommandEncoder->endEncoding();
	copyCommandBuffer->commit();
	copyCommandBuffer->waitUntilCompleted();
	blitCommandEncoder->release();
	copyCommandBuffer->release();
	memcpy(cpu_arr, stagingBuffer->contents(), transferSize);
	stagingBuffer->release();
#endif
	return resFFT;
}

#endif
