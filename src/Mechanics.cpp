// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mechanics.h"
#include "PEPE.h"
#include "Settings.h"

namespace Mechanics
{
	namespace
	{
		using Source = RE::MagicSystem::CastingSource;
		using State = RE::MagicCaster::State;

		constexpr auto kPluginFile = "FG03_SpellMechanicsFramework.esp"sv;

		RE::BGSKeyword* g_runeKeyword = nullptr;
		RE::TESGlobal*  g_leftHandSchool = nullptr;    // skill of the spell in the left hand (actor value index, 0 = none)
		RE::TESGlobal*  g_rightHandSchool = nullptr;   // the same for the right hand
		RE::TESGlobal*  g_castingBothHands = nullptr;  // 1 while both hands cast at once (not a dual cast)
		RE::Setting*    g_maxRunesSetting = nullptr;
		std::int32_t    g_baseMaxRunes = 1;
		// Explosion form -> the player spell whose projectile last set it off (explosions carry no spell).
		std::unordered_map<RE::BGSExplosion*, RE::MagicItem*> g_explosionSpells;

		std::mutex g_lock;

		// A bounded "seen" set keyed by reference handle (handles carry an age counter, so reuse is rare).
		class HandleSet
		{
		public:
			bool Insert(std::uint32_t a_handle)
			{
				if (!_set.insert(a_handle).second) {
					return false;
				}
				_order.push_back(a_handle);
				if (_order.size() > kCap) {
					_set.erase(_order.front());
					_order.pop_front();
				}
				return true;
			}
			[[nodiscard]] bool Contains(std::uint32_t a_handle) const { return _set.contains(a_handle); }

		private:
			static constexpr std::size_t       kCap = 8192;
			std::unordered_set<std::uint32_t> _set;
			std::deque<std::uint32_t>         _order;
		};

		HandleSet g_seenProjectiles;
		HandleSet g_spawnedProjectiles;
		HandleSet g_seenExplosions;

		struct CasterState
		{
			RE::MagicItem* spell{ nullptr };          // spell whose charge multiplier is cached
			float          mult{ 1.0f };
			float          lastTimer{ -1.0f };
			State          lastState{ State::kNone };  // for diagnostics and change detection
			RE::MagicItem* lastSpellSeen{ nullptr };
			int            lastStep{ 0 };              // sign of the timer's last change, to learn its direction
		};
		std::unordered_map<RE::ActorMagicCaster*, CasterState> g_casters;
		int g_timerDir = 0;  // -1 = charge timer counts down, +1 = counts up, 0 = not learned yet

		std::unordered_set<std::string> g_reportedFailures;

#define FG03_DEBUG(...)                       \
	do {                                     \
		if (Settings::Get().debugLog) {      \
			logs::info(__VA_ARGS__);         \
		}                                    \
	} while (false)

		bool IsPlayer(const RE::Actor* a_actor)
		{
			return a_actor && a_actor == RE::PlayerCharacter::GetSingleton();
		}

		bool Allowed(const RE::Actor* a_actor)
		{
			return a_actor && (!Settings::Get().playerOnly || IsPlayer(a_actor));
		}

		// "Firebolt [00012FD0]": several records share a name, so the log names the exact one.
		std::string Name(const RE::MagicItem* a_spell)
		{
			if (!a_spell) {
				return "none";
			}
			const char* name = a_spell->GetName();
			return std::format("{} [{:08X}]", (name && *name) ? name : "<unnamed>", a_spell->GetFormID());
		}

