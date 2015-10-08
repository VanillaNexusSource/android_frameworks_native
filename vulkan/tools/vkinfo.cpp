/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <inttypes.h>
#include <sstream>
#include <stdlib.h>
#include <vector>

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#define LOG_TAG "vkinfo"
#include <log/log.h>

namespace {

[[noreturn]] void die(const char* proc, VkResult result) {
    const char* result_str;
    switch (result) {
        // clang-format off
        case VK_SUCCESS: result_str = "VK_SUCCESS"; break;
        case VK_UNSUPPORTED: result_str = "VK_UNSUPPORTED"; break;
        case VK_NOT_READY: result_str = "VK_NOT_READY"; break;
        case VK_TIMEOUT: result_str = "VK_TIMEOUT"; break;
        case VK_EVENT_SET: result_str = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET: result_str = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE: result_str = "VK_INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: result_str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: result_str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: result_str = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST: result_str = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: result_str = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT: result_str = "VK_ERROR_LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: result_str = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: result_str = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        default: result_str = "<unknown VkResult>"; break;
            // clang-format on
    }
    fprintf(stderr, "%s failed: %s (%d)\n", proc, result_str, result);
    exit(1);
}

const char* VkPhysicalDeviceTypeStr(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "OTHER";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "INTEGRATED_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "DISCRETE_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "VIRTUAL_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "<UNKNOWN>";
    }
}

const char* VkQueueFlagBitStr(VkQueueFlagBits bit) {
    switch (bit) {
        case VK_QUEUE_GRAPHICS_BIT:
            return "GRAPHICS";
        case VK_QUEUE_COMPUTE_BIT:
            return "COMPUTE";
        case VK_QUEUE_DMA_BIT:
            return "DMA";
        case VK_QUEUE_SPARSE_MEMMGR_BIT:
            return "SPARSE";
        case VK_QUEUE_EXTENDED_BIT:
            return "EXT";
    }
}

void DumpPhysicalDevice(uint32_t idx, VkPhysicalDevice pdev) {
    VkResult result;
    std::ostringstream strbuf;

    VkPhysicalDeviceProperties props;
    result = vkGetPhysicalDeviceProperties(pdev, &props);
    if (result != VK_SUCCESS)
        die("vkGetPhysicalDeviceProperties", result);
    printf("  %u: \"%s\" (%s) %u.%u.%u/%#x [%04x:%04x]\n", idx,
           props.deviceName, VkPhysicalDeviceTypeStr(props.deviceType),
           (props.apiVersion >> 22) & 0x3FF, (props.apiVersion >> 12) & 0x3FF,
           (props.apiVersion >> 0) & 0xFFF, props.driverVersion, props.vendorId,
           props.deviceId);

    VkPhysicalDeviceMemoryProperties mem_props;
    result = vkGetPhysicalDeviceMemoryProperties(pdev, &mem_props);
    if (result != VK_SUCCESS)
        die("vkGetPhysicalDeviceMemoryProperties", result);
    for (uint32_t heap = 0; heap < mem_props.memoryHeapCount; heap++) {
        if ((mem_props.memoryHeaps[heap].flags &
             VK_MEMORY_HEAP_HOST_LOCAL_BIT) != 0)
            strbuf << "HOST_LOCAL";
        printf("     Heap %u: 0x%" PRIx64 " %s\n", heap,
               mem_props.memoryHeaps[heap].size, strbuf.str().c_str());
        strbuf.str(std::string());

        for (uint32_t type = 0; type < mem_props.memoryTypeCount; type++) {
            if (mem_props.memoryTypes[type].heapIndex != heap)
                continue;
            VkMemoryPropertyFlags flags =
                mem_props.memoryTypes[type].propertyFlags;
            if (flags == VK_MEMORY_PROPERTY_DEVICE_ONLY)
                strbuf << "DEVICE_ONLY";
            if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
                strbuf << "HOST_VISIBLE";
            if ((flags & VK_MEMORY_PROPERTY_HOST_NON_COHERENT_BIT) != 0)
                strbuf << " NON_COHERENT";
            if ((flags & VK_MEMORY_PROPERTY_HOST_UNCACHED_BIT) != 0)
                strbuf << " UNCACHED";
            if ((flags & VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT) != 0)
                strbuf << " WRITE_COMBINED";
            if ((flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0)
                strbuf << " LAZILY_ALLOCATED";
            printf("       Type %u: %s\n", type, strbuf.str().c_str());
            strbuf.str(std::string());
        }
    }

    uint32_t num_queue_families;
    result = vkGetPhysicalDeviceQueueFamilyProperties(pdev, &num_queue_families,
                                                      nullptr);
    if (result != VK_SUCCESS)
        die("vkGetPhysicalDeviceQueueFamilyProperties (count)", result);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        num_queue_families);
    result = vkGetPhysicalDeviceQueueFamilyProperties(
        pdev, &num_queue_families, queue_family_properties.data());
    if (result != VK_SUCCESS)
        die("vkGetPhysicalDeviceQueueFamilyProperties (values)", result);
    for (uint32_t family = 0; family < num_queue_families; family++) {
        const VkQueueFamilyProperties& qprops = queue_family_properties[family];
        const char* sep = "";
        int bit, queue_flags = static_cast<int>(qprops.queueFlags);
        while ((bit = __builtin_ffs(queue_flags)) != 0) {
            VkQueueFlagBits flag = VkQueueFlagBits(1 << (bit - 1));
            strbuf << sep << VkQueueFlagBitStr(flag);
            queue_flags &= ~flag;
            sep = "+";
        }
        printf("     Queue Family %u: %2ux %s timestamps:%s\n", family,
               qprops.queueCount, strbuf.str().c_str(),
               qprops.supportsTimestamps ? "YES" : "NO");
        strbuf.str(std::string());
    }
}

}  // namespace

int main(int /*argc*/, char const* /*argv*/ []) {
    VkResult result;

    VkInstance instance;
    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pAppInfo = nullptr,
        .pAllocCb = nullptr,
        .layerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .extensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };
    result = vkCreateInstance(&create_info, &instance);
    if (result != VK_SUCCESS)
        die("vkCreateInstance", result);

    uint32_t num_physical_devices;
    result =
        vkEnumeratePhysicalDevices(instance, &num_physical_devices, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumeratePhysicalDevices (count)", result);
    std::vector<VkPhysicalDevice> physical_devices(num_physical_devices,
                                                   VK_NULL_HANDLE);
    result = vkEnumeratePhysicalDevices(instance, &num_physical_devices,
                                        physical_devices.data());
    if (result != VK_SUCCESS)
        die("vkEnumeratePhysicalDevices (data)", result);
    if (num_physical_devices != physical_devices.size()) {
        fprintf(stderr,
                "number of physical devices decreased from %zu to %u!\n",
                physical_devices.size(), num_physical_devices);
        physical_devices.resize(num_physical_devices);
    }
    printf("PhysicalDevices:\n");
    for (uint32_t i = 0; i < physical_devices.size(); i++)
        DumpPhysicalDevice(i, physical_devices[i]);

    vkDestroyInstance(instance);

    return 0;
}
