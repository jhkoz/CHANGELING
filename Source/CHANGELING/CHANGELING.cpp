// Copyright Epic Games, Inc. All Rights Reserved.

#include "CHANGELING.h"
#include "Modules/ModuleManager.h"

// NOTE: do NOT register "/Project" via AddShaderSourceDirectoryMapping here.
// UE auto-maps "/Project" -> <Project>/Shaders during engine init, so adding it
// again hits a duplicate-mapping assert (RenderCore/ShaderCore.cpp). Files in
// Shaders/ already resolve via #include "/Project/...". shadertoolsconfig.json
// still needs its own /Project entry (VS HLSL Tools can't see UE's auto-mapping).

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, CHANGELING, "CHANGELING" );

DEFINE_LOG_CATEGORY(LogCHANGELING)