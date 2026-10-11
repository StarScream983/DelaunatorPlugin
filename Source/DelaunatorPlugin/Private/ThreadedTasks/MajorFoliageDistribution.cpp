#include "ThreadedTasks/MajorFoliageDistribution.h"
#include "GeoDelaunatorComponent.h"

FMajorFoliageDistribution::FMajorFoliageDistribution(UGeoDelaunatorComponent* InOwner, int32 InSiteIndex)
	: Owner(InOwner)
	, SiteIndex(InSiteIndex)
{
}

void FMajorFoliageDistribution::DoThreadedWork()
{
	// Foliage for SiteIndex runs here, after that site's collision split has finished. Not called from the tick yet.
	delete this;
}

void FMajorFoliageDistribution::Abandon()
{
	delete this;
}
