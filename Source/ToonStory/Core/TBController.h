#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/TBTypes.h"
#include "TBController.generated.h"

/** 入力とコンソールコマンドを、試合管理や接続処理へ渡す。 */
UCLASS()
class TOONSTORY_API ATBController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	bool IsLeaveMenuOpen() const { return bLeaveMenuOpen; }
	void ToggleLeaveMenu();
	UFUNCTION(Exec)
	void TBHost();
	UFUNCTION(Exec)
	void TBFind();
	UFUNCTION(Exec)
	void TBJoin(int32 Index);
	UFUNCTION(Exec)
	void TBLeave();
	UFUNCTION(Exec)
	void TBLobby();
	UFUNCTION(Server, Reliable)
	void ServerReturnToLobby();
	UFUNCTION(Exec)
	void TBReady();
	UFUNCTION(Exec)
	void TBStart();
	UFUNCTION(Exec)
	void TBRules(int32 Humans, int32 Toys, float Seconds = 600.f, int32 Items = 5);
	UFUNCTION(Exec)
	void TBPrefer(int32 Team);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerReady();
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerStart();
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRules(FTBSettings Rules);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerPreference(ETBTeam Team);
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FString& Message);
	UPROPERTY(BlueprintReadOnly)
	FString Notice;

private:
	void UpdateLobbyCamera(bool InLobby);
	void UpdateMenuInput(bool ShowMenu);
	bool bLobbyInput = false;
	bool bLeaveMenuOpen = false;
	bool bLobbyCameraActive = false;
	bool bLobbyCameraSearched = false;
	TWeakObjectPtr<AActor> LobbyCamera;
};
