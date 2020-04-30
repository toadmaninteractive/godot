#pragma once

#include "drivers/vulkan/rendering_device_vulkan.h"

class RenderingDeviceWinGNM : public RenderingDeviceVulkan {
public:
	virtual int limit_get(Limit p_limit) override;

	virtual RenderingDevice *create_local_device() override;
};