#pragma once

#include "CoreMinimal.h"
#include "Misc/IQueuedWork.h"

class UGeoDelaunatorComponent;

// One site's collision split. The pool calls DoThreadedWork on a worker. The task deletes itself when it finishes or is abandoned.
class FCollisionSubdiv : public IQueuedWork
{
public:
	FCollisionSubdiv(UGeoDelaunatorComponent* InOwner, int32 InSiteIndex);

	virtual void DoThreadedWork() override;
	virtual void Abandon() override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("CollisionSubdiv"); }

private:
	UGeoDelaunatorComponent* Owner = nullptr;
	int32 SiteIndex = INDEX_NONE;
};
