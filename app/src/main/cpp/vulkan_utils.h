//
// Created by mj on 1/11/26.
//

#pragma once

#include <vector>
#include <cstdint>
#include "volk.h"

namespace VulkanUtils {

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code);

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer& buffer, VkDeviceMemory& bufferMemory);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

} // namespace VulkanUtils

