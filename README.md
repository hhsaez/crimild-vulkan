Vulkan Setup
============

OSX
---

1. Download LunarG Vulkan SDK

2. Extract

3. Add ENV variables:
export VULKAN_SDK=/PATH/TO/VULKANSDK/macOS
export VK_ICD_FILENAMES=$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d

4. CMake

5. Build & Run :)

Troubleshooting
===============

When generating Xcode projects using CMake, it might be required to add

1. Product -> Scheme -> Edit Scheme
2. Declare VK_ICD_FILENAMES and VK_LAYER_PATH environmental variables using full paths to files/directories
3. Build and run

