#pragma once

// MinHook keeps one state per DLL, shared by Core (Reflex's sleep hook) and the game mod (its own engine hooks), compiled once
// through "Luma.Features.props" ("ENABLE_MINHOOK"). So each owner initializes it with "InitializeMinHook()" and only removes its
// own hooks ("MH_RemoveHook()"/"MH_DisableHook(target)"): "MH_ALL_HOOKS" and "MH_Uninitialize()" would also remove the other
// owner's: this header removes the first and deprecates the second (C4995). Core uninitializes it when the addon unloads.
#include "../../External/reshade/deps/minhook/include/MinHook.h"

inline bool InitializeMinHook()
{
   const MH_STATUS status = MH_Initialize();
   return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
}

inline void UninitializeMinHook()
{
   MH_Uninitialize();
}

#undef MH_ALL_HOOKS
#pragma deprecated(MH_Uninitialize)
