// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBTVButton.h"

UTBTVButton::UTBTVButton()
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore); // 一旦全部無視して、踏む側のToyPawnだけOverlapするようにする。
	SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // DefaultEngine.ini参照。
}

FName UTBTVButton::GetGlowParameterName() const
{
	// M_RemoteGlow のパラメータ名と一致させる。
	switch (CurrentKey)
	{
		case ETBRemoteKey::Digit:
			return FName(*FString::Printf(TEXT("Btn%d"), Digit));
		case ETBRemoteKey::Next:
			return TEXT("BtnNext");
		case ETBRemoteKey::Prev:
			return TEXT("BtnPrev");
		case ETBRemoteKey::Power:
			return TEXT("BtnPower");
	}
	return NAME_None;
}