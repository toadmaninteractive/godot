#include "rendering_device_win_gnm.h"

RenderingDeviceWinGNM::RenderingDeviceWinGNM() {
}

RenderingDeviceWinGNM::~RenderingDeviceWinGNM() {
}

int RenderingDeviceWinGNM::limit_get(Limit p_limit) {
	switch (p_limit) {
		case LIMIT_MAX_TEXTURES_PER_SHADER_STAGE:
			return 128;
		case LIMIT_MAX_UNIFORM_BUFFER_SIZE:
			return 65536;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X:
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y:
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z:
			return 64;
		default:
			ERR_FAIL_V(0);
	}
}