Vulkan Setup
============

OSX
---

1. Download LunarG Vulkan SDK

2. Extract

3. Add ENV variables:
export VULKAN_SDK=/Users/hernan/Development/bin/vulkansdk-macos-1.1.101.0
export VK_ICD_FILENAMES=$VULKAN_SDK/macOS/etc/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=$VULKAN_SDK/macOS/etc/vulkan/explicit_layer.d

4. CMake

5. Build & Run :)

