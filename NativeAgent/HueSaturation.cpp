#include "HueSaturation.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    constexpr double Pi = 3.14159265358979323846;

    void MultiplyMatrices(
        const double left[3][3],
        const double right[3][3],
        double output[3][3])
    {
        double temporary[3][3] = {};

        for (int row = 0; row < 3; row++)
        {
            for (int column = 0; column < 3; column++)
            {
                for (int index = 0; index < 3; index++)
                {
                    temporary[row][column] +=
                        left[row][index] * right[index][column];
                }
            }
        }

        std::memcpy(output, temporary, sizeof(temporary));
    }

    void GenerateHueSaturationMatrix(
        double hue,
        double saturation,
        double output[3][3])
    {
        const double hueRadians = hue * Pi / 180.0;
        const double cosine = std::cos(hueRadians);
        const double sine = std::sin(hueRadians);

        const double hueRotation[3][3] =
        {
            { 1.0, 0.0,    0.0 },
            { 0.0, cosine, -sine },
            { 0.0, sine,    cosine }
        };

        const double saturationMatrix[3][3] =
        {
            { 1.0, 0.0,        0.0 },
            { 0.0, saturation, 0.0 },
            { 0.0, 0.0,        saturation }
        };

        const double rgbToYCbCr709[3][3] =
        {
            {  0.2126,  0.7152,  0.0722 },
            { -0.1146, -0.3854,  0.5000 },
            {  0.5000, -0.4542, -0.0458 }
        };

        const double yCbCrToRgb709[3][3] =
        {
            { 1.0000,  0.0000,  1.5748 },
            { 1.0000, -0.1873, -0.4681 },
            { 1.0000,  1.8556,  0.0000 }
        };

        double stage1[3][3] = {};
        double stage2[3][3] = {};
        double stage3[3][3] = {};

        // RGB -> YCbCr
        // Apply hue rotation and saturation to chroma channels
        // YCbCr -> RGB
        MultiplyMatrices(hueRotation, rgbToYCbCr709, stage1);
        MultiplyMatrices(saturationMatrix, stage1, stage2);
        MultiplyMatrices(yCbCrToRgb709, stage2, stage3);

        std::memcpy(output, stage3, sizeof(stage3));
    }
}

ctl_result_t ApplyHueSaturation(
    ctl_display_output_handle_t display,
    ctl_pixtx_pipe_get_config_t* capabilities,
    int32_t matrixBlockIndex,
    double hue,
    double saturation)
{
    if (display == nullptr ||
        capabilities == nullptr ||
        capabilities->pBlockConfigs == nullptr)
    {
        return CTL_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (matrixBlockIndex < 0 ||
        matrixBlockIndex >= static_cast<int32_t>(capabilities->NumBlocks))
    {
        return CTL_RESULT_ERROR_INVALID_ARGUMENT;
    }

    hue = std::clamp(hue, 0.0, 359.0);
    saturation = std::clamp(saturation, 0.75, 1.25);

    ctl_pixtx_block_config_t matrixConfig =
        capabilities->pBlockConfigs[matrixBlockIndex];

    matrixConfig.Size = sizeof(ctl_pixtx_block_config_t);

    GenerateHueSaturationMatrix(
        hue,
        saturation,
        matrixConfig.Config.MatrixConfig.Matrix);

    ctl_pixtx_pipe_set_config_t setConfig = {};

    setConfig.Size =
        sizeof(ctl_pixtx_pipe_set_config_t);

    setConfig.OpertaionType =
        CTL_PIXTX_CONFIG_OPERTAION_TYPE_SET_CUSTOM;

    setConfig.NumBlocks = 1;
    setConfig.pBlockConfigs = &matrixConfig;

    return ctlPixelTransformationSetConfig(
        display,
        &setConfig);
}