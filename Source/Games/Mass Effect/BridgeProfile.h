#pragma once

// Force included into Core's unity build in the Development configurations (see "Mass Effect.vcxproj"): the SR bridge's frame
// steps, timed in game by "SR Bridge Profile" (main.cpp). The macro expands in the bridge's frame steps ("SRBridgeD3D11.h", "SRBridge.cpp"),
// where "frame" is the bridge's frame number (it joins the helper's profile rows; the last step's is the frame's).
struct ID3D11DeviceContext;
void ProfileBridgeStep(ID3D11DeviceContext* context, int step, unsigned long long bridge_frame);
#define SR_BRIDGE_PROFILE(command_list, step) ProfileBridgeStep(command_list, step, frame)
