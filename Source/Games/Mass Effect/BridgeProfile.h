#pragma once

// Force included into Core's unity build in the Development configurations (see "Mass Effect.vcxproj"): the SR bridge's frame
// steps, timed in game by "SR Bridge Profile" (main.cpp). The macro only expands in
// "Bridge::Draw", whose "custom_data" is the instance (its frame number joins the helper's profile rows).
struct ID3D11DeviceContext;
void ProfileBridgeStep(ID3D11DeviceContext* context, int step, unsigned long long bridge_frame);
#define SR_BRIDGE_PROFILE(command_list, step) ProfileBridgeStep(command_list, step, custom_data.frame)
