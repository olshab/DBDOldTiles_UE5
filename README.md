# Old Tiles

This project aims to restore the **original visual look** for maps in *Dead By Daylight*.

It works by hiding vanilla tiles and spawning custom ones instead. These custom tiles contain only static visual elements (i.e. grass, crates, rocks, main buildings etc). This ensures that all collision remain unchanged and no gameplay features are broken.

This Unreal Engine project is dedicated only to tiles and blueprint replacement, it doesn't contain *Old Lighting*, *Old Interactable Objects* and other miscellaneous additions of **Old Tiles** mod.

## Prerequisites

The following dependencies are necessary for **Old Tiles** development:

### Environment

- **Unreal Engine 5.2** with *UnversionedFix* applied
- **UnrealPak** with UTOC/UCAS support
- **Visual Studio 2022** (17.8+) with installed workload `.NET Desktop Development`

### Programs

- [**BlueprintReferenceViewer**](https://github.com/olshab/BlueprintReferenceViewer_UE5)
- [**BlueprintDumper**](https://github.com/olshab/BlueprintDumper_UE5)

## Blueprint generation specifics

This section explains some points that are particularly important for understanding the **Old Tiles** development process.

### Blueprints to be replaced

First of all, let's determine what blueprint should be replaced in game (*Replace* here means not direct replacement of asset in game files, but hiding that blueprint and spawning our custom one at the same location). There are two types of blueprints:

1. **Tile**: this is a top level blueprint, basically serving as a container for all visual components that can be spawned on that tile. Tile blueprints contain two types of components:
    - Static components: they are always present on the tile. Our custom tile should contain ONLY static components.
    - *ActorSpawner* components - components of special `ActorSpawner` type: they are being conditionally spawned on the tile (for example, Basement on the Shack tile).
2. **ActorSpawner** blueprint: any blueprints that are used in `ActorSpawner` components. Since we can't replace ActorSpawner directly on the tile, we need to replace it separately, just like tiles.

### Child Actors

One of the components that tiles can contain is `ChildActorComponent`, which is a way to have a blueprint inside of another blueprint (for example, *Shack Building* blueprint inside of *Shack* tile). So these child actor blueprints must be generated too, before the creation of tile blueprint. To choose a blueprint for child component, set it in `ChildActorClass` property (in case it hasn't been set automatically).

### Actor Spawners

Tiles also contain Actor Spawners - special components, that can spawn blueprints *only if certain conditions are met*.
Actor Spawers have three main properties:
* **ActivatedSceneElement** - determines which blueprint will be spawned if that ActorSpawner is marked as Activated
* **DeactivatedSceneElement** - determines which blueprint will be spawned if that ActorSpawner is marked as Deactivated
* **Visualization** - used only in Editor to visualize that ActorSpawner in viewport (*not used in game*)

Most of the Actor Spawners have only **ActivatedSceneElement**, whereas **DeactivatedSceneElement** is empty. This means if such ActorSpawner is deactivated, nothing will be spawned (for example, *Basement* Actor Spawner in *Shack* tile). But some ActorSpawners has **DeactivatedSceneElement** as well, like *shack floor* in *Shack* tile - if it's activated (which means basement is spawned in that tile as well), then floor variation with an access to the basement will be spawned, otherwise there's gonna be spawned just plain floor.

So, Actor Spawner is not a static object that always exists in tile, and we shouldn't create it in our tile. But my tool generates tiles with them just for the purpose of visualization. All ActorSpawners in the project are created as child actors and have "*ActorSpawner*" tag. All ActorSpawners will be stripped out during the process of injection tile blueprints.

## Development

This section will guide you through the process of development **Old Tiles** mod, starting from generation all the necessary tiles/blueprints in editor to configuring which blueprints should be injected (i.e. replaced in game).

### 1. Finding Blueprints Referenced in Tiles

Tile Blueprints can reference another blueprints, for example this Asylum tile (`BP_TL_Fr_16x16_HD31`) references `BP_WL_Br_4mStraight01_Asy`.
It means that blueprint is used as Child Actor in tile:

![BlueprintReferenceViewerExample](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintReferenceViewerExample.png?raw=true)

So if you uncook tile blueprint `BP_TL_Fr_16x16_HD31` without uncooking `BP_WL_Br_4mStraight01_Asy` first, you will end up with empty Child Actor when you generate tile blueprint, because there is no such `BP_WL_Br_4mStraight01_Asy` in the project yet. To avoid that, you need to uncook `BP_WL_Br_4mStraight01_Asy` first and then the tile blueprint `BP_TL_Fr_16x16_HD31` itself. 

To check what blueprints are referenced in tiles, you need to use [**BlueprintReferenceViewer**](https://github.com/olshab/BlueprintReferenceViewer_UE5) tool:

![BlueprintReferenceViewerSettings](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintReferenceViewerSettings.png?raw=true)

First of all, in FModel find tile blueprints for particular map (for example, all assets at `DeadByDaylight/Content/Blueprints/Tiles/04-Asylum`) that you want to uncook:

![TileListFModel](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/TileListFModel.png?raw=true)

Select all tiles and copy their paths (Right Mouse Button -> Copy -> Package Path) and paste it to some .TXT file, on **BlueprintReferenceViewer** settings screenshot above that .TXT file is `C:\Users\Oleg\Desktop\ToDump.txt`.

Set all other settings, for example if you are uncooking live tiles, set `Paks Directory` to live Paks folder and `Engine Version` to `GAME_DeadByDaylight` (it would be `GAME_UE4_21` if you are uncooking 3.0.0 tiles).

Create some folder where the tool will save its result which can be copied as an input for the next tool, **BlueprintDumper**, on settings screenshot above that folder is `C:\Users\Oleg\Desktop\BlueprintReferenceViewer`.

You can also provide a path to OldTiles UE project using `ProjectDirectory` variable with `bScanProjectForReferencedAssets` set to `true`. If you'll do that, **BlueprintReferenceViewer** will be aware of already existing blueprint in your project. As you can see, in the output window all blueprints that already exist in the project marked with grey:

![BlueprintReferenceViewerGreyList](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintReferenceViewerGreyList.png?raw=true)

Since both live and old tiles usually have blueprints with the same name (`BP_Static_Rocks` exists in 3.0.0 and in live DBD), you need to specify which blueprints you need to account when BlueprintReferenceViewer will look for existing blueprints. This can be done by setting variable `AssetsPackagePathToScanAt` in settings:

- `/Game/OriginalTiles` - if you are uncooking old 3.0.0 tiles/blueprints
- `/Game/NewTiles` - if you are uncooking live tiles/blueprints

After you run the program with F5, the following text files will appear in `DumpFolder`:

![AssetsFolder](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintReferenceViewerFolderResult.png?raw=true)

Now you need to uncook blueprints contained in these .TXT files starting from the highest level (`Level 2` in my case) up to the `Level 0` (`Level 0` will contain all blueprint paths that were initially in `DumpList` .TXT file).

### 2. Uncooking Tiles/Blueprints

This is done in two steps: 
1. Dumping `.uasset` blueprints as `.json` with [**BlueprintDumper**](https://github.com/olshab/BlueprintDumper_UE5) 
2. Generating blueprint asset from `.json` dump in editor with **BlueprintUncooker** UE plugin.

**BlueprintDumper** settings look like this:

![BlueprintDumperSettings](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintDumperSettings.png?raw=true)

This tool reads all blueprint paths in `DumpList` .TXT file and write dumped `.json` in `DumpFolder` folder. Usually you'll need to set `DumpList` to one of the `Level XX` text files from the previous step. `DumpFolder` should not be the same as for **BlueprintReferenceViewer** tool.

You also need to set `CustomTag` which will be used during *merging* step:

- `OriginalTiles` - if you are uncooking old 3.0.0 tiles/blueprints
- `NewTiles` - if you are uncooking live tiles/blueprints

The tool will automatically export all textures, materials and meshes referenced by blueprints you are dumping. To prevent it from unnecessary exporting assets that already exists in OldTiles UE project, specify path to UE project in `ProjectDirectory`, set `bScanProjectForReferencedAssets` to `true` and set `AssetsPackagePathToScanAt` to one of the following values:

- `/Game/OriginalTiles` - if you are uncooking old 3.0.0 tiles/blueprints
- `/Game/NewTiles` - if you are uncooking live tiles/blueprints

Run the tool with F5 and dumped json files will appear in `DumpFolder`. After you've dumped blueprints, you need to generate them in editor. In Old Tiles Unreal Engine project Open **BlueprintUncooker** window:

![BlueprintUncookerButton](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintUncookerButton.png?raw=true)

![BlueprintUncookerUI](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintUncookerUI.png?raw=true)

`Path To Dumped JSON Files` should be the same as `DumpFolder` in **BlueprintDumper**, as well as `Generate at Path`. Set `Parent for Created Materials` to the base material that will used for creating material instances (set it to `/Game/Materials/OldTiles/MI_Common`).

After you click `Generate` button, assets will appear at the following locations:
- Blueprints at `Content/[New/Original]Tiles/Blueprints/_buffer`
- Meshes at `Content/[New/Original]Tiles/Meshes/_buffer`
- Materials at `Content/[New/Original]Tiles/Materials/_buffer`
- Textures at ``Content/[New/Original]Tiles/Textures/_buffer``

### 3. Merging Tiles/ActorSpawners in Editor

**BlueprintMergeTool** button is accessible from the same toolbar as **BlueprintUncooker**:

![BlueprintMergeToolUI](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/BlueprintMergeToolUI.png?raw=true)

Merging process works as follows: a copy of the blueprint specified as the `Destination` *(from NewTiles)* is created and all components from the blueprint specified as the `Source` *(from OriginalTiles)* are copied to it.

With that:
- Materials of all StaticMesh components in Destination blueprint are replaced with `Translucent Material` (semi-transparent blue by default) if `Make Meshes Translucent` set to True. Note that it will not affect meshes that are inside of ChildActor components
- All components from Destination blueprint are set to be `HiddenInGame` (but they are still visible in Editor) and `NewTiles` component tag is added to them. **Warning:** every time you want to use something from Destination blueprint (for example, Rocks meshes), don't forget to set `HiddenInGame` to false and clear `NewTiles` tag
- All `ActorSpawners` (which are actually just ChildActors) from OriginalTiles are hidden in both Editor and Game, since ActorSpawners from 3.0.0 tiles are not used in live tiles in any ways
- All `ActorSpawners` from Destination blueprint (from NewTiles) are replaced with blueprints from `Folder To Take ActorSpawners From` if `Replace Actor Spawners` is set to True. ActorSpawners in merged blueprint exist only for visualization purposes, they are not used in game

Merging should be done to all Tile blueprints and blueprints used as ActorSpawners in live tiles, but you can perform it for any new/old blueprints. For example, it can be useful when you are backporting main buildings, allowing you to easily compare new/old models.

### 4. Configuring Tile/ActorSpawner replacement

After you have merged Tile or ActorSpawner you wanted, you need to configure it to actually see it in game instead of vanilla (live) Tile or ActorSpawner.

Go to `Content/OldTilesCore/Data/MAP_NAME` and create or open `ActorOverrideDB.uasset`. If you just created it, make a reference to that newly created DataTable inside `Content/ModdingCore/Gameplay/BP_HorologiumMod_150.uasset`, in variable `ThemeToDataTable`:

![ThemeToDataTable](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/ThemeToDataTable.png?raw=true)

Inside `ActorOverrideDB.uasset` add a new row with the name corresponding to package path of live Tile or ActorSpawner blueprint that you want to "replace" in game. Click on that row to make sure it's active, and for `ReplaceWith` property select blueprint which you merged in previous step (remember that you can drag-and-drop it from Content Browser). After configuring `ActorOverrideDB.uasset` should look like this:

![ActorOverrideDB](https://github.com/olshab/DBDOldTiles_UE5/blob/main/Guide/ActorOverrideDB.png?raw=true)

### 5. Packaging

In pakchunk you need to always include that following folders:

- `Content/MergedTiles`
- `Content/OriginalTiles`
- `Content/OldTilesCore`
- `Content/ModdingCore`

It's recommended to not include `Content/NewTiles` as a whole since that folder contains assets from live version which you don't want to see anyway. However, it is necessary to include the following files (with corresponding `*.uexp` files), because they are used as a base class for merged blueprints from `Content/MergedTiles`:

- `Content/NewTiles/Tiles/TileBase01.uasset`
- `Content/NewTiles/Tiles/00-Common/BP_BaseEscapeTile.uasset`

### 6. Testing in-game

This mod needs the following modloader: [**ModLoader**](https://drive.google.com/file/d/1fZwSOcxpMJdZBNND8CLDkyMORkjxaWQK/view?usp=sharing)

After you have packaged the mod into pakchunk, you can load into the map (for example, in KYF) you are working on and find your replaced Tile/ActorSpawner. However, you might want to easily spawn and debug the tile. This can be done using [**Tile Editor**](https://drive.google.com/file/d/1PGMZi7YAJdMaGU2XUuQb5Hbx_3EsVkYf/view?usp=sharing).
