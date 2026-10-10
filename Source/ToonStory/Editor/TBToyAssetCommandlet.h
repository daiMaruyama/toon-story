#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TBToyAssetCommandlet.generated.h"

/** Native collection maintenance: -run=TBToyAsset [-Only=Teddy|Space] [-Repair] [-Render -AllowCommandletRendering].
 * Inspection is read-only by default. Repair saves collection assets; ExportTextures writes diagnostic PNGs to Saved.
 */
UCLASS()
class UTBToyAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTBToyAssetCommandlet();
	virtual int32 Main(const FString& Params) override;
};
