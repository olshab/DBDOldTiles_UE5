#include "BlueprintAssetGenerator.h"
#include "UObject/Package.h"
#include "AssetRegistryModule.h"
#include "AssetData.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManagerGeneric.h"
#include "PSKXFactory.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/TextureFactory.h"
#include "UncookerSettings.h"

static void SplitFModelObjectPath(const FString& InObjectPath, FString& OutPackageName, FString& OutAssetName, FString& OutObjectName)
{
	int32 ObjectDelimIdx = 0;
	if (!InObjectPath.FindChar(TEXT('.'), ObjectDelimIdx))
	{
		UE_LOG(LogTemp, Fatal, TEXT("Provided object path `%s` is not valid (missing object delimiter)"), *InObjectPath);
	}
	OutPackageName = InObjectPath.Mid(0, ObjectDelimIdx);

	int32 LastSlashIdx = 0;
	if (!InObjectPath.FindLastChar(TEXT('/'), LastSlashIdx))
	{
		UE_LOG(LogTemp, Fatal, TEXT("Provided object path `%s` is not valid (missing slash in package name)"), *InObjectPath);
	}
	OutAssetName = InObjectPath.Mid(LastSlashIdx + 1, ObjectDelimIdx - LastSlashIdx - 1);
	OutObjectName = InObjectPath.Mid(ObjectDelimIdx + 1);
}

void UObjectResolver::Initialize(FString&& InPackagePathToSearchAt)
{
	PackagePaths.Emplace(TEXT("/Game/Effects"));
	PackagePaths.Emplace(TEXT("/Game/Materials"));
	PackagePaths.Emplace(std::move(InPackagePathToSearchAt));
	RefreshAssetsData();
}

UObject* UObjectResolver::LoadObject(const FString& InObjectPath)
{
	if (InObjectPath.IsEmpty())
	{
		return nullptr;
	}

	if (FPackageName::IsScriptPackage(InObjectPath))
	{
		UObject* FoundScriptObject = ::LoadObject<UObject>(nullptr, *InObjectPath);
		if (!FoundScriptObject)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load script object `%s`"), *InObjectPath);
		}
		return FoundScriptObject;
	}
	else
	{
		FString PackageName;
		FString AssetName;
		FString ObjectName;
		SplitFModelObjectPath(InObjectPath, PackageName, AssetName, ObjectName);

		if (!PackageName.StartsWith(TEXT("/")))
		{
			PackageName = ConvertFilenameToPackageName(PackageName);
		}
		GetOverridenPackageName(AssetName, PackageName);
		FString ObjectPath = PackageName + TCHAR('.') + ObjectName;

		UObject* FoundAssetObject = ::LoadObject<UObject>(nullptr, *ObjectPath);
		if (!FoundAssetObject)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load asset object `%s`"), *ObjectPath);
		}
		return FoundAssetObject;
	}
}

UObject* UObjectResolver::LoadObjectByAssetName(const FString& InAssetName)
{
	FString PackageName{};
	bool bPackageNameFound = GetOverridenPackageName(InAssetName, PackageName);
	if (bPackageNameFound)
	{
		FString ObjectPath = PackageName + TCHAR('.') + InAssetName;
		UObject* FoundAssetObject = ::LoadObject<UObject>(nullptr, *ObjectPath);
		return FoundAssetObject;
	}
	return nullptr;
}

void UObjectResolver::RefreshAssetsData()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	for (const FString& PackagePathToSearchAt : PackagePaths)
	{
		Filter.PackagePaths.Add(*PackagePathToSearchAt);
	}
	Filter.bRecursivePaths = true;
	AssetRegistry.GetAssets(Filter, AssetsData);
}

