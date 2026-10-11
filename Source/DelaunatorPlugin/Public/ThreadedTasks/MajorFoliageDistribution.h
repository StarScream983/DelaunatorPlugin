#pragma once

#include "CoreMinimal.h"
#include "Misc/IQueuedWork.h"

class UGeoDelaunatorComponent;

// Major foliage for one site, read from a finished collision split. The task deletes itself when it finishes or is abandoned.
class FMajorFoliageDistribution : public IQueuedWork
{
public:
	FMajorFoliageDistribution(UGeoDelaunatorComponent* InOwner, int32 InSiteIndex);

	virtual void DoThreadedWork() override;
	virtual void Abandon() override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("MajorFoliageDistribution"); }

private:
	UGeoDelaunatorComponent* Owner = nullptr;
	int32 SiteIndex = INDEX_NONE;
};
