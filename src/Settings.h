// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

struct Settings
{
	bool  playerOnly{ true };
	bool  debugLog{ false };
	float minMult{ 0.1f };
	float maxMult{ 10.0f };
	int   maxExtraProjectiles{ 4 };
	float extraSpreadDegrees{ 6.0f };
	int   maxRunesCap{ 20 };
	int   runeMode{ 1 };  // 0 = iMaxPlayerRunes only, 1 = auto (take over if the game ignores it), 2 = always manage

	static Settings& Get()
	{
		static Settings s;
		return s;
	}

	void Load()
	{
		std::ifstream in("Data/SKSE/Plugins/FG03_SpellMechanicsFramework.ini");
		if (!in) {
			logs::info("No FG03_SpellMechanicsFramework.ini found; using defaults.");
			return;
		}
		std::string line;
		while (std::getline(in, line)) {
			const auto hash = line.find_first_of(";#");
			if (hash != std::string::npos) {
				line.erase(hash);
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto trim = [](std::string s) {
				const auto b = s.find_first_not_of(" \t\r");
				const auto e = s.find_last_not_of(" \t\r");
				return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
			};
			const auto key = trim(line.substr(0, eq));
			const auto val = trim(line.substr(eq + 1));
			try {
				if (key == "bPlayerOnly") playerOnly = std::stoi(val) != 0;
				else if (key == "bDebugLog") debugLog = std::stoi(val) != 0;
				else if (key == "fMinMult") minMult = std::stof(val);
				else if (key == "fMaxMult") maxMult = std::stof(val);
				else if (key == "iMaxExtraProjectiles") maxExtraProjectiles = std::stoi(val);
				else if (key == "fExtraProjectileSpreadDegrees") extraSpreadDegrees = std::stof(val);
				else if (key == "iMaxRunesCap") maxRunesCap = std::stoi(val);
				else if (key == "iRuneLimitMode") runeMode = std::clamp(std::stoi(val), 0, 2);
			} catch (...) {
				logs::warn("Ignoring bad value for {}: '{}'", key, val);
			}
		}
		logs::info("Settings: playerOnly={} debug={} mult=[{}, {}] maxExtra={} spread={} runeCap={} runeMode={}",
			playerOnly, debugLog, minMult, maxMult, maxExtraProjectiles, extraSpreadDegrees, maxRunesCap, runeMode);
	}
};
