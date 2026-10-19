// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Mechanics
{
	// PEPE categories (the part of the keyword EditorID after PEPE's required "GROUP__").
	// Each is evaluated on the "Mod Spell Magnitude" entry point (Perk Owner, Spell and Target
	// condition tabs). Multipliers start at 1.0; counts start at 0.
	// PEPE matches a category by PREFIX, so no name may be the start of another: that is why the
	// either-hand categories end in _Any rather than being the bare name.
	namespace Category
	{
		inline constexpr auto kRadius = "FG03_SpellRadius_Any"sv;
		inline constexpr auto kRadiusLeft = "FG03_SpellRadius_Left"sv;
		inline constexpr auto kRadiusRight = "FG03_SpellRadius_Right"sv;
		inline constexpr auto kRange = "FG03_SpellRange_Any"sv;
		inline constexpr auto kRangeLeft = "FG03_SpellRange_Left"sv;
		inline constexpr auto kRangeRight = "FG03_SpellRange_Right"sv;
		inline constexpr auto kCastTime = "FG03_SpellCastTime_Any"sv;
		inline constexpr auto kCastTimeLeft = "FG03_SpellCastTime_Left"sv;
		inline constexpr auto kCastTimeRight = "FG03_SpellCastTime_Right"sv;
		inline constexpr auto kProjectiles = "FG03_SpellProjectiles_Any"sv;
		inline constexpr auto kProjectilesLeft = "FG03_SpellProjectiles_Left"sv;
		inline constexpr auto kProjectilesRight = "FG03_SpellProjectiles_Right"sv;
		inline constexpr auto kMaxRunes = "FG03_MaxRunes"sv;
	}

	void OnDataLoaded();
	void OnGameLoad();
	void InstallHooks();
}