FString UObjectResolver::ConvertFilenameToPackageName(const FString& InFilename)
{
	// Get package name from relative file path
	// For example: SomeGame/Content/Meshes/SM_Mesh.uasset -> /Game/Meshes/SM_Mesh
	//				    ^~~~ Game name
	// or for engine content (including plugins content): Engine/Plugins/Animation/ControlRig/Content/Controls/ControlRig_Triangle_1mm.uasset
	//																			       ^~~~~~ Plugin name
	// -> ControlRig/Controls/ControlRig_Triangle_1mm

	int32 FirstSlashIndex;
	InFilename.FindChar(TCHAR('/'), FirstSlashIndex);
	check(FirstSlashIndex != INDEX_NONE);

	FStringView EngineOrGameName = MakeStringView(*InFilename, FirstSlashIndex);
	bool bIsPluginContent = MakeStringView(InFilename).RightChop(FirstSlashIndex).StartsWith(TEXT("/Plugins/"));

	int32 ContentSubstrStart = bIsPluginContent ? InFilename.Find(TEXT("/Content/"), ESearchCase::CaseSensitive) : FirstSlashIndex;
	check(ContentSubstrStart != INDEX_NONE);
	FStringView ContentPath = MakeStringView(InFilename);
	ContentPath.RightChopInline(ContentSubstrStart + 9);  // 9 is length of '/Content/'

	TStringBuilder<256> PackageNameBuilder;
	if (!bIsPluginContent)
	{
		const TCHAR* RootName = EngineOrGameName == TEXT("Engine") ? TEXT("/Engine") : TEXT("/Game");
		PackageNameBuilder << RootName << TCHAR('/') << ContentPath;
	}
	else
	{
		FStringView RootNameView(*InFilename, ContentSubstrStart);
		int32 SlashBeforeRootName;
		RootNameView.FindLastChar(TCHAR('/'), SlashBeforeRootName);
		RootNameView.RightChopInline(SlashBeforeRootName);

		PackageNameBuilder << RootNameView << TCHAR('/') << ContentPath;
	}

	return FPaths::ChangeExtension(PackageNameBuilder.ToString(), TEXT(""));
}

bool FBlueprintAssetGenerator::Generate()
{
	const FString DumpsDirectory = Settings->DumpsDirectory;
	FFileManagerGeneric FileManager;

	ObjectResolver.Initialize(*Settings->GenerateAtPath);

	/** Import all referenced meshes and materials (for OverrideMaterials) before generating blueprints */
	bool bImportMaterials = true;
	if (bImportMaterials)
	{
		const FString ReferencedMaterialsListFilename = DumpsDirectory / TEXT("Lists") / TEXT("ReferencedMaterials.txt");
		if (FileManager.FileExists(*ReferencedMaterialsListFilename))
		{
			ImportMaterialsFromList(ReferencedMaterialsListFilename);
		}
	}

	bool bImportMeshes = true;
	if (bImportMeshes)
	{
		FString MeshesDirectory = DumpsDirectory + TEXT("\\Meshes");
		const FString MeshesPackagePath = Settings->GenerateAtPath + TEXT("/Meshes/_buffer");

		if (FileManager.DirectoryExists(*MeshesDirectory))
		{
			ImportMeshesFromDirectory(MeshesDirectory, MeshesPackagePath);
		}
	}

	ObjectResolver.RefreshAssetsData();

	TArray<FString> BlueprintDumps;
	FileManager.FindFiles(BlueprintDumps, *DumpsDirectory, TEXT("*.json"));
	for (FString& BlueprintDump : BlueprintDumps)
	{
		FString DumpJsonFilepath = DumpsDirectory;
		DumpJsonFilepath.AppendChar(TEXT('\\'));
		DumpJsonFilepath.Append(BlueprintDump);

		UBlueprint* Blueprint = CreateBlueprint(DumpJsonFilepath);
		if (!Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create Blueprint"));
			continue;
		}
		AddComponentsToBlueprint(Blueprint);
		AddOverrideRecords(Blueprint);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	return true;
}

FString FBlueprintAssetGenerator::GetAssetName(const FString& PackageName)
{
	FString AssetName;
	bool SplitSuccess = PackageName.Split(FString(TEXT("/")), nullptr, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	if (!SplitSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("GetAssetName failed!"));
	}
	return AssetName;
}

bool FBlueprintAssetGenerator::NodeSetParent(USimpleConstructionScript* SCS, USCS_Node* Node, const FName ParentNodeName = TEXT(""))
{
	if (!Node)
	{
		return false;
	}
	if (ParentNodeName.ToString().IsEmpty() || ParentNodeName.IsNone())
	{
		SCS->AddNode(Node);
		return true;
	}

	USCS_Node* NodeInCurrentSCS = SCS->FindSCSNode(ParentNodeName);
	if (NodeInCurrentSCS)  /* If parent node not in any parent classes */
	{
		NodeInCurrentSCS->AddChildNode(Node);
		return true;
	}

	UBlueprintGeneratedClass* ParentClass = nullptr;
	USimpleConstructionScript* CurrentSCS = SCS;
	do
	{
		ParentClass = Cast<UBlueprintGeneratedClass>(CurrentSCS->GetParentClass());
		if (!ParentClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Can't find parent node for %s (cast to parent BP class failed)"), *Node->GetVariableName().ToString());

			return false;
		}
		USimpleConstructionScript* ParentSCS = ParentClass->SimpleConstructionScript;
		CurrentSCS = ParentSCS;

		NodeInCurrentSCS = CurrentSCS->FindSCSNode(ParentNodeName);
		if (NodeInCurrentSCS)
		{
			UE_LOG(LogTemp, Display, TEXT("Parent node %s found in class %s"), *ParentNodeName.ToString(), *ParentClass->GetName());
			SCS->AddNode(Node);
			Node->SetParent(NodeInCurrentSCS);
			return true;
		}

	} while (ParentClass);

	return false;
}

UBlueprint* FBlueprintAssetGenerator::CreateBlueprint(const FString& JsonFilePath)
{
	FString JsonContents;
	if (!FFileHelper::LoadFileToString(JsonContents, *JsonFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Can't load string from file %s"), *JsonFilePath);
		return nullptr;
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonContents), JsonObject))
	{
		UE_LOG(LogTemp, Error, TEXT("Can't deserialize Json file: %s"), *JsonFilePath);
		return nullptr;
	}
	_currentAssetInfo = JsonObject;

	/** Get info about parent class */
	FString ParentClassObjectPath = _currentAssetInfo->GetStringField(TEXT("ParentClassObjectPath"));
	UClass* ParentClass = Cast<UClass>(ObjectResolver.LoadObject(ParentClassObjectPath));
	//UClass* ParentClass = LoadClass<UObject>(nullptr, *ParentClassObjectPath);

	if (!ParentClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find Parent Class: %s. Using AActor instead"), *ParentClassObjectPath);
		ParentClass = AActor::StaticClass();
	}
	
	/** Blueprint asset to create */
	FString BlueprintAssetName = _currentAssetInfo->GetStringField(TEXT("BlueprintName"));
	FString BlueprintPackagePath = Settings->GenerateAtPath + TEXT("/Blueprints/_buffer");
	FString BlueprintPackageName = BlueprintPackagePath + TEXT('/') + BlueprintAssetName;

	/** Don't create already existing blueprint */
	UPackage* Package = GetPackage(BlueprintPackageName);
	if (Package)
	{
		UE_LOG(LogTemp, Error, TEXT("Blueprint %s already exists"), *BlueprintPackageName);
		return nullptr;
	}

	Package = CreatePackage(*BlueprintPackageName);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		*BlueprintAssetName,
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create UBlueprint: %s"), *BlueprintAssetName);
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Blueprint);
	// FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	return Blueprint;
}

void FBlueprintAssetGenerator::AddComponentsToBlueprint(UBlueprint* Blueprint)
{
	const TArray<TSharedPtr<FJsonValue>> Components = _currentAssetInfo->GetArrayField(TEXT("Components"));
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

	for (TSharedPtr<FJsonValue> ComponentValue : Components)
	{
		TSharedPtr<FJsonObject> Component = ComponentValue->AsObject();

		FString InternalVariableName = Component->GetStringField(TEXT("InternalVariableName"));
		FString ParentNodeName = Component->GetStringField(TEXT("ParentNodeName"));
		FString ComponentClassPath = Component->GetStringField(TEXT("ComponentClass"));
		TSharedPtr<FJsonObject> Properties = Component->GetObjectField(TEXT("Properties"));

		/** There is DefaultSceneRoot already when we create blueprint instantiated from AActor */
		if (InternalVariableName.Equals("DefaultSceneRoot"))
		{
			PopulateComponentWithProperties(SCS->FindSCSNode(TEXT("DefaultSceneRoot"))->ComponentTemplate, Properties);
			continue;
		}

		UClass* ComponentClass = LoadClass<UObject>(nullptr, *ComponentClassPath);
		if (!ComponentClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get component class for %s. Creating dummy scene instead"), *InternalVariableName);
			USCS_Node* DummyNode = AddDummyComponent(SCS, InternalVariableName, ParentNodeName);
			checkf(DummyNode, TEXT("Failed to create dummy scene node for component \"%s\" (blueprint \"%s\")"),
				*InternalVariableName, *Blueprint->GetName());

			PopulateComponentWithProperties(DummyNode->ComponentTemplate, Properties);
			continue;
		}

		USCS_Node* Node = SCS->CreateNode(ComponentClass, *InternalVariableName);
		if (!Node)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create SCS Node %s"), *InternalVariableName);
			continue;
		}

		if (!NodeSetParent(SCS, Node, *ParentNodeName))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to set parent for Node %s (parent is %s)"), *InternalVariableName, *ParentNodeName);
			continue;
		}

		/** Add properties in ComponentTemplate*/
		PopulateComponentWithProperties(Node->ComponentTemplate, Properties);
	}
}

void FBlueprintAssetGenerator::AddOverrideRecords(UBlueprint* Blueprint)
{
	const TArray<TSharedPtr<FJsonValue>> OverrideRecords = _currentAssetInfo->GetArrayField(TEXT("ComponentOverrideRecords"));
	UInheritableComponentHandler* ComponentHandler = Blueprint->GetInheritableComponentHandler(true);
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

	for (TSharedPtr<FJsonValue> RecordValue : OverrideRecords)
	{
		TSharedPtr<FJsonObject> Record = RecordValue->AsObject();

		FString SCSVariableName = Record->GetStringField(TEXT("SCSVariableName"));
		TSharedPtr<FJsonObject> Properties = Record->GetObjectField(TEXT("Properties"));
		USCS_Node* InheritedNode = FindInheritedNode(SCS, SCSVariableName);
		if (!InheritedNode)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to find inherited SCS Node %s"), *SCSVariableName);
			continue;
		}

		FComponentKey ComponentKey(InheritedNode);
		UActorComponent* ComponentTemplate = ComponentHandler->GetOverridenComponentTemplate(ComponentKey);
		if (!ComponentTemplate)
			ComponentTemplate = ComponentHandler->CreateOverridenComponentTemplate(ComponentKey);

		// Add properties in ComponentTemplate
		PopulateComponentWithProperties(ComponentTemplate, Properties);
	}
}

USCS_Node* FBlueprintAssetGenerator::FindInheritedNode(USimpleConstructionScript* SCS, const FString& SCSVariableName)
{
	UBlueprintGeneratedClass* ParentClass = nullptr;
	USimpleConstructionScript* CurrentSCS = SCS;

	do
	{
		ParentClass = Cast<UBlueprintGeneratedClass>(CurrentSCS->GetParentClass());
		if (!ParentClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Can't find inherited node %s (cast to parent BP class failed)"), *SCSVariableName);
			return nullptr;
		}
		USimpleConstructionScript* ParentSCS = ParentClass->SimpleConstructionScript;
		CurrentSCS = ParentSCS;

		USCS_Node* InheritedNode = CurrentSCS->FindSCSNode(*SCSVariableName);
		if (InheritedNode)
			return InheritedNode;

	} while (ParentClass);

	return nullptr;
}

USCS_Node* FBlueprintAssetGenerator::AddDummyComponent(USimpleConstructionScript* SCS, const FString& InternalVariableName, const FString& ParentNodeName)
{
	USCS_Node* DummyNode = SCS->CreateNode(USceneComponent::StaticClass(), *InternalVariableName);

	if (!NodeSetParent(SCS, DummyNode, *ParentNodeName))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to set parent for Node %s (parent is %s)"), *InternalVariableName, *ParentNodeName);
		return nullptr;
	}

	return DummyNode;
}

bool FBlueprintAssetGenerator::PopulateComponentWithProperties(UActorComponent* ComponentTemplate, TSharedPtr<FJsonObject> Properties)
{
	TArray<FString> PropertyNames;
	Properties->Values.GetKeys(PropertyNames);

	for (const FString& JsonProperty : PropertyNames)
	{
		FProperty* Property = ComponentTemplate->GetClass()->FindPropertyByName(*JsonProperty);
		if (!Property)
		{
			UE_LOG(LogTemp, Error, TEXT("%s doesn't have property with the name %s"), *ComponentTemplate->GetName(), *JsonProperty);
			continue;
		}

		SetPropertyValue(
			Property,
			Property->ContainerPtrToValuePtr<void>(ComponentTemplate),
			Properties->GetField<EJson::None>(JsonProperty)
		);
	}

	return true;
}

void FBlueprintAssetGenerator::SetPropertyValue(FProperty* InProperty, void* PropertyValuePtr, TSharedPtr<FJsonValue> Value)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		FScriptArrayHelper Array(ArrayProperty, PropertyValuePtr);
		Array.EmptyValues();

		const TArray<TSharedPtr<FJsonValue>>& JsonArrayValues = Value->AsArray();

		for (const TSharedPtr<FJsonValue>& JsonArrayValue : JsonArrayValues)
		{
			int32 NewIndex = Array.AddValue();
			uint8* ArrayValuePtr = Array.GetRawPtr(NewIndex);
			SetPropertyValue(ArrayProperty->Inner, ArrayValuePtr, JsonArrayValue);
		}

		return;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		UScriptStruct* Struct = StructProperty->Struct;

		TArray<FString> JsonProperties;
		Value->AsObject()->Values.GetKeys(JsonProperties);

		for (FString& JsonProperty : JsonProperties)
		{
			FProperty* Property = Struct->FindPropertyByName(*JsonProperty);
			if (Property == nullptr)
			{
				UE_LOG(LogTemp, Error, TEXT("Can't find property %s from JSON in ScriptStruct"), *JsonProperty);
				continue;
			}

			SetPropertyValue(
				Property,
				Property->ContainerPtrToValuePtr<void>(PropertyValuePtr),
				Value->AsObject()->GetField<EJson::None>(JsonProperty)
			);
		}

		return;
	}

	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
	{
		BoolProperty->SetPropertyValue(PropertyValuePtr, Value->AsBool());
		return;
	}

	if (FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		/*
		 * FByteProperty can possibly be 255-value enumeration variable,
		 * so we should perform this check before FNumericProperty
		*/

		UEnum* IntPropertyEnum = ByteProperty->GetIntPropertyEnum();
		if (IntPropertyEnum)
		{
			FString EnumValue = Value->AsString();
			ByteProperty->SetIntPropertyValue(PropertyValuePtr, IntPropertyEnum->GetValueByNameString(EnumValue));
		}
		else
		{
			int64 ByteValue = (int64)Value->AsNumber();
			ByteProperty->SetIntPropertyValue(PropertyValuePtr, ByteValue);
		}

		return;
	}

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		UEnum* Enum = EnumProperty->GetEnum();

		FString EnumValue = Value->AsString();
		UnderlyingProperty->SetIntPropertyValue(PropertyValuePtr, Enum->GetValueByNameString(EnumValue));

		return;
	}

	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
	{
		if (NumericProperty->IsFloatingPoint())
		{
			NumericProperty->SetFloatingPointPropertyValue(PropertyValuePtr, Value->AsNumber());
		}
		else
		{
			NumericProperty->SetIntPropertyValue(PropertyValuePtr, (int64)Value->AsNumber());
		}
		return;
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(InProperty))
	{
		FName* NamePtr = static_cast<FName*>(PropertyValuePtr);
		*NamePtr = *Value->AsString();
		return;
	}

	if (FStrProperty* StrProperty = CastField<FStrProperty>(InProperty))
	{
		FString* StrPtr = static_cast<FString*>(PropertyValuePtr);
		*StrPtr = *Value->AsString();
		return;
	}

	if (FTextProperty* TextProperty = CastField<FTextProperty>(InProperty))
	{
		FText* TextPtr = static_cast<FText*>(PropertyValuePtr);
		*TextPtr = FText::FromString(Value->AsString());
		return;
	}

	if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
	{
		FString ObjectPath = Value->AsString();
		UObject* Object = ObjectResolver.LoadObject(ObjectPath);
		if (Object == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load object for property %s"), *InProperty->GetName());
		}
		ObjectProperty->SetObjectPropertyValue(PropertyValuePtr, Object);
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("Failed to resolve FProperty type for property %s"), *InProperty->GetName())
}

UObject* FBlueprintAssetGenerator::DeserializeObjectProperty(int32 PackageIndex, TSharedPtr<FJsonObject> JsonImportTable)
{
	if (PackageIndex >= 0)
		return nullptr;

	const TArray<TSharedPtr<FJsonValue>> ImportTable = _currentAssetInfo->GetArrayField(TEXT("ImportTable"));
	TSharedPtr<FJsonObject> Import = ImportTable[-PackageIndex - 1]->AsObject();

	FString ClassPackageName = Import->GetStringField(TEXT("ClassPackage"));
	FString ClassName = Import->GetStringField(TEXT("ClassName"));
	int32 OuterIndex = Import->GetIntegerField(TEXT("OuterIndex"));
	FString ObjectName = Import->GetStringField(TEXT("ObjectName"));

	/** Imports that have OuterIndex = 0 are just path to the asset */
	if (OuterIndex == 0)
	{
		UPackage* Package = GetPackage(*ObjectName);
		if (!Package)
			UE_LOG(LogTemp, Error, TEXT("Can't deserialize package: %s"), *ObjectName);

		return Package;
	}

	/** Otherwise, go through the entire UObject chain until we find an import with OuterIndex = 0*/
	UPackage* ClassPackage = GetPackage(ClassPackageName);
	if (!ClassPackage)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't deserialize ClassPackage: %s"), *ClassPackageName);
		return nullptr;
	}

	UClass* Class = FindObjectFast<UClass>(ClassPackage, *ClassName);
	if (!Class)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't deserialize ClassName: %s"), *ClassName);
		return nullptr;
	}

	UObject* OuterObject = DeserializeObjectProperty(OuterIndex, JsonImportTable);
	if (!OuterObject)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't deserialize Outer Object for object: %s"), *ObjectName);
		return nullptr;
	}

	UObject* ResultObject = StaticFindObjectFast(Class, OuterObject, *ObjectName);
	if (!ResultObject)
		UE_LOG(LogTemp, Error, TEXT("Can't deserialize object: %s"), *ObjectName);

	return ResultObject;
}

