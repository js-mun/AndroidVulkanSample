# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("mygame")

include(FetchContent)
FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(mygame SHARED
        main.cpp
        Renderer.cpp
        asset_utils.cpp
        VulkanBuffer.cpp
        VulkanContext.cpp
        VulkanPipeline.cpp
        VulkanSwapchain.cpp
        VulkanSync.cpp
        VulkanCommand.cpp
        VulkanDescriptor.cpp
        VulkanMesh.cpp
        VulkanModel.cpp
        Camera.cpp
)

add_library(volk STATIC third_party/volk/volk.c)
target_include_directories(volk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/volk)
target_compile_definitions(volk PUBLIC
        VK_USE_PLATFORM_ANDROID_KHR # 안드로이드 전용 확장 기능 활성화
        VK_NO_PROTOTYPES            # Vulkan 함수를 직접(정적) 호출하지 않고, volk를 통해 동적 로드
)

target_include_directories(mygame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf)
target_compile_definitions(mygame PRIVATE
        TINYGLTF_ANDROID_LOAD_FROM_ASSETS # 안드로이드 에셋 로딩 활성화
)

# Dependencies
find_package(game-activity REQUIRED CONFIG)

# Forces the linker to keep the JNI entry point for GameActivity
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

# Configure libraries CMake uses to link your target library.
target_link_libraries(mygame
        game-activity::game-activity_static
        android
        log
        volk
        dl # Required for volkInitialize (dlopen/dlsym)
        glm::glm
)