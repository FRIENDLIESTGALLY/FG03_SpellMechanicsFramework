// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Minimal declaration of Perk Entry Point Extender's public plugin interface (version 3).
// PEPE exports PEPE_RequestInterfaceImpl(Version) from PerkEntryPointExtender.dll; the returned
// object's virtual table is laid out as declared below (one destructor, then the methods in order).
namespace PEPE
{
	enum class RequestResult : std::int32_t
	{
		kSuccess = -1,
		kInvalidAPI,
		kUnsupported,
		kUnmatchedArgs,
		kBadFormArg,
		kBadEntryPoint,
		kBadString,
		kNoActor,
		kInvalidActor,
		kInvalidOut,
		kUnknownFlags,
		kBadExtraData,
		kUnknownError
	};

	enum EntryPointFlag : std::uint64_t
	{
		kNone = 0
	};

	using EntryPoint = RE::BGSEntryPoint::ENTRY_POINT;

	struct Interface
	{
		virtual ~Interface() = default;                                                                                                                       // 0
		virtual std::int32_t  GetVersion() = 0;                                                                                                             // 1
		virtual RequestResult ApplyDeprecated1(RE::Actor*, EntryPoint, std::span<RE::TESForm*>, void*, const char*, std::uint8_t) = 0;                      // 2
		virtual RequestResult ApplyDeprecated2(RE::Actor*, EntryPoint, std::span<RE::TESForm*>, void*, const char*, std::uint8_t, EntryPointFlag) = 0;      // 3
		virtual RequestResult Apply(RE::Actor* a_target, EntryPoint a_entryPoint, std::span<RE::TESForm*> a_args, std::span<RE::ExtraDataList*> a_extras,
			void* a_out, const std::string_view& a_category, std::uint8_t a_channel, EntryPointFlag a_flags) = 0;                                          // 4
		virtual std::string_view GetCurrentCategory() = 0;                                                                                                  // 5
	};

	inline Interface* Request()
	{
		static Interface* api = nullptr;
		static bool       tried = false;
		if (api || tried) {
			return api;
		}
		tried = true;

		const auto module = REX::W32::GetModuleHandleW(L"PerkEntryPointExtender.dll");
		if (!module) {
			logs::error("PerkEntryPointExtender.dll is not loaded; perk categories will not apply.");
			return nullptr;
		}
		using request_t = void* (*)(std::int32_t);
		const auto request = reinterpret_cast<request_t>(REX::W32::GetProcAddress(module, "PEPE_RequestInterfaceImpl"));
		if (!request) {
			logs::error("PEPE_RequestInterfaceImpl export not found; PEPE version too old?");
			return nullptr;
		}
		api = static_cast<Interface*>(request(2));  // Version3
		if (api) {
			logs::info("Connected to Perk Entry Point Extender (interface version {}).", api->GetVersion());
		} else {
			logs::error("PEPE refused interface version 3.");
		}
		return api;
	}
}
