cmake_minimum_required(VERSION 3.22)

project(mygame_desktop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

set(COMMON_DIR ${CMAKE_SOURCE_DIR}/../app/src/main/cpp)

add_library(volk STATIC ${COMMON_DIR}/third_party/volk/volk.c)
target_include_directories(volk PUBLIC ${COMMON_DIR}/third_party/volk)
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)

add_library(mygame_core STATIC
        ${COMMON_DIR}/Camera.cpp
        ${COMMON_DIR}/DescriptorLayouts.cpp
        ${COMMON_DIR}/GlobalDescriptor.cpp
        ${COMMON_DIR}/Renderer.cpp
        ${COMMON_DIR}/RenderGraph.cpp
        ${COMMON_DIR}/ShadowResources.cpp
        ${COMMON_DIR}/VulkanBuffer.cpp
        ${COMMON_DIR}/VulkanCommand.cpp
        ${COMMON_DIR}/VulkanContext.cpp
        ${COMMON_DIR}/VulkanDescriptor.cpp
        ${COMMON_DIR}/VulkanMesh.cpp
        ${COMMON_DIR}/VulkanModel.cpp
        ${COMMON_DIR}/VulkanPipeline.cpp
        ${COMMON_DIR}/VulkanSwapchain.cpp
        ${COMMON_DIR}/VulkanSync.cpp
        ${COMMON_DIR}/VulkanTexture.cpp
)

target_include_directories(mygame_core
        PUBLIC
        ${COMMON_DIR}
        ${COMMON_DIR}/third_party/tinygltf
)
target_include_directories(mygame_core
        SYSTEM PUBLIC
        ${COMMON_DIR}/third_party/VulkanMemoryAllocator/include
)
target_compile_definitions(mygame_core PRIVATE DEBUG_LOG=1)
target_link_libraries(mygame_core PUBLIC volk glm::glm dl)

add_executable(mygame_desktop
        desktop_main.cpp
        DesktopAssetProvider.cpp
        DesktopInputProvider.cpp
        GlfwSurfaceProvider.cpp
)

target_include_directories(mygame_desktop PRIVATE ${COMMON_DIR})
target_link_libraries(mygame_desktop
        PRIVATE
        mygame_core
        glfw
        Vulkan::Vulkan
)
