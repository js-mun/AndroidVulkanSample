#pragma once

#include "volk.h"
#include <functional>
#include <string>
#include <vector>

class RenderGraph {
public:
    struct Pass {
        std::string name;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
        std::function<void(VkCommandBuffer)> record;
    };

    void reset();
    void addPass(Pass pass);
    bool compile(const std::vector<std::string>& externalResources = {});
    void execute(VkCommandBuffer cmd) const;

private:
    std::vector<Pass> mPasses;
};
