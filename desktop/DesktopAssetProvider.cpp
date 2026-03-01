#include "DesktopAssetProvider.h"

#include <filesystem>
#include <fstream>
#include <iterator>

DesktopAssetProvider::DesktopAssetProvider(std::string rootPath)
        : mRootPath(std::move(rootPath)) {
}

std::string DesktopAssetProvider::resolvePath(const std::string& path) const {
    if (path.empty()) {
        return mRootPath;
    }
    if (!path.empty() && path[0] == '/') {
        return path;
    }
    if (mRootPath.empty()) {
        return path;
    }
    return mRootPath + "/" + path;
}

bool DesktopAssetProvider::readBinaryFile(const std::string& path, std::vector<uint8_t>& outData) const {
    outData.clear();

    const std::string fullPath = resolvePath(path);
    std::ifstream in(fullPath, std::ios::binary);
    if (!in) {
        return false;
    }

    in.unsetf(std::ios::skipws);
    outData.insert(outData.begin(),
                   std::istream_iterator<uint8_t>(in),
                   std::istream_iterator<uint8_t>());
    return !outData.empty();
}

bool DesktopAssetProvider::exists(const std::string& path) const {
    return std::filesystem::exists(resolvePath(path));
}

