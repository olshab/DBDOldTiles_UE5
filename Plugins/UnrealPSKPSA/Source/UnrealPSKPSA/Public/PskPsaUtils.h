﻿#pragma once
#include "AssetRegistry/AssetRegistryModule.h"

class FPskPsaUtils
{
public:
	template <typename T, typename = UObject>
	static T* LocalFindOrCreate(UClass* StaticClass, UObject* FactoryParent, FString Filename, EObjectFlags Flags)
	{
		const auto Package = CreatePackage(*FPaths::Combine(FPaths::GetPath(FactoryParent->GetPathName() + "/"), Filename));

		auto Asset = LoadObject<T>(Package, *Filename);
		if (!Asset)
		{
			Asset = NewObject<T>(Package, StaticClass, FName(Filename), Flags);
			Asset->PostEditChange();
			FAssetRegistryModule::AssetCreated(Asset);
			Asset->MarkPackageDirty();
		}
		
		return Asset;
	}

	template <typename T>
	static T* LocalCreate(UClass* StaticClass, UObject* FactoryParent, FString Filename, EObjectFlags Flags)
	{
		auto Asset = NewObject<T>(FactoryParent, StaticClass, FName(Filename), Flags);
		return Asset;
	}
};
