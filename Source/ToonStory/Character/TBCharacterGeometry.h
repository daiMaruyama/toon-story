#pragma once
#include "TBToyPresentation.h"

// Character collision and authored placement validation share these dimensions (cm).
namespace TBCharacterGeometry
{
	inline constexpr float HumanRadius = 26.f;
	inline constexpr float HumanHalfHeight = 65.f;
	inline constexpr float HumanCrouchedHalfHeight = 42.f;
	inline constexpr float ToyRadius = 10.f * TBToyPresentation::SizeScale;
	inline constexpr float ToyHalfHeight = 13.f * TBToyPresentation::SizeScale;
}
