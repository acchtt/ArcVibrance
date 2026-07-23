#pragma once

#define CTL_APIEXPORT
#include "igcl_api.h"

class IGCLManager
{
public:
    IGCLManager();
    ~IGCLManager();

    bool Initialize();
    void Shutdown();

    bool SetSaturation(double saturation);

private:
    bool FindDisplay();

    ctl_api_handle_t apiHandle = nullptr;
    ctl_display_output_handle_t display = nullptr;

    ctl_pixtx_pipe_get_config_t capabilities = {};

    int32_t matrixBlockIndex = -1;

    bool initialized = false;
};