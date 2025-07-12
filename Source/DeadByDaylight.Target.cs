// (C) Behaviour Interactive Inc. - All Rights Reserved.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// This file is proprietary and confidential.

using UnrealBuildTool;
using System.Collections.Generic;

public class DeadByDaylightTarget : TargetRules
{
	public DeadByDaylightTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		ExtraModuleNames.AddRange( new string[] { "DeadByDaylight" } );
	}
}
