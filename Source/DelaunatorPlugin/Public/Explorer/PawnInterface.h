// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PawnInterface.generated.h"

class AExplorer;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPawnInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DELAUNATORPLUGIN_API IPawnInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION()
	virtual FVector GetExplorerLocation() const = 0;

	/*UFUNCTION()
	virtual AExplorer* GetExplorerActor() const = 0;*/
};
