// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBLoadingScreenSettings.h"
#include "Engine/Texture2D.h"

UTBLoadingScreenSettings::UTBLoadingScreenSettings()
{
	for (const TCHAR* Name : {TEXT("TeddyBear_Realistic"), TEXT("Dinosaur_Realistic"), TEXT("TinRobot_Realistic"),
	                          TEXT("Duck_Realistic"), TEXT("Nutcracker")})
	{
		Toys.Emplace(FSoftObjectPath(FString::Printf(TEXT("/Game/UI/Loading/T_LoadToy_%s.T_LoadToy_%s"), Name, Name)));
	}
}
