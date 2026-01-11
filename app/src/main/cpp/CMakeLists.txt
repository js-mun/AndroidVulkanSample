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
        vulkan_utils.cpp
)

add_library(volk STATIC third_party/volk/volk.c)
target_include_directories(volk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/volk)
# Define Android platform for volk compilation
target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_ANDROID_KHR)

# Dependencies
find_package(game-activity REQUIRED CONFIG)
#find_library(vulkan-lib vulkan)

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