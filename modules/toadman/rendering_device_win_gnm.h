#pragma once

#include "drivers/vulkan/rendering_device_vulkan.h"


class RenderingDeviceWinGNM : public RenderingDeviceVulkan {
public:
    RenderingDeviceWinGNM();
    ~RenderingDeviceWinGNM();

    virtual int limit_get(Limit p_limit);
};