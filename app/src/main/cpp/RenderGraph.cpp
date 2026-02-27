// RenderGraph.cpp
#include "RenderGraph.h"
#include "Log.h"
#include <unordered_set>

void RenderGraph::reset() {
    mPasses.clear();
}

void RenderGraph::addPass(Pass pass) {
    mPasses.push_back(std::move(pass));
}

bool RenderGraph::compile(const std::vector<std::string>& externalResources) {
    std::unordered_set<std::string> produced;
    std::unordered_set<std::string> external(externalResources.begin(), externalResources.end());

    for (const auto& pass : mPasses) {
        for (const auto& r : pass.reads) {
            if (produced.find(r) == produced.end() && external.find(r) == external.end()) {
                LOGW("[RG] Pass '%s' reads resource '%s' with no producer (treated as external?)",
                     pass.name.c_str(), r.c_str());
            }
        }
        for (const auto& w : pass.writes) {
            produced.insert(w);
        }
    }

    if (DEBUG_LOG) {
        for (size_t i = 0; i < mPasses.size(); ++i) {
            LOGV("[RG] pass %zu: %s", i, mPasses[i].name.c_str());
        }
    }
    return true;
}

void RenderGraph::execute(VkCommandBuffer cmd) const {
    for (const auto& pass : mPasses) {
        pass.record(cmd);
    }
}
