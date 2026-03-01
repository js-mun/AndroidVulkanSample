#include "AndroidAssetProvider.h"
#include "Log.h"

AndroidAssetProvider::AndroidAssetProvider(AAssetManager* assetManager)
        : mAssetManager(assetManager) {
}

bool AndroidAssetProvider::readBinaryFile(const std::string& path,
                                          std::vector<uint8_t>& outData) const {
    outData.clear();
    if (!mAssetManager) {
        LOGE("Asset manager is null");
        return false;
    }

    AAsset* asset = AAssetManager_open(mAssetManager, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        return false;
    }

    const off_t size = AAsset_getLength(asset);
    if (size <= 0) {
        AAsset_close(asset);
        return false;
    }

    outData.resize(static_cast<size_t>(size));
    const int readSize = AAsset_read(asset, outData.data(), size);
    AAsset_close(asset);

    if (readSize < 0 || static_cast<size_t>(readSize) != outData.size()) {
        outData.clear();
        return false;
    }
    return true;
}

bool AndroidAssetProvider::exists(const std::string& path) const {
    if (!mAssetManager) {
        return false;
    }
    AAsset* asset = AAssetManager_open(mAssetManager, path.c_str(), AASSET_MODE_UNKNOWN);
    if (!asset) {
        return false;
    }
    AAsset_close(asset);
    return true;
}