UPackage* FBlueprintAssetGenerator::GetPackage(const FString& PackageName)
{
	UPackage* Package = FindPackage(nullptr, *PackageName);
	if (!Package)
	{
		Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	}

	return Package;
}

void FBlueprintAssetGenerator::ResolveProperty(FProperty* InProperty, void* PropertyValuePtr)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("ArrayProperty: %s"), *InProperty->GetName());

		FScriptArrayHelper Array(ArrayProperty, PropertyValuePtr);
		FProperty* ElementProperty = ArrayProperty->Inner;

		UE_LOG(LogTemp, Warning, TEXT("Start of array"));
		for (int32 i = 0; i < Array.Num(); i++)
			ResolveProperty(ElementProperty, Array.GetRawPtr(i));

		UE_LOG(LogTemp, Warning, TEXT("End of array"));

		return;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("StructProperty: %s"), *InProperty->GetName());
		UScriptStruct* Struct = StructProperty->Struct;

		UE_LOG(LogTemp, Warning, TEXT("Start of struct"));
		for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext)
			ResolveProperty(Property, Property->ContainerPtrToValuePtr<void>(PropertyValuePtr));

		UE_LOG(LogTemp, Warning, TEXT("End of struct"));

		return;
	}

	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("BoolProperty: %s"), *InProperty->GetName());
		return;
	}

	if (FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("ByteProperty: %s"), *InProperty->GetName());
		return;
	}

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnumProperty: %s"), *InProperty->GetName());
		return;
	}

	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
	{
		if (NumericProperty->IsFloatingPoint())
		{
			UE_LOG(LogTemp, Warning, TEXT("FloatingPointProperty: %s"), *InProperty->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("IntegerProperty: %s"), *InProperty->GetName());
		}

		return;
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("NameProperty: %s"), *InProperty->GetName());
		return;
	}

	if (FStrProperty* StrProperty = CastField<FStrProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("StrProperty: %s"), *InProperty->GetName());
		return;
	}

	if (FTextProperty* TextProperty = CastField<FTextProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("TextProperty: %s"), *InProperty->GetName());
		return;
	}

	if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("ObjectProperty: %s"), *InProperty->GetName());
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("Failed to resolve FProperty type for property %s"), *InProperty->GetName())
}

