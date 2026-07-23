#define CTL_APIEXPORT
#include "igcl_api.h"
#include "HueSaturation.h"

#include <conio.h>
#include <iomanip>
#include <iostream>
#include <vector>

bool GetPixelTransformCapabilities(
    ctl_display_output_handle_t display,
    ctl_pixtx_pipe_get_config_t& caps,
    int32_t& matrixBlockIndex)
{
    caps = {};
    caps.Size = sizeof(caps);
    caps.QueryType =
        CTL_PIXTX_CONFIG_QUERY_TYPE_CAPABILITY;

    ctl_result_t result =
        ctlPixelTransformationGetConfig(
            display,
            &caps);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "First GetConfig failed: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        return false;
    }

    if (caps.NumBlocks == 0)
    {
        std::cout
            << "No pixel transformation blocks found."
            << std::endl;

        return false;
    }

    caps.pBlockConfigs =
        new ctl_pixtx_block_config_t[caps.NumBlocks]{};

    result =
        ctlPixelTransformationGetConfig(
            display,
            &caps);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "Second GetConfig failed: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        delete[] caps.pBlockConfigs;
        caps.pBlockConfigs = nullptr;

        return false;
    }

    matrixBlockIndex = -1;

    for (uint8_t i = 0; i < caps.NumBlocks; i++)
    {
        std::cout
            << "Block "
            << static_cast<int>(i)
            << " Type="
            << static_cast<int>(
                caps.pBlockConfigs[i].BlockType)
            << " Id="
            << caps.pBlockConfigs[i].BlockId
            << std::endl;

        if (caps.pBlockConfigs[i].BlockType ==
            CTL_PIXTX_BLOCK_TYPE_3X3_MATRIX)
        {
            matrixBlockIndex =
                static_cast<int32_t>(i);
        }
    }

    if (matrixBlockIndex < 0)
    {
        std::cout
            << "No 3x3 matrix block found."
            << std::endl;

        delete[] caps.pBlockConfigs;
        caps.pBlockConfigs = nullptr;

        return false;
    }

    std::cout
        << "Using matrix block "
        << matrixBlockIndex
        << std::endl;

    return true;
}

int main()
{
    ctl_result_t result;

    ctl_init_args_t initArgs = {};
    initArgs.Size = sizeof(initArgs);
    initArgs.AppVersion =
        CTL_MAKE_VERSION(
            CTL_IMPL_MAJOR_VERSION,
            CTL_IMPL_MINOR_VERSION);

    ctl_api_handle_t apiHandle = nullptr;

    result =
        ctlInit(
            &initArgs,
            &apiHandle);

    if (result != CTL_RESULT_SUCCESS)
    {
        std::cout
            << "ctlInit failed: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;

        return 1;
    }

    uint32_t adapterCount = 0;

    result =
        ctlEnumerateDevices(
            apiHandle,
            &adapterCount,
            nullptr);

    if (result != CTL_RESULT_SUCCESS ||
        adapterCount == 0)
    {
        std::cout
            << "No Intel adapters found."
            << std::endl;

        ctlClose(apiHandle);
        return 1;
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
            << "Could not enumerate adapters."
            << std::endl;

        ctlClose(apiHandle);
        return 1;
    }

    std::cout
        << "Adapters found: "
        << adapterCount
        << std::endl;

    ctl_display_output_handle_t selectedDisplay =
        nullptr;

    for (uint32_t adapterIndex = 0;
        adapterIndex < adapterCount;
        adapterIndex++)
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

        std::vector<ctl_display_output_handle_t>
            displays(displayCount);

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
            displayIndex++)
        {
            ctl_display_properties_t properties = {};
            properties.Size = sizeof(properties);

            result =
                ctlGetDisplayProperties(
                    displays[displayIndex],
                    &properties);

            if (result != CTL_RESULT_SUCCESS)
            {
                continue;
            }

            bool active =
                (properties.DisplayConfigFlags &
                    CTL_DISPLAY_CONFIG_FLAG_DISPLAY_ACTIVE)
                != 0;

            bool attached =
                (properties.DisplayConfigFlags &
                    CTL_DISPLAY_CONFIG_FLAG_DISPLAY_ATTACHED)
                != 0;

            std::cout
                << "Display "
                << displayIndex
                << " Active="
                << active
                << " Attached="
                << attached
                << std::endl;

            // Select the first active and attached display.
            if (selectedDisplay == nullptr &&
                active &&
                attached)
            {
                selectedDisplay =
                    displays[displayIndex];

                std::cout
                    << "Selected Display "
                    << displayIndex
                    << std::endl;
            }
        }
    }

    if (selectedDisplay == nullptr)
    {
        std::cout
            << "No active display found."
            << std::endl;

        ctlClose(apiHandle);
        return 1;
    }

    ctl_pixtx_pipe_get_config_t capabilities = {};
    int32_t matrixBlockIndex = -1;

    if (!GetPixelTransformCapabilities(
        selectedDisplay,
        capabilities,
        matrixBlockIndex))
    {
        ctlClose(apiHandle);
        return 1;
    }

    double saturation = 1.0;
    bool running = true;

    std::cout << "\n";
    std::cout << "ArcVibrance controls\n";
    std::cout << "====================\n";
    std::cout << "1  = saturation 0.75\n";
    std::cout << "2  = saturation 1.00\n";
    std::cout << "3  = saturation 1.10\n";
    std::cout << "4  = saturation 1.20\n";
    std::cout << "5  = saturation 1.25\n";
    std::cout << "+  = increase by 0.05\n";
    std::cout << "-  = decrease by 0.05\n";
    std::cout << "R  = reset to 1.00\n";
    std::cout << "Q  = quit and reset\n";
    std::cout << "\n";

    while (running)
    {
        // Wait here until the user presses one key.
        int key = _getch();

        bool applyNewValue = true;

        switch (key)
        {
        case '1':
            saturation = 0.75;
            break;

        case '2':
            saturation = 1.00;
            break;

        case '3':
            saturation = 1.10;
            break;

        case '4':
            saturation = 1.20;
            break;

        case '5':
            saturation = 1.25;
            break;

        case '+':
        case '=':
            saturation += 0.05;
            break;

        case '-':
        case '_':
            saturation -= 0.05;
            break;

        case 'r':
        case 'R':
            saturation = 1.00;
            break;

        case 'q':
        case 'Q':
            running = false;
            applyNewValue = false;
            break;

        default:
            applyNewValue = false;
            break;
        }

        if (!applyNewValue)
        {
            continue;
        }

        if (saturation < 0.00)
        {
            saturation = 0.00;
        }

        if (saturation > 2.00)
        {
            saturation = 2.00;
        }

        result =
            ApplyHueSaturation(
                selectedDisplay,
                &capabilities,
                matrixBlockIndex,
                0.0,
                saturation);

        std::cout
            << "Saturation: "
            << std::fixed
            << std::setprecision(2)
            << saturation
            << " | Result: 0x"
            << std::hex
            << result
            << std::dec
            << std::endl;
    }

    std::cout
        << "Restoring normal saturation..."
        << std::endl;

    ApplyHueSaturation(
        selectedDisplay,
        &capabilities,
        matrixBlockIndex,
        0.0,
        1.0);

    delete[] capabilities.pBlockConfigs;
    capabilities.pBlockConfigs = nullptr;

    ctlClose(apiHandle);

    return 0;
}