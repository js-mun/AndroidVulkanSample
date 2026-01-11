//
// Created by mj on 1/11/26.
//

#pragma once

#include <vector>
#include <cstdint>
#include "volk.h"

namespace VulkanUtils {

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code);

} // namespace VulkanUtils

