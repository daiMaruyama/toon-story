#include "TBEditorAssets.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"

bool TBEditorAssets::BuildGripSocket(USkeletalMesh* ToyMesh)
{
	if (!ToyMesh || !ToyMesh->GetImportedModel() || ToyMesh->GetImportedModel()->LODModels.IsEmpty())
	{
		return false;
	}
	const auto& Skeleton = ToyMesh->GetRefSkeleton();
	FName GripBone = NAME_None;
	for (FName Candidate :
	     {FName(TEXT("spine")), FName(TEXT("Spine02")), FName(TEXT("spine_01")), FName(TEXT("pelvis"))})
	{
		if (Skeleton.FindBoneIndex(Candidate) != INDEX_NONE)
		{
			GripBone = Candidate;
			break;
		}
	}
	const int32 BodyBoneIndex = Skeleton.FindBoneIndex(GripBone);
	if (BodyBoneIndex == INDEX_NONE)
	{
		return false;
	}
	FTransform Reference = FTransform::Identity;
	for (int Bone = BodyBoneIndex; Bone != INDEX_NONE; Bone = Skeleton.GetParentIndex(Bone))
	{
		Reference = Reference * Skeleton.GetRefBonePose()[Bone];
	}
	TArray<FSoftSkinVertex> Vertices;
	ToyMesh->GetImportedModel()->LODModels[0].GetVertices(Vertices);
	if (Vertices.IsEmpty())
	{
		return false;
	}
	// Fit the torso surface using skin weights to exclude arms and accessories.
	TArray<FVector> Torso;
	FBox BodyBounds(ForceInit);
	const int32 Pelvis = Skeleton.FindBoneIndex(GripBone == TEXT("Spine02") ? TEXT("Hips") : TEXT("pelvis"));
	for (const auto& Section : ToyMesh->GetImportedModel()->LODModels[0].Sections)
	{
		for (const auto& Vertex : Section.SoftVertices)
		{
			uint32 Weight = 0;
			for (int32 I = 0; I < MAX_TOTAL_INFLUENCES; ++I)
			{
				const int32 Bone = Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[I])
				                       ? Section.BoneMap[Vertex.InfluenceBones[I]]
				                       : INDEX_NONE;
				if (Bone == BodyBoneIndex || Bone == Pelvis ||
				    (Bone != INDEX_NONE && Skeleton.GetBoneName(Bone).ToString().StartsWith(TEXT("Spine"))))
				{
					Weight += Vertex.InfluenceWeights[I];
				}
			}
			if (Weight > 32767)
			{
				Torso.Add(FVector(Vertex.Position));
				BodyBounds += FVector(Vertex.Position);
			}
		}
	}
	if (Torso.IsEmpty())
	{
		for (const auto& Vertex : Vertices)
		{
			Torso.Add(FVector(Vertex.Position));
			BodyBounds += FVector(Vertex.Position);
		}
	}
	FVector Target = BodyBounds.GetCenter();
	Target.X = BodyBounds.Max.X;
	FVector Contact = Target;
	double Distance = DBL_MAX;
	for (const FVector& Position : Torso)
	{
		const FVector Delta = Position - Target;
		const double Candidate = Delta.X * Delta.X + Delta.Y * Delta.Y + 4 * Delta.Z * Delta.Z;
		if (Candidate < Distance)
		{
			Distance = Candidate;
			Contact = Position;
		}
	}
	USkeletalMeshSocket* Socket = nullptr;
	ToyMesh->Modify();
	for (USkeletalMeshSocket* Existing : ToyMesh->GetMeshOnlySocketList())
	{
		if (Existing->SocketName == TEXT("TB_Grip"))
		{
			Socket = Existing;
		}
	}
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(ToyMesh, NAME_None, RF_Transactional);
		Socket->SocketName = TEXT("TB_Grip");
		ToyMesh->GetMeshOnlySocketList().Add(Socket);
	}
	Socket->SetFlags(RF_Transactional);
	Socket->Modify();
	Socket->BoneName = GripBone;
	Socket->RelativeLocation = Reference.InverseTransformPosition(Contact);
	// Opposing torso contacts for a two-handed carry. These remain editable mesh sockets.
	FBox Waist(ForceInit);
	const bool Slender = ToyMesh->GetName().Contains(TEXT("Recon")) || ToyMesh->GetName().Contains(TEXT("Nutcracker"));
	const double WaistZ = BodyBounds.Min.Z + BodyBounds.GetSize().Z * (Slender ? .32 : .45);
	for (const FVector& Position : Torso)
	{
		if (FMath::Abs(Position.Z - WaistZ) < BodyBounds.GetSize().Z * .10f)
		{
			Waist += Position;
		}
	}
	if (!Waist.IsValid)
	{
		Waist = BodyBounds;
	}
	const FVector Center = Waist.GetCenter();
	for (int Side = -1; Side <= 1; ++Side)
	{
		const FName Name = Side == 0 ? TEXT("TB_CarryCenter") : Side < 0 ? TEXT("TB_CarryLeft") : TEXT("TB_CarryRight");
		USkeletalMeshSocket* Carry = nullptr;
		for (USkeletalMeshSocket* Existing : ToyMesh->GetMeshOnlySocketList())
		{
			if (Existing->SocketName == Name)
			{
				Carry = Existing;
			}
		}
		if (!Carry)
		{
			Carry = NewObject<USkeletalMeshSocket>(ToyMesh, NAME_None, RF_Transactional);
			Carry->SocketName = Name;
			ToyMesh->GetMeshOnlySocketList().Add(Carry);
		}
		Carry->Modify();
		Carry->BoneName = GripBone;
		FVector Point = Center;
		Point.Y += Side * Waist.GetExtent().Y;
		Carry->RelativeLocation = Reference.InverseTransformPosition(Point);
	}
	ToyMesh->MarkPackageDirty();
	// Editor tools leave saving to the user so socket adjustments participate in normal undo/save.
	return true;
}
