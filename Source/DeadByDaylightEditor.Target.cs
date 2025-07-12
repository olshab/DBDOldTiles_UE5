// (C) Behaviour Interactive Inc. - All Rights Reserved.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// This file is proprietary and confidential.

using UnrealBuildTool;
using System.Collections.Generic;

public class DeadByDaylightEditorTarget : TargetRules
{
	public DeadByDaylightEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		ExtraModuleNames.AddRange( new string[] { "DeadByDaylight" } );
	}
}