		// Runs every perk entry in a PEPE category on the "Mod Spell Magnitude" entry point.
		// That entry point has three condition tabs (Perk Owner, Spell, Target), so PEPE takes two
		// arguments: the spell and the target. With no known target, the caster stands in.
		float Eval(RE::Actor* a_actor, RE::MagicItem* a_spell, std::string_view a_category, float a_base, RE::TESObjectREFR* a_target = nullptr)
		{
			auto* api = PEPE::Request();
			if (!api || !a_actor || !a_spell) {
				return a_base;
			}
			float        out = a_base;
			RE::TESForm* args[2]{ a_spell, a_target ? a_target : a_actor };
			const auto   result = api->Apply(a_actor, PEPE::EntryPoint::kModSpellMagnitude, std::span<RE::TESForm*>(args), std::span<RE::ExtraDataList*>(),
				  &out, a_category, 255, PEPE::kNone);
			if (result != PEPE::RequestResult::kSuccess) {
				if (g_reportedFailures.emplace(a_category).second) {
					logs::warn("PEPE rejected category {} (result {}).", a_category, static_cast<std::int32_t>(result));
				}
				return a_base;
			}
			return out;
		}

		std::string_view HandCategory(Source a_source, std::string_view a_left, std::string_view a_right)
		{
			switch (a_source) {
			case Source::kLeftHand:
				return a_left;
			case Source::kRightHand:
				return a_right;
			default:
				return {};
			}
		}

		float ClampMult(float a_value)
		{
			const auto& s = Settings::Get();
			return std::clamp(a_value, s.minMult, s.maxMult);
		}

		float EvalMult(RE::Actor* a_actor, RE::MagicItem* a_spell, Source a_source, std::string_view a_generic, std::string_view a_left, std::string_view a_right,
			RE::TESObjectREFR* a_target = nullptr)
		{
			float mult = Eval(a_actor, a_spell, a_generic, 1.0f, a_target);
			if (const auto hand = HandCategory(a_source, a_left, a_right); !hand.empty()) {
				mult *= Eval(a_actor, a_spell, hand, 1.0f, a_target);
			}
			return ClampMult(mult);
		}

		int EvalCount(RE::Actor* a_actor, RE::MagicItem* a_spell, Source a_source, std::string_view a_generic, std::string_view a_left, std::string_view a_right,
			RE::TESObjectREFR* a_target = nullptr)
		{
			float count = Eval(a_actor, a_spell, a_generic, 0.0f, a_target);
			if (const auto hand = HandCategory(a_source, a_left, a_right); !hand.empty()) {
				count += Eval(a_actor, a_spell, hand, 0.0f, a_target);
			}
			return std::clamp(static_cast<int>(std::lround(count)), 0, Settings::Get().maxExtraProjectiles);
		}

		// ---------------------------------------------------------------- projectiles: range + extra copies

		void LaunchCopies(RE::ActorHandle a_shooter, RE::SpellItem* a_spell, RE::NiPoint3 a_origin, float a_angleX, float a_angleZ, int a_count)
		{
			auto shooter = a_shooter.get();
			if (!shooter) {
				return;
			}
			const float spread = Settings::Get().extraSpreadDegrees * (std::numbers::pi_v<float> / 180.0f);
			for (int i = 1; i <= a_count; ++i) {
				const float side = (i % 2) ? 1.0f : -1.0f;
				const float step = static_cast<float>((i + 1) / 2);
				const RE::Projectile::ProjectileRot rot{ a_angleX, a_angleZ + side * step * spread };
				RE::ProjectileHandle                out{};
				RE::Projectile::LaunchSpell(&out, shooter.get(), a_spell, a_origin, rot);
				if (out) {
					std::scoped_lock l(g_lock);
					g_spawnedProjectiles.Insert(out.native_handle());
				}
			}
			FG03_DEBUG("Launched {} extra projectile(s) of {}.", a_count, Name(a_spell));
		}

		void ApplyRuneLimit(RE::Actor* a_actor, RE::MagicItem* a_spell);