void FBlueprintAssetGenerator::ImportMeshesFromDirectory(const FString& PskDirectory, const FString& AssetPath)
{
	FFileManagerGeneric FileManager;
	TArray<FString> PskFiles;
	FileManager.FindFiles(PskFiles, *PskDirectory, TEXT("*.pskx"));

	for (FString& PskFilename : PskFiles)
	{
		FString PskFullPath = PskDirectory;
		PskFullPath.AppendChar(TEXT('\\'));
		PskFullPath.Append(PskFilename);

		FString AssetName;
		PskFilename.Split(TEXT("."), &AssetName, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		checkf(!AssetName.IsEmpty(), TEXT("Failed to get Asset Name from .pskx file: %s"), *PskFilename);
		UStaticMesh* ImportedMesh = LoadOrImportMesh(AssetName, PskFullPath, AssetPath);

		// Create Material Instances for that mesh
		const FString MaterialsPackagePath = Settings->GenerateAtPath + TEXT("/Materials/_buffer");
		AddMaterials(
			ImportedMesh,
			MaterialsPackagePath,
			Settings->ParentForCreatedMaterials.ToString()
		);
	}
}

UStaticMesh* FBlueprintAssetGenerator::LoadOrImportMesh(const FString& MeshName, const FString& PskFullPath, const FString& GamePath)
{
	// Load the mesh without importing if it exists
	UStaticMesh* ExistingMesh = Cast<UStaticMesh>(ObjectResolver.LoadObjectByAssetName(MeshName));
	if (ExistingMesh)
	{
		return ExistingMesh;
	}

	FString PackageName = GamePath + TEXT('/') + MeshName;
	UPackage* MeshPackage = GetPackage(*PackageName);
	if (MeshPackage)  // if that mesh already have been imported into buffer
	{
		return Cast<UStaticMesh>(StaticFindObjectFast(UTexture::StaticClass(), MeshPackage, *MeshName));
	}

	UPackage* Package = CreatePackage(*PackageName);
	UObject* ImportedMesh = UPskxFactory::Import(PskFullPath, Package, *MeshName, RF_Public | RF_Standalone, TMap<FString, FString>());
	return CastChecked<UStaticMesh>(ImportedMesh);
}

void FBlueprintAssetGenerator::ImportMaterialsFromList(const FString& MaterialsListFilename)
{
	TArray<FString> ReferencedMaterials;
	if (!FFileHelper::LoadFileToStringArray(ReferencedMaterials, *MaterialsListFilename))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read referenced materials from `%s`"), *MaterialsListFilename);
		return;
	}

	const FString MaterialsPackagePath = Settings->GenerateAtPath + TEXT("/Materials/_buffer");
	const FString ParentMaterialObjectPath = Settings->ParentForCreatedMaterials.ToString();

	for (const FString& ReferencedMaterial : ReferencedMaterials)
	{
		FString AssetName;
		bool SplitSuccess = ReferencedMaterial.Split(FString(TEXT(".")), nullptr, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (!SplitSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get asset name for referenced material `%s`"), *ReferencedMaterial);
		}

		UMaterialInterface* Material = nullptr;

		FString MaterialInstancePackageName = MaterialsPackagePath + TCHAR('/') + AssetName;
		UPackage* MatPackage = GetPackage(*MaterialInstancePackageName);
		if (!MatPackage)
		{
			Material = CreateMaterialInstance(MaterialInstancePackageName, ParentMaterialObjectPath);
		}
	}
}

void FBlueprintAssetGenerator::AddMaterials(UStaticMesh* StaticMesh, const FString& MI_AssetPath, const FString& ParentMaterialObjectPath)
{
	TArray<FStaticMaterial> StaticMaterials = StaticMesh->GetStaticMaterials();
	for (int i = 0; i < StaticMaterials.Num(); i++)
	{
		const FString MatInstAssetName = StaticMaterials[i].MaterialSlotName.ToString();
		UMaterialInterface* MatInst = Cast<UMaterialInterface>(ObjectResolver.LoadObjectByAssetName(MatInstAssetName));
		if (MatInst)
		{
			StaticMesh->SetMaterial(i, MatInst);
		}
		else
		{
			UMaterialInterface* Material = nullptr;

			FString MaterialInstancePackageName = MI_AssetPath + TCHAR('/') + MatInstAssetName;
			UPackage* MatPackage = GetPackage(*MaterialInstancePackageName);
			if (MatPackage)
			{
				Material = FindObjectFast<UMaterialInterface>(MatPackage, *MatInstAssetName);
			}
			else
			{
				Material = CreateMaterialInstance(MaterialInstancePackageName, ParentMaterialObjectPath);
			}

			StaticMesh->SetMaterial(i, Material);
		}
	}
}

UMaterialInterface* FBlueprintAssetGenerator::CreateMaterialInstance(const FString& MaterialInstancePackageName, const FString& ParentMaterialObjectPath)
{
	const FString AssetPathForImportedTextures = Settings->GenerateAtPath + TEXT("/Textures/_buffer");

	FString MaterialName = GetAssetName(MaterialInstancePackageName);
	UTexture* DiffuseTexture = GetTextureFromDump(MaterialName, AssetPathForImportedTextures);

	// Create MaterialInstanceConstant derived from ParentMaterial
	UMaterialInterface* ParentMaterial = ::LoadObject<UMaterialInterface>(nullptr, *ParentMaterialObjectPath);
	UPackage* Package = CreatePackage(*MaterialInstancePackageName);
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>(GetTransientPackage(), FName(NAME_None));
	Factory->InitialParent = ParentMaterial;

	UMaterialInstanceConstant* MatInstance = (UMaterialInstanceConstant*)Factory->FactoryCreateNew(
		UMaterialInstanceConstant::StaticClass(),
		Package,
		*MaterialName,
		RF_Standalone | RF_Public,
		nullptr,
		nullptr
	);

	if (MatInstance)
	{
		// Diffuse Texture
		if (DiffuseTexture != NULL)
		{
			MatInstance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Main_BaseColor")), DiffuseTexture);
		}

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(MatInstance);
		MatInstance->MarkPackageDirty();

		return MatInstance;
	}

	return nullptr;
}

UTexture* FBlueprintAssetGenerator::GetTextureFromDump(const FString& MaterialName, const FString& DefaultTexturesPath)
{
	FString MaterialDumpsDirectory = Settings->DumpsDirectory + TEXT("\\Materials");
	FFileManagerGeneric FileManager;

	if (!FileManager.DirectoryExists(*MaterialDumpsDirectory))
	{
		return nullptr;
	}
	FString MaterialDumpFile = MaterialDumpsDirectory + TEXT("\\") + MaterialName + TEXT(".txt");
	//checkf(FileManager.FileExists(*MaterialDumpFile), TEXT("Failed to find dump file for material %s"), *MaterialName);
	if (!FileManager.FileExists(*MaterialDumpFile))
	{
		// If dump file doesn't exist, then it is just Material, not MaterialInstanceConstant
		// Then we are gonna create material instance with default white basecolor texture
		return NULL;
	}

	FString DiffuseTexturesFile;
	if (!FFileHelper::LoadFileToString(DiffuseTexturesFile, *MaterialDumpFile))
	{
		UE_LOG(LogTemp, Error, TEXT("Can't load string from file %s"), *MaterialDumpFile);
		return nullptr;
	}

	TArray<FString> DiffuseTextures;
	DiffuseTexturesFile.ParseIntoArrayLines(DiffuseTextures, true);

	if (DiffuseTextures.Num() == 0)
	{
		return nullptr;
	}

	// Assuming first texture is Main_BaseColor
	const FString TextureName = DiffuseTextures[0];
	UTexture* DiffuseTexture = LoadOrImportTexture(
		TextureName,
		*DefaultTexturesPath,
		Settings->DumpsDirectory + TEXT("\\Textures")
	);

	return DiffuseTexture;
}

UTexture* FBlueprintAssetGenerator::LoadOrImportTexture(const FString& TextureName, const FString& AssetPath, const FString& ExportedTexturesDirectory)
{
	if (TextureName.StartsWith(TEXT("/Game"), ESearchCase::CaseSensitive))
	{
		UPackage* ExistingPackage = GetPackage(TextureName);
		FString AssetName = GetAssetName(TextureName);
		UTexture* ExistingTexture = Cast<UTexture>(StaticFindObjectFast(UTexture::StaticClass(), ExistingPackage, *AssetName));

		return ExistingTexture;
	}

	// Check if we have already imported that texture
	UTexture* ExistingTexture = Cast<UTexture>(ObjectResolver.LoadObjectByAssetName(TextureName));
	if (ExistingTexture)
	{
		return ExistingTexture;
	}

	/** Import texture from dumps directory */
	const FString TextureFilePath = ExportedTexturesDirectory + TEXT("\\") + TextureName + TEXT(".png");

	TArray<uint8> DataBinary;
	FFileHelper::LoadFileToArray(DataBinary, *TextureFilePath);

	FString PackageName = AssetPath + TEXT("/") + TextureName;
	UPackage* TexturePackage = GetPackage(*PackageName);
	if (TexturePackage)  // if that texture already have been imported into buffer
	{
		return Cast<UTexture>(StaticFindObjectFast(UTexture::StaticClass(), TexturePackage, *TextureName));
	}

	TexturePackage = CreatePackage(*PackageName);
	UTexture* ImportedTexture = NULL;

	if (DataBinary.Num() > 0)
	{
		const uint8* PtrTexture = DataBinary.GetData();
		UTextureFactory* TextureFact = NewObject<UTextureFactory>();
		TextureFact->AddToRoot();

		ImportedTexture = (UTexture*)TextureFact->FactoryCreateBinary(
			UTexture2D::StaticClass(),
			TexturePackage,
			*TextureName,
			RF_Standalone | RF_Public,
			NULL,
			TEXT("png"),
			PtrTexture,
			PtrTexture + DataBinary.Num(),
			NULL);

		if (ImportedTexture != NULL)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(ImportedTexture);

			// Set the dirty flag so this package will get saved later
			TexturePackage->SetDirtyFlag(true);
		}
		TextureFact->RemoveFromRoot();

		return ImportedTexture;
	}

	return nullptr;
}
