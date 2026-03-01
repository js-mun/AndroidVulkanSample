#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AssetProvider {
public:
    virtual ~AssetProvider() = default;

    virtual bool readBinaryFile(const std::string& path, std::vector<uint8_t>& outData) const = 0;
    virtual bool exists(const std::string& path) const = 0;
};

