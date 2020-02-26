#include "rendering_device_win_gnm.h"

RenderingDeviceWinGNM::RenderingDeviceWinGNM() {

}

RenderingDeviceWinGNM::~RenderingDeviceWinGNM() {
    
}

int RenderingDeviceWinGNM::limit_get(Limit p_limit) {
    switch (p_limit) {
		case LIMIT_MAX_TEXTURES_PER_SHADER_STAGE:
			return 256;	// TEMP: This is to solve the current issue with to many gi probe textures in the shader.
		default:
			return RenderingDeviceVulkan::limit_get(p_limit);
	}
}