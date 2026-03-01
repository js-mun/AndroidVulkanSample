#pragma once

#include "IAssetProvider.h"

#include <android/asset_manager.h>

class AndroidAssetProvider final : public IAssetProvider {
public:
    explicit AndroidAssetProvider(AAssetManager* assetManager);

    bool readBinaryFile(const std::string& path, std::vector<uint8_t>& outData) const override;
    bool exists(const std::string& path) const override;

private:
    AAssetManager* mAssetManager = nullptr;
};