		void OnProjectileUpdate(RE::Projectile* a_proj)
		{
			auto& rt = a_proj->GetProjectileRuntimeData();
			auto* spell = rt.spell;
			if (!spell) {
				return;
			}
			const auto shooterRef = rt.shooter.get();
			auto*      actor = shooterRef ? shooterRef->As<RE::Actor>() : nullptr;
			if (!Allowed(actor)) {
				return;
			}

			const auto handle = a_proj->GetHandle().native_handle();
			bool       isCopy = false;
			{
				std::scoped_lock l(g_lock);
				if (!g_seenProjectiles.Insert(handle)) {
					return;  // already handled on its first update
				}
				isCopy = g_spawnedProjectiles.Contains(handle);
			}
			// Projectiles restored from a save (placed runes, bolts in flight) were handled when cast;
			// treating them as new would scale their range again and launch more copies.
			using Flag = RE::Projectile::Flags;
			if (rt.livingTime > 1.0f || rt.flags.any(Flag::kProcessedImpacts, Flag::kAddedVisualEffectOnGround, Flag::kDestroyed)) {
				FG03_DEBUG("Skipping restored {} (alive {:.1f}s).", Name(spell), rt.livingTime);
				return;
			}
			const auto* base = a_proj->GetProjectileBase();
			if (IsPlayer(actor)) {
				// Remember which explosions this spell sets off, so its radius applies to them.
				{
					std::scoped_lock l(g_lock);
					if (base && base->data.explosionType) {
						g_explosionSpells[base->data.explosionType] = spell;
					}
					for (const auto* effect : spell->effects) {
						if (effect && effect->baseEffect && effect->baseEffect->data.explosion) {
							g_explosionSpells[effect->baseEffect->data.explosion] = spell;
						}
					}
				}
				// Backup for the equip-time rune limit: a rune is a spell lobber (grenade) projectile.
				if (base && base->IsGrenade()) {
					ApplyRuneLimit(actor, spell);
				}
			}

			const auto  source = rt.castingSource;
			const auto  targetPtr = rt.desiredTarget.get();
			auto*       target = targetPtr.get();
			const float rangeMult = EvalMult(actor, spell, source, Category::kRange, Category::kRangeLeft, Category::kRangeRight, target);
			if (rangeMult != 1.0f) {
				const float before = rt.range;
				rt.range *= rangeMult;
				FG03_DEBUG("{} range {} -> {} (x{})", Name(spell), before, rt.range, rangeMult);
			}

			if (isCopy) {
				return;  // copies never spawn more copies
			}
			const int extra = EvalCount(actor, spell, source, Category::kProjectiles, Category::kProjectilesLeft, Category::kProjectilesRight, target);
			auto*     spellItem = spell->As<RE::SpellItem>();
			if (extra <= 0 || !spellItem) {
				return;
			}
			const auto  origin = a_proj->GetPosition();
			const float angleX = a_proj->GetAngleX();
			const float angleZ = a_proj->GetAngleZ();
			const auto  shooterHandle = actor->GetHandle();
			// Launch on the next task pass, not from inside the projectile manager's update.
			SKSE::GetTaskInterface()->AddTask([shooterHandle, spellItem, origin, angleX, angleZ, extra]() {
				LaunchCopies(shooterHandle, spellItem, origin, angleX, angleZ, extra);
			});
		}

