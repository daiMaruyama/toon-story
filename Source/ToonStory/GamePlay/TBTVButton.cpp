#include "TBTVButton.h"

UTBTVButton::UTBTVButton()
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore); // 一旦全部無視して、踏む側のToyPawnだけOverlapするようにする。
	SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // DefaultEngine.ini参照。
}
