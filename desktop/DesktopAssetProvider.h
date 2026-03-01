#pragma once

#include "IAssetProvider.h"

#include <string>

class DesktopAssetProvider final : public IAssetProvider {
public:
    explicit DesktopAssetProvider(std::string rootPath);

    bool readBinaryFile(const std::string& path, std::vector<uint8_t>& outData) const override;
    bool exists(const std::string& path) const override;

private:
    std::string resolvePath(const std::string& path) const;

    std::string mRootPath;
};

