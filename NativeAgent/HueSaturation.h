#pragma once

#define CTL_APIEXPORT
#include "igcl_api.h"

ctl_result_t ApplyHueSaturation(
    ctl_display_output_handle_t display,
    ctl_pixtx_pipe_get_config_t* capabilities,
    int32_t matrixBlockIndex,
    double hue,
    double saturation);