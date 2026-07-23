#include "IGCLManager.h"
#include "HueSaturation.h"

#include <algorithm>
#include <iostream>
#include <vector>

IGCLManager::IGCLManager()
{
    capabilities = {};
}

IGCLManager::~IGCLManager()
{
    Shutdown();
}

bool IGCLManager::Initialize()
{
    if (initialized)
    {
        return true;
    }

    ctl_init_args_t initArgs = {};
    initArgs.Size = sizeof(initArgs);

    initArgs.AppVersion =
        CTL_MAKE_VERSION(
            CTL_IMPL_MAJOR_VERSION,
            CTL_IMPL_MINOR_VERSION);

    ctl_result_t result =
        ctlInit(
            &initArgs,
            &apiHandle);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "ctlInit failed. Result: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        apiHandle = nullptr;
        return false;
    }

    if (!FindDisplay())
    {
        std::cout
            << "No compatible active display was found."
            << std::endl;

        Shutdown();
        return false;
    }

    capabilities = {};
    capabilities.Size =
        sizeof(capabilities);

    capabilities.QueryType =
        CTL_PIXTX_CONFIG_QUERY_TYPE_CAPABILITY;

    // First call: ask IGCL how many blocks exist.
    result =
        ctlPixelTransformationGetConfig(
            display,
            &capabilities);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "First pixel transformation query failed. Result: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        Shutdown();
        return false;
    }

    if (capabilities.NumBlocks == 0)
    {
        std::cout
            << "The selected display has no pixel transformation blocks."
            << std::endl;

        Shutdown();
        return false;
    }

    capabilities.pBlockConfigs =
        new ctl_pixtx_block_config_t[
            capabilities.NumBlocks]{};

    // Second call: retrieve the actual block information.
    result =
        ctlPixelTransformationGetConfig(
            display,
            &capabilities);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "Second pixel transformation query failed. Result: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        Shutdown();
        return false;
    }

    matrixBlockIndex = -1;

    for (uint8_t index = 0;
         index < capabilities.NumBlocks;
         ++index)
    {
        const ctl_pixtx_block_config_t& block =
            capabilities.pBlockConfigs[index];

        std::cout
            << "Pixel block "
            << static_cast<int>(index)
            << " Type="
            << static_cast<int>(block.BlockType)
            << " Id="
            << block.BlockId
            << std::endl;

        if (block.BlockType ==
            CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX)
        {
            matrixBlockIndex =
                static_cast<int32_t>(index);

            break;
        }
    }

    if (matrixBlockIndex < 0)
    {
        std::cout
            << "No 3x3 color matrix block was found."
            << std::endl;

        Shutdown();
        return false;
    }

    initialized = true;

    std::cout
        << "IGCLManager initialized successfully."
        << std::endl;

    return true;
}

bool IGCLManager::FindDisplay()
{
    if (apiHandle == nullptr)
    {
        return false;
    }

    uint32_t adapterCount = 0;

    ctl_result_t result =
        ctlEnumerateDevices(
            apiHandle,
            &adapterCount,
            nullptr);

    if (result != CTL_RESULT_SUCCESS ||
        adapterCount == 0)
    {
        std::cout
            << "No Intel graphics adapters were found."
            << std::endl;

        return false;
    }

    std::vector<ctl_device_adapter_handle_t> adapters(
        adapterCount);

    result =
        ctlEnumerateDevices(
            apiHandle,
            &adapterCount,
            adapters.data());

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "Could not enumerate Intel graphics adapters."
            << std::endl;

        return false;
    }

    for (uint32_t adapterIndex = 0;
         adapterIndex < adapterCount;
         ++adapterIndex)
    {
        uint32_t displayCount = 0;

        result =
            ctlEnumerateDisplayOutputs(
                adapters[adapterIndex],
                &displayCount,
                nullptr);

        if (result != CTL_RESULT_SUCCESS ||
            displayCount == 0)
        {
            continue;
        }

        std::vector<ctl_display_output_handle_t> displays(
            displayCount);

        result =
            ctlEnumerateDisplayOutputs(
                adapters[adapterIndex],
                &displayCount,
                displays.data());

        if (result != CTL_RESULT_SUCCESS)
        {
            continue;
        }

        for (uint32_t displayIndex = 0;
             displayIndex < displayCount;
             ++displayIndex)
        {
            ctl_display_properties_t properties = {};
            properties.Size =
                sizeof(properties);

            result =
                ctlGetDisplayProperties(
                    displays[displayIndex],
                    &properties);

            if (result != CTL_RESULT_SUCCESS)
            {
                continue;
            }

            const bool active =
                (properties.DisplayConfigFlags &
                 CTL_DISPLAY_CONFIG_FLAG_DISPLAY_ACTIVE) != 0;

            const bool attached =
                (properties.DisplayConfigFlags &
                 CTL_DISPLAY_CONFIG_FLAG_DISPLAY_ATTACHED) != 0;

            std::cout
                << "Display "
                << displayIndex
                << " Active="
                << active
                << " Attached="
                << attached
                << std::endl;

            if (active && attached)
            {
                display =
                    displays[displayIndex];

                std::cout
                    << "Selected display "
                    << displayIndex
                    << std::endl;

                return true;
            }
        }
    }

    return false;
}

bool IGCLManager::SetSaturation(
    double saturation)
{
    if (!initialized ||
        display == nullptr ||
        capabilities.pBlockConfigs == nullptr ||
        matrixBlockIndex < 0)
    {
        return false;
    }

    // Protect the matrix function from extreme values.
    saturation =
        std::clamp(
            saturation,
            0.0,
            3.0);

    const ctl_result_t result =
        ApplyHueSaturation(
            display,
            &capabilities,
            matrixBlockIndex,
            0.0,
            saturation);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "SetSaturation failed. Result: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        return false;
    }

    return true;
}

void IGCLManager::Shutdown()
{
    /*
        Restore normal saturation before releasing IGCL.

        Only do this when initialization completed, because an incomplete
        capability query may not have a valid block configuration.
    */
    if (initialized &&
        display != nullptr &&
        capabilities.pBlockConfigs != nullptr &&
        matrixBlockIndex >= 0)
    {
        ApplyHueSaturation(
            display,
            &capabilities,
            matrixBlockIndex,
            0.0,
            1.0);
    }

    if (capabilities.pBlockConfigs != nullptr)
    {
        delete[] capabilities.pBlockConfigs;
        capabilities.pBlockConfigs = nullptr;
    }

    capabilities = {};
    matrixBlockIndex = -1;
    display = nullptr;
    initialized = false;

    if (apiHandle != nullptr)
    {
        ctlClose(apiHandle);
        apiHandle = nullptr;
    }
}