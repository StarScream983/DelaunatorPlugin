#include "ThreadedTasks/CollisionSubdiv.h"
#include "GeoDelaunatorComponent.h"

FCollisionSubdiv::FCollisionSubdiv(UGeoDelaunatorComponent* InOwner, int32 InSiteIndex)
	: Owner(InOwner)
	, SiteIndex(InSiteIndex)
{
}

void FCollisionSubdiv::DoThreadedWork()
{
	// Collision split of SiteIndex runs here. Not called from the tick yet.
	delete this;
}

void FCollisionSubdiv::Abandon()
{
	delete this;
}
