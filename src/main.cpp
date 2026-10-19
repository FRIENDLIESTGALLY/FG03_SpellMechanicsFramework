// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mechanics.h"
#include "Settings.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}
		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Mechanics::OnDataLoaded();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Mechanics::OnGameLoad();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	logs::info("Spell Mechanics Framework loading.");

	Settings::Get().Load();
	Mechanics::InstallHooks();

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logs::error("Could not register the SKSE message listener.");
	}
	return true;
}