		template <std::size_t I>
		struct ProjectileUpdateHook
		{
			static void thunk(RE::Projectile* a_this, float a_delta)
			{
				if (a_this) {
					OnProjectileUpdate(a_this);
				}
				func(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		template <std::size_t I>
		void InstallProjectileHook(REL::VariantID a_vtable, std::string_view a_name)
		{
			REL::Relocation<std::uintptr_t> vtable{ a_vtable };
			ProjectileUpdateHook<I>::func = vtable.write_vfunc(REL::Relocate<std::size_t>(0xAB, 0xAC), ProjectileUpdateHook<I>::thunk);
			logs::info("Hooked {}::UpdateImpl", a_name);
		}

		// ---------------------------------------------------------------- explosions: radius

		HandleSet g_diagExplosions;
		int       g_diagExplosionCount = 0;

		// Called from Initialize (after), Update and FindTargets (before); whichever first sees a
		// player-owned explosion scales it, once.
		void OnExplosion(RE::Explosion* a_explosion, std::string_view a_from)
		{
			auto&      rt = a_explosion->GetExplosionRuntimeData();
			const auto handle = a_explosion->GetHandle().native_handle();
			auto       ownerPtr = rt.actorOwner.get();
			auto*      actor = ownerPtr.get();
			decltype(ownerPtr) causePtr;
			if (!actor && rt.actorCause) {
				causePtr = rt.actorCause->actor.get();
				actor = causePtr.get();
			}
			RE::MagicItem* spell = rt.magicCaster ? rt.magicCaster->currentSpell : nullptr;

			if (Settings::Get().debugLog) {
				std::scoped_lock l(g_lock);
				if (g_diagExplosionCount < 30 && g_diagExplosions.Insert(handle)) {
					++g_diagExplosionCount;
					logs::info("Explosion via {}: owner {} (player {}), caster {}, spell {}, weapon {}, radius {}, age {}",
						a_from, actor ? actor->GetName() : "none", IsPlayer(actor), rt.magicCaster ? "yes" : "no", Name(spell),
						rt.weaponSource ? "yes" : "no", rt.radius, rt.age);
				}
			}

			if (!Allowed(actor)) {
				return;
			}
			bool matched = false;
			{
				std::scoped_lock l(g_lock);
				if (!g_seenExplosions.Insert(handle)) {
					return;  // already scaled (or found to have no spell) by an earlier hook
				}
				// Explosions do not carry their spell; match the explosion form to the player spell that
				// last fired a projectile with it. Weapon and shout explosions find no match.
				if (!spell && IsPlayer(actor) && !rt.weaponSource) {
					auto* base = a_explosion->GetBaseObject();
					auto* form = base ? base->As<RE::BGSExplosion>() : nullptr;
					if (const auto it = g_explosionSpells.find(form); form && it != g_explosionSpells.end()) {
						spell = it->second;
						matched = true;
					}
				}
			}
			if (!spell) {
				return;
			}

			const float mult = ClampMult(Eval(actor, spell, Category::kRadius, 1.0f));
			if (mult != 1.0f) {
				const float before = rt.radius;
				rt.radius *= mult;
				rt.imodRadius *= mult;
				FG03_DEBUG("{} explosion radius {} -> {} (x{}) via {}{}", Name(spell), before, rt.radius, mult, a_from, matched ? " [matched by explosion form]" : "");
			}
		}

		struct ExplosionInitializeHook
		{
			static void thunk(RE::Explosion* a_this)
			{
				func(a_this);
				if (a_this) {
					OnExplosion(a_this, "Initialize"sv);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ExplosionUpdateHook
		{
			static void thunk(RE::Explosion* a_this, float a_delta)
			{
				if (a_this) {
					OnExplosion(a_this, "Update"sv);
				}
				func(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ExplosionFindTargetsHook
		{
			static void thunk(RE::Explosion* a_this)
			{
				if (a_this) {
					OnExplosion(a_this, "FindTargets"sv);
				}
				func(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// ---------------------------------------------------------------- casters: cast time, rune limit, globals

		// The skill of a hand's spell as its actor value index (Alteration 18, Conjuration 19,
		// Destruction 20, Illusion 21, Restoration 22), or 0 when the hand holds no spell.
		float SchoolOf(RE::TESForm* a_form)
		{
			auto* spell = a_form ? a_form->As<RE::SpellItem>() : nullptr;
			if (!spell) {
				return 0.0f;
			}
			const auto skill = spell->GetAssociatedSkill();
			return skill == RE::ActorValue::kNone ? 0.0f : static_cast<float>(std::to_underlying(skill));
		}

		// Any state but idle: charging, ready, or releasing.
		bool IsCasting(RE::Actor* a_actor, Source a_source)
		{
			auto* caster = a_actor->GetMagicCaster(a_source);
			return caster && caster->state.get() != State::kNone;
		}

		void UpdatePlayerGlobals(RE::Actor* a_player)
		{
			static std::uint32_t tick = 0;
			if ((++tick % 4) != 0) {
				return;
			}
			const float left = SchoolOf(a_player->GetEquippedObject(true));
			const float right = SchoolOf(a_player->GetEquippedObject(false));
			const bool  both = left != 0.0f && right != 0.0f && !a_player->IsDualCasting() && IsCasting(a_player, Source::kLeftHand) &&
			                  IsCasting(a_player, Source::kRightHand);
			const float castingBoth = both ? 1.0f : 0.0f;

			static float lastLeft = -1.0f, lastRight = -1.0f, lastBoth = -1.0f;
			if (left != lastLeft || right != lastRight || castingBoth != lastBoth) {
				FG03_DEBUG("Globals: left hand school {}, right hand school {}, casting both hands {}", left, right, castingBoth);
				lastLeft = left;
				lastRight = right;
				lastBoth = castingBoth;
			}
			if (g_leftHandSchool) {
				g_leftHandSchool->value = left;
			}
			if (g_rightHandSchool) {
				g_rightHandSchool->value = right;
			}
			if (g_castingBothHands) {
				g_castingBothHands->value = castingBoth;
			}
		}


		// ---------------------------------------------------------------- runes: the player's rune limit

		// A rune is a spell whose effect throws a lobber (grenade) projectile at a location. The
		// MagicRune keyword alone misses mod runes (Magic Redone's runes do not carry it).
		bool IsRuneSpell(const RE::MagicItem* a_spell)
		{
			if (!a_spell) {
				return false;
			}
			return std::ranges::any_of(a_spell->effects, [](const RE::Effect* a_effect) {
				const auto* mgef = a_effect ? a_effect->baseEffect : nullptr;
				if (!mgef) {
					return false;
				}
				if (g_runeKeyword && mgef->HasKeyword(g_runeKeyword)) {
					return true;
				}
				return mgef->data.delivery == RE::MagicSystem::Delivery::kTargetLocation && mgef->data.projectileBase &&
				       mgef->data.projectileBase->IsGrenade();
			});
		}

		std::atomic<int> g_runeLimit{ 0 };  // the limit the perks give right now; 0 = not evaluated yet

		int EvalRuneLimit(RE::Actor* a_actor, RE::MagicItem* a_spell)
		{
			const float value = Eval(a_actor, a_spell, Category::kMaxRunes, static_cast<float>(g_baseMaxRunes));
			return std::clamp(static_cast<int>(std::lround(value)), 1, Settings::Get().maxRunesCap);
		}

		void SetRuneLimit(int a_limit, const RE::MagicItem* a_spell, std::string_view a_from)
		{
			const int old = g_runeLimit.exchange(a_limit);
			if (g_maxRunesSetting) {
				g_maxRunesSetting->data.i = a_limit;
			}
			if (old != a_limit) {
				FG03_DEBUG("Rune limit {} -> {} for {} ({})", old, a_limit, Name(a_spell), a_from);
			}
		}

		void ApplyRuneLimit(RE::Actor* a_actor, RE::MagicItem* a_spell)
		{
			SetRuneLimit(EvalRuneLimit(a_actor, a_spell), a_spell, "rune projectile"sv);
		}

		// Sets the limit from the rune spells in the player's hands, before anything is cast.
		void UpdateEquippedRuneLimit(RE::Actor* a_player)
		{
			static RE::TESForm*  lastLeft = nullptr;
			static RE::TESForm*  lastRight = nullptr;
			static std::uint32_t tick = 0;
			auto* left = a_player->GetEquippedObject(true);
			auto* right = a_player->GetEquippedObject(false);
			// Also re-check about twice a second, so a perk gained while holding the spell applies.
			if (left == lastLeft && right == lastRight && (++tick % 30) != 0) {
				return;
			}
			lastLeft = left;
			lastRight = right;
			int            best = 0;
			RE::MagicItem* bestSpell = nullptr;
			for (auto* form : { left, right }) {
				auto* spell = form ? form->As<RE::MagicItem>() : nullptr;
				if (!IsRuneSpell(spell)) {
					continue;
				}
				if (const int limit = EvalRuneLimit(a_player, spell); limit > best) {
					best = limit;
					bestSpell = spell;
				}
			}
			if (best > 0) {
				SetRuneLimit(best, bestSpell, "equipped"sv);
			}
		}

		// The game keeps the player's runes in PlayerCharacter::runesCast and removes the oldest when a
		// new one would pass its limit. If it removes one while fewer than our limit are out, it is not
		// using the setting we changed, so the plugin takes over: it moves every new rune out of that
		// list (the game then never removes one) and removes the oldest itself past the perk limit.
		constexpr int kRuneModeSetting = 0;  // only change iMaxPlayerRunes
		constexpr int kRuneModeAuto = 1;     // change it, and take over if the game ignores it
		constexpr int kRuneModeManaged = 2;  // always manage the limit in the plugin

		std::vector<std::uint32_t>       g_lastRunes;     // the game's list at the last check
		std::deque<RE::ProjectileHandle> g_managedRunes;  // runes the plugin holds, oldest first
		bool                             g_managing = false;

		bool RuneAlive(const RE::ProjectileHandle& a_handle)
		{
			const auto proj = a_handle.get();
			return proj && !proj->IsDeleted() && !proj->IsMarkedForDeletion() && !proj->IsDisabled();
		}

		// Runs on the main thread from the ticker.
		void TrackRunes()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			auto&      runes = player->GetPlayerRuntimeData().runesCast;
			const int  limit = g_runeLimit.load();
			const int  mode = Settings::Get().runeMode;

			std::vector<std::uint32_t> now;
			now.reserve(runes.size());
			for (const auto& handle : runes) {
				now.push_back(handle.native_handle());
			}
			if (now != g_lastRunes) {
				int added = 0;
				int removed = 0;
				for (const auto h : now) {
					added += std::ranges::find(g_lastRunes, h) == g_lastRunes.end();
				}
				for (const auto h : g_lastRunes) {
					removed += std::ranges::find(now, h) == now.end();
				}
				FG03_DEBUG("Game rune list {} -> {} (+{} -{}); plugin holds {}; limit {}, iMaxPlayerRunes {}", g_lastRunes.size(), now.size(),
					added, removed, g_managedRunes.size(), limit, g_maxRunesSetting ? g_maxRunesSetting->data.i : -1);
				// A new rune went in and an old one came out while there was room under the limit.
				if (!g_managing && mode == kRuneModeAuto && added > 0 && removed > 0 && static_cast<int>(g_lastRunes.size()) < limit) {
					g_managing = true;
					logs::warn("The game removed a rune with {} out and a limit of {}: it ignores the changed iMaxPlayerRunes. "
							   "The plugin manages the rune limit from now on.",
						g_lastRunes.size(), limit);
				}
				g_lastRunes = std::move(now);
			}

			if (!g_managing && mode != kRuneModeManaged) {
				return;
			}
			g_managing = true;
			if (!runes.empty()) {
				for (const auto& handle : runes) {
					g_managedRunes.push_back(handle);
				}
				runes.clear();
				g_lastRunes.clear();
			}
			std::erase_if(g_managedRunes, [](const RE::ProjectileHandle& a_handle) { return !RuneAlive(a_handle); });
			const int cap = limit > 0 ? limit : g_baseMaxRunes;
			while (static_cast<int>(g_managedRunes.size()) > cap) {
				if (const auto proj = g_managedRunes.front().get()) {
					proj->Kill();
				}
				g_managedRunes.pop_front();
				FG03_DEBUG("Removed the oldest rune ({} out, limit {}).", g_managedRunes.size(), cap);
			}
		}

		// ---------------------------------------------------------------- casters: cast time

		// Runs on the main thread from the ticker. The game moves the charge timer each frame; whatever
		// it moved since the last tick is scaled, so the result does not depend on the tick rate and
		// stops by itself while the game is paused.
		void OnCasterTick(RE::Actor* a_actor, RE::ActorMagicCaster* a_caster)
		{
			auto*       spell = a_caster->currentSpell;
			const auto  state = a_caster->state.get();
			const float timer = a_caster->castingTimer;

			std::scoped_lock l(g_lock);
			auto&            st = g_casters[a_caster];

			// Diagnostics: every state / spell change of the player's casters.
			if (IsPlayer(a_actor) && Settings::Get().debugLog && (state != st.lastState || spell != st.lastSpellSeen)) {
				logs::info("Caster {}: state {} -> {}, spell {}, timer {}", static_cast<int>(a_caster->GetCastingSource()),
					static_cast<int>(st.lastState), static_cast<int>(state), Name(spell), timer);
			}
			st.lastState = state;
			st.lastSpellSeen = spell;

			// In game the player's casters go 0 (idle) -> 2 (charging, timer counts down) -> 3 (ready)
			// -> 4 (releasing) -> 0. Anything before "ready" with a spell counts as charging.
			const bool charging = spell && state != State::kNone && state != State::kReady && state != State::kUnk04 &&
			                      state != State::kCasting && state != State::kUnk07 && state != State::kUnk08 && state != State::kUnk09;
			if (!charging) {
				st.spell = nullptr;
				st.lastTimer = -1.0f;
				st.lastStep = 0;
				return;
			}
			if (st.spell != spell) {
				st.spell = spell;
				st.lastTimer = -1.0f;
				st.lastStep = 0;
				st.mult = EvalMult(a_actor, spell, a_caster->GetCastingSource(), Category::kCastTime, Category::kCastTimeLeft, Category::kCastTimeRight);
				FG03_DEBUG("{} charging (state {}, timer {}), cast time x{}", Name(spell), static_cast<int>(state), timer, st.mult);
			}

			const float step = st.lastTimer >= 0.0f ? timer - st.lastTimer : 0.0f;
			if (step != 0.0f) {
				// Learn the direction from two changes in a row that agree (a reset is a single jump).
				const int sign = step < 0.0f ? -1 : 1;
				if (g_timerDir == 0 && st.lastStep == sign) {
					g_timerDir = sign;
					logs::info("Charge timer counts {} (state {}).", g_timerDir < 0 ? "down" : "up", static_cast<int>(state));
				}
				st.lastStep = sign;
				if (g_timerDir == sign && st.mult != 1.0f) {
					float adjusted = timer + step * (1.0f / st.mult - 1.0f);
					if (g_timerDir < 0) {
						adjusted = (std::max)(adjusted, 0.0f);
					}
					a_caster->castingTimer = adjusted;
				}
			}
			st.lastTimer = a_caster->castingTimer;
		}

		// ---------------------------------------------------------------- the ticker

		// Neither ActorMagicCaster::Update nor PlayerCharacter::Update is called through its vtable for
		// the player, so vtable hooks never ran. Instead a worker thread posts one task to the main thread
		// about every 10 ms, never more than one at a time (a task that re-queues itself would run
		// endlessly inside one frame).
		void OnActorTick(RE::Actor* a_actor)
		{
			if (IsPlayer(a_actor)) {
				UpdatePlayerGlobals(a_actor);
				UpdateEquippedRuneLimit(a_actor);
				const auto& s = Settings::Get();
				if (s.debugLog || s.runeMode != kRuneModeSetting) {
					TrackRunes();
				}
			}
			for (auto* caster : a_actor->GetActorRuntimeData().magicCasters) {
				if (caster) {
					OnCasterTick(a_actor, caster);
				}
			}
		}

		void Tick()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->Is3DLoaded()) {
				return;
			}
			static bool announced = false;
			if (!announced) {
				announced = true;
				logs::info("Ticker is running.");
			}
			OnActorTick(player);
			if (Settings::Get().playerOnly) {
				return;
			}
			if (auto* lists = RE::ProcessLists::GetSingleton()) {
				for (auto& handle : lists->highActorHandles) {
					const auto actor = handle.get();
					if (actor && actor.get() != player) {
						OnActorTick(actor.get());
					}
				}
			}
		}

		std::atomic<bool> g_tickPending{ false };

		void StartTicker()
		{
			static std::once_flag once;
			std::call_once(once, []() {
				std::thread([]() {
					for (;;) {
						std::this_thread::sleep_for(10ms);
						if (!g_tickPending.exchange(true)) {
							SKSE::GetTaskInterface()->AddTask([]() {
								Tick();
								g_tickPending = false;
							});
						}
					}
				}).detach();
			});
		}
	}

	void OnDataLoaded()
	{
		g_runeKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(0x109D79);  // MagicRune
		if (auto* dh = RE::TESDataHandler::GetSingleton()) {
			g_leftHandSchool = dh->LookupForm<RE::TESGlobal>(0x800, kPluginFile);     // FG03_LeftHandSpellSchool
			g_castingBothHands = dh->LookupForm<RE::TESGlobal>(0x801, kPluginFile);   // FG03_CastingBothHands
			g_rightHandSchool = dh->LookupForm<RE::TESGlobal>(0x810, kPluginFile);    // FG03_RightHandSpellSchool
		}
		if (auto* gmst = RE::GameSettingCollection::GetSingleton()) {
			g_maxRunesSetting = gmst->GetSetting("iMaxPlayerRunes");
			if (g_maxRunesSetting) {
				g_baseMaxRunes = g_maxRunesSetting->data.i;
			}
		}
		PEPE::Request();
		const auto found = [](const void* a_form) { return a_form ? "found" : "MISSING"; };
		logs::info("Data loaded: rune keyword {}, globals left/right/both {}/{}/{}, iMaxPlayerRunes base {}, rune mode {}",
			found(g_runeKeyword), found(g_leftHandSchool), found(g_rightHandSchool), found(g_castingBothHands), g_baseMaxRunes,
			Settings::Get().runeMode);
		StartTicker();
	}

	void OnGameLoad()
	{
		// Handles from the previous session mean nothing now; the game's list is re-read fresh.
		SKSE::GetTaskInterface()->AddTask([]() {
			g_managedRunes.clear();
			g_lastRunes.clear();
		});
	}

	void InstallHooks()
	{
		InstallProjectileHook<0>(RE::VTABLE_MissileProjectile[0], "MissileProjectile");
		InstallProjectileHook<1>(RE::VTABLE_FlameProjectile[0], "FlameProjectile");
		InstallProjectileHook<2>(RE::VTABLE_BeamProjectile[0], "BeamProjectile");
		InstallProjectileHook<3>(RE::VTABLE_ConeProjectile[0], "ConeProjectile");
		InstallProjectileHook<4>(RE::VTABLE_BarrierProjectile[0], "BarrierProjectile");
		InstallProjectileHook<5>(RE::VTABLE_GrenadeProjectile[0], "GrenadeProjectile");

		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_Explosion[0] };
		ExplosionInitializeHook::func = vtable.write_vfunc(REL::Relocate<std::size_t>(0xA2, 0xA3), ExplosionInitializeHook::thunk);
		ExplosionUpdateHook::func = vtable.write_vfunc(REL::Relocate<std::size_t>(0xA3, 0xA4), ExplosionUpdateHook::thunk);
		ExplosionFindTargetsHook::func = vtable.write_vfunc(REL::Relocate<std::size_t>(0xA4, 0xA5), ExplosionFindTargetsHook::thunk);
		logs::info("Hooked Explosion::Initialize / Update / FindTargets");
	}
}