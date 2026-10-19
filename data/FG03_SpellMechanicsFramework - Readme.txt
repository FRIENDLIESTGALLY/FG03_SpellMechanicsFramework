================================================================================================
                                 SPELL MECHANICS FRAMEWORK
                                        Version 1.0.0
                                  by <your Nexus username>
================================================================================================

A modder's resource for perk authors. This mod adds NO perks, spells or gameplay changes of its
own. It gives your perks new things they can do to spells, which the Creation Kit cannot do on
its own:

  - Spell radius        make explosions and area effects bigger or smaller
  - Spell range         make projectiles fly further or less far
  - Cast time           make spells charge faster or slower
  - Extra projectiles   fire several copies of a spell with one cast
  - Rune limit          let the player keep more than one rune down at a time
  - Hand globals        conditions that know which school of magic is in each hand, and whether
                        both hands are casting at once

Everything is built in the Creation Kit with ordinary perk entries and conditions. No scripts,
no MCM, nothing to set up in game.

PLAYERS: install this when another mod lists it as a requirement. On its own it does nothing.


------------------------------------------------------------------------------------------------
CONTENTS
------------------------------------------------------------------------------------------------
   1. Requirements
   2. Installation
   3. How it works
   4. Making a perk, step by step
   5. Keyword reference
   6. Global reference
   7. Examples
   8. Rules that keep your perks safe
   9. How each mechanic behaves
  10. Settings (ini)
  11. Testing and the log
  12. Compatibility and known limitations
  13. FAQ
  14. Credits
  15. Changelog
  16. License and source code


------------------------------------------------------------------------------------------------
1. REQUIREMENTS
------------------------------------------------------------------------------------------------
  - SKSE64
  - Address Library for SKSE Plugins
  - Perk Entry Point Extender (PEPE) by NoahBoddie, plus its own requirements

  Tested on Skyrim Anniversary Edition 1.6.1170 with SKSE 2.2.6. The DLL is built with
  CommonLibSSE-NG, which supports SE 1.5.97, AE and VR in a single file, but only 1.6.1170 has
  been tested so far.


------------------------------------------------------------------------------------------------
2. INSTALLATION
------------------------------------------------------------------------------------------------
  Install with any mod manager and enable the plugin. Files:

    FG03_SpellMechanicsFramework.esp                  ESL-flagged, uses no full load order slot
    SKSE\Plugins\FG03_SpellMechanicsFramework.dll
    SKSE\Plugins\FG03_SpellMechanicsFramework.ini

  The plugin must load above every plugin that uses it. LOOT sorts this for you, because those
  plugins list it as a master.

  Removing it: the framework has no scripts. The only things it leaves in your save are the
  values of its three globals, which the game drops harmlessly, so it is safe to remove
  mid-playthrough. Remove every mod that requires it first.


------------------------------------------------------------------------------------------------
3. HOW IT WORKS
------------------------------------------------------------------------------------------------
  The Creation Kit's list of perk entry points is fixed, so no mod can add a new entry type
  such as "Mod Spell Projectiles" to it. Perk Entry Point Extender works around this, and this
  framework is built on PEPE:

    1. Your perk entry uses the vanilla entry point "Mod Spell Magnitude".
    2. Its first condition is a tag: HasKeyword with one of this mod's GROUP__FG03_ keywords.
    3. The game skips tagged entries when it works out a spell's real magnitude, so the entry
       never changes the spell's damage or strength.
    4. The framework asks PEPE for the entries with a given tag, and gets back the number your
       entries produce (for example "9 extra projectiles" or "range x1.5").

  "Mod Spell Magnitude" is only borrowed as a container. It was picked because it has three
  condition tabs (Perk Owner, Spell, Target). The Spell tab is what lets you limit an entry to
  certain spells, with EPMagic_SpellHasSkill, EPMagic_SpellHasKeyword or SpellHasCastingPerk.


------------------------------------------------------------------------------------------------
4. MAKING A PERK, STEP BY STEP
------------------------------------------------------------------------------------------------
  Before you start: your plugin needs FG03_SpellMechanicsFramework.esp as a master. The vanilla
  Creation Kit will not keep an .esp as a master. Use Creation Kit Platform Extended with
  bAllowMasterESP=true, or add the master in xEdit.

  1. In the Creation Kit, load FG03_SpellMechanicsFramework.esp and your plugin, with yours set
     as the active file.
  2. Create or open a perk, and add a perk entry:
        Entry type .......... Entry Point
        Entry Point ......... Mod Spell Magnitude
        Function ............ Add Value or Multiply Value (see the keyword reference)
        Value ............... your number
  3. On the Perk Owner tab, make the FIRST condition the tag:
        HasKeyword   GROUP__FG03_<mechanic>   == 1
  4. Add any other conditions after the tag:
        Perk Owner tab ...... skill level, HasPerk, IsDualCasting, the FG03 globals...
        Spell tab ........... EPMagic_SpellHasSkill, EPMagic_SpellHasKeyword, SpellHasCastingPerk
        Target tab .......... conditions on the target, where there is one
  5. Save, then test in game with  player.addperk <perk ID>  (find the ID with  help <name> 4).
  6. Turn on bDebugLog in the ini while testing. Every value the framework applies is logged.


------------------------------------------------------------------------------------------------
5. KEYWORD REFERENCE
------------------------------------------------------------------------------------------------
  All keywords are in FG03_SpellMechanicsFramework.esp.

  KEYWORD (the tag)                        CONTROLS                                  STARTS AT
  GROUP__FG03_SpellRadius_Any              explosion / area radius                   1.0  (x)
  GROUP__FG03_SpellRadius_Left / _Right    reserved, currently does nothing
  GROUP__FG03_SpellRange_Any               projectile range                          1.0  (x)
  GROUP__FG03_SpellRange_Left / _Right     range, spells from that hand only         1.0  (x)
  GROUP__FG03_SpellCastTime_Any            charge time                               1.0  (x)
  GROUP__FG03_SpellCastTime_Left / _Right  charge time, that hand only               1.0  (x)
  GROUP__FG03_SpellProjectiles_Any         extra projectiles per cast                0    (+)
  GROUP__FG03_SpellProjectiles_Left/_Right extra projectiles, that hand only         0    (+)
  GROUP__FG03_MaxRunes                     rune limit                                the game's
                                                                                     limit (+)

  (x) = a multiplier. Use Multiply Value: 1.5 = +50%, 0.7 = -30%. For cast time, below 1 is
        faster: 0.5 charges in half the time.
  (+) = a count. Use Add Value: 2 = two more.

  - Several entries stack: multipliers multiply together, counts add up.
  - _Any applies to both hands. The hand versions stack on top of it: a left-hand cast gets
    _Any x _Left.
  - Results are kept inside the limits in the ini.
  - Use the names exactly. PEPE matches a tag by how its name BEGINS, which is why the
    either-hand versions end in _Any.


------------------------------------------------------------------------------------------------
6. GLOBAL REFERENCE
------------------------------------------------------------------------------------------------
  Read these with GetGlobalValue in any condition. They describe the player, and the framework
  updates them many times a second.

  FG03_LeftHandSpellSchool     school of the spell in the left hand, 0 = no spell
  FG03_RightHandSpellSchool    school of the spell in the right hand, 0 = no spell
  FG03_CastingBothHands        1 while both hands are casting at the same time (not a dual cast)

  School values: 18 Alteration, 19 Conjuration, 20 Destruction, 21 Illusion, 22 Restoration.

  Example, "both hands hold Destruction spells":
      GetGlobalValue FG03_LeftHandSpellSchool  == 20   AND
      GetGlobalValue FG03_RightHandSpellSchool == 20

  The globals work in any condition anywhere, not only in framework entries.


------------------------------------------------------------------------------------------------
7. EXAMPLES
------------------------------------------------------------------------------------------------
  A. Ten Firebolts with one cast
       Mod Spell Magnitude, Add Value 9
       Perk Owner: HasKeyword GROUP__FG03_SpellProjectiles_Any == 1
       Spell:      EPMagic_SpellHasKeyword MagicDamageFire == 1
                   SpellHasCastingPerk <Apprentice Destruction perk> == 1
     The original plus 9 copies makes 10. The ini allows up to 10 extra by default.

  B. Left-hand Destruction spells fly 50% further
       Mod Spell Magnitude, Multiply Value 1.5
       Perk Owner: HasKeyword GROUP__FG03_SpellRange_Left == 1
       Spell:      EPMagic_SpellHasSkill Destruction == 1

  C. Spells charge 30% faster while both hands hold Destruction
       Mod Spell Magnitude, Multiply Value 0.7
       Perk Owner: HasKeyword GROUP__FG03_SpellCastTime_Any == 1
                   GetGlobalValue FG03_LeftHandSpellSchool  == 20
                   GetGlobalValue FG03_RightHandSpellSchool == 20

  D. Keep three runes down instead of one
       Mod Spell Magnitude, Add Value 2
       Perk Owner: HasKeyword GROUP__FG03_MaxRunes == 1

  E. Fire spell explosions 25% wider
       Mod Spell Magnitude, Multiply Value 1.25
       Perk Owner: HasKeyword GROUP__FG03_SpellRadius_Any == 1
       Spell:      EPMagic_SpellHasKeyword MagicDamageFire == 1

  One perk can hold framework entries and normal entries side by side. For example, a rank perk
  can have a normal "fire damage x1.25" entry and a tagged "+2 projectiles" entry, and each
  does its own job.


------------------------------------------------------------------------------------------------
8. RULES THAT KEEP YOUR PERKS SAFE
------------------------------------------------------------------------------------------------
  Normal magnitude entries (vanilla, Requiem, your own untagged ones) are never counted by the
  framework, and tagged entries never change real magnitude. Four mistakes break that:

  1. The tag is not the FIRST condition on the Perk Owner tab.
     PEPE then treats the entry as a normal one, and also makes the GROUP__ check pass. The
     entry becomes a real magnitude bonus: "Add Value 9" would become +9 to the spell's
     strength. This is the dangerous one.

  2. The tag is in the perk's own Conditions box (on the perk record, not inside an entry).
     PEPE also checks a perk's first two conditions. A GROUP__ keyword there turns EVERY entry
     in that perk into a framework entry, so its normal magnitude entries stop working.

  3. A perk's EditorID contains "GROUP__".
     PEPE has an older mode that treats such perks as category perks. Never put GROUP__ in a
     perk's EditorID.

  4. A GROUP__FG03_ keyword is used in some other condition (on a spell, magic effect, etc.).
     PEPE makes every GROUP__ keyword check pass, so that condition will always be true. Only
     use these keywords as the entry tag.

  Quick check: open each entry's Perk Owner tab and look at row 1. If it is a GROUP__FG03_
  keyword, the entry is a framework value. If it is anything else, the entry changes real
  magnitude.


------------------------------------------------------------------------------------------------
9. HOW EACH MECHANIC BEHAVES
------------------------------------------------------------------------------------------------
  RANGE
    Multiplies the projectile's maximum range when it is launched.

  EXTRA PROJECTILES
    - Copies are launched on the original's first frame, from its position and aim. Each copy
      fans further out, alternating left and right by fExtraProjectileSpreadDegrees: with 9
      copies at 6 degrees the volley covers +/-30 degrees.
    - Copies cost no extra magicka and never spawn copies of their own.
    - Projectiles restored from a save (runes already placed, bolts in flight) are ignored.
    - Concentration spells (Flames, beams, cones) are untested and may be copied on every tick
      of the stream. Limit extra-projectile entries to fire-and-forget spells with Spell-tab
      conditions.

  CAST TIME
    Scales the charge-up: the time between starting a cast and the spell being ready to
    release. It does not change the release animation or a concentration spell's upkeep.

  RADIUS
    Explosions do not record which spell made them. The framework matches each explosion to
    the player spell that last fired a projectile carrying that explosion. If two spells share
    one explosion, the perks of the one cast most recently apply. The _Left and _Right radius
    keywords are reserved for a later version and currently do nothing.

  RUNE LIMIT
    - A rune is any spell that throws a lobber projectile at a target location, or carries the
      vanilla MagicRune keyword. Runes added by other mods count too.
    - The limit is worked out from the rune spells in the player's hands (the higher one wins),
      and again whenever a rune is thrown. It starts from the game's own limit (iMaxPlayerRunes;
      1 in vanilla and Requiem).
    - Copies from extra-projectile entries count toward the limit.
    - The limit is not saved anywhere. It is worked out fresh every session from your perks.
    - The game's rune limit only exists for the player.

  NPCS
    By default only the player's spells are affected (bPlayerOnly=1). Setting it to 0 extends
    range, extra projectiles, cast time and radius to NPC spells. NPC mode is untested.


------------------------------------------------------------------------------------------------
10. SETTINGS (FG03_SpellMechanicsFramework.ini)
------------------------------------------------------------------------------------------------
  bPlayerOnly                     1    only the player's spells are affected
  bDebugLog                       0    1 = log every value applied (see section 11)
  fMinMult                        0.1  lowest multiplier any radius/range/cast-time result may be
  fMaxMult                        10   highest multiplier
  iMaxExtraProjectiles            10   most extra projectiles one cast can add
  fExtraProjectileSpreadDegrees   6    angle between neighbouring copies
  iMaxRunesCap                    20   highest rune limit a perk can give
  iRuneLimitMode                  1    0 = only change the game's rune setting
                                       1 = change it, and take over if the game ignores it
                                       2 = the framework always enforces the limit itself

  Restart the game after changing the ini.


------------------------------------------------------------------------------------------------
11. TESTING AND THE LOG
------------------------------------------------------------------------------------------------
  Set bDebugLog=1. The log is written to:
     Documents\My Games\Skyrim Special Edition\SKSE\FG03_SpellMechanicsFramework.log

  A healthy start shows:
     Connected to Perk Entry Point Extender (interface version 2).
     Data loaded: rune keyword found, globals left/right/both found/found/found ...
     Ticker is running.

  While playing, each applied value is logged with the spell's name and FormID, for example:
     Firebolt [00012FD0] range 10000 -> 20000 (x2)
     Launched 9 extra projectile(s) of Firebolt [00012FD0].
     Fireball [0001C789] explosion radius 330 -> 990 (x3)

  If your perk never shows up in the log:
    - Check that the tag is row 1 on the Perk Owner tab (section 8).
    - Check the spell's FormID. Many spells share a display name: Requiem, for instance, has
      separate "Firebolt" records for each hand, plus College, trap and creature copies, and
      the console command psb gives you all of them. A Spell-tab condition can pass for one
      "Firebolt" and fail for another.

  "PEPE rejected category ..." means the installed PEPE does not accept the request. Update
  Perk Entry Point Extender.


------------------------------------------------------------------------------------------------
12. COMPATIBILITY AND KNOWN LIMITATIONS
------------------------------------------------------------------------------------------------
  - Works with any spell or perk overhaul. The plugin only adds keywords and globals and
    overrides nothing.
  - Mods that set iMaxPlayerRunes in a plugin are fine: their value becomes the starting limit.
    Another SKSE plugin or script that changes it while the game runs will fight with this
    framework.
  - Other SKSE plugins that change projectile range, projectile count or charge time may stack
    or conflict with it.
  - Untested so far: SE 1.5.97, VR, game versions 1.7 and later, concentration spells with
    extra projectiles, the _Left / _Right categories, and NPC mode.


------------------------------------------------------------------------------------------------
13. FAQ
------------------------------------------------------------------------------------------------
  Why does my entry say "Mod Spell Magnitude" when it does something else?
    The Creation Kit cannot show new entry types, so the framework borrows that one as a
    container (section 3). The tag on row 1 is what decides the mechanic.

  Will my framework entries change a spell's damage?
    No, as long as the tag is row 1 on the Perk Owner tab (section 8, rule 1).

  Can I add my own mechanics or tags?
    No. The DLL only reads the keywords listed in section 5. A new mechanic needs a new version
    of the DLL.

  Can players use this without any perk mod?
    It installs fine but does nothing on its own.


------------------------------------------------------------------------------------------------
14. CREDITS
------------------------------------------------------------------------------------------------
  - NoahBoddie for Perk Entry Point Extender, which this framework is built on
  - The SKSE team
  - meh321 for Address Library for SKSE Plugins
  - The CommonLibSSE-NG authors and contributors
  - The authors of spdlog, MinHook, DirectXMath, DirectXTK and rapidcsv, which are compiled
    into the DLL (see the third-party notices)


------------------------------------------------------------------------------------------------
15. CHANGELOG
------------------------------------------------------------------------------------------------
  1.0.0  First release.


------------------------------------------------------------------------------------------------
16. LICENSE AND SOURCE CODE
------------------------------------------------------------------------------------------------
  This mod is free software under the GNU General Public License, version 3 or later
  (GPL-3.0-or-later). The DLL is built on CommonLibSSE-NG, whose license requires this.

    FG03_SpellMechanicsFramework - LICENSE.txt               the full license text
    FG03_SpellMechanicsFramework - Third-party notices.txt   notices for the libraries compiled
                                                             into the DLL

  Source code, with the build files and the exact library versions used:
    <your GitHub repository URL>

  You may use, change and share this mod, including the DLL, under the terms of the GPL. The
  plugin (.esp), the ini and this readme are released under the same license.

  The code and this readme were written with AI assistance (Claude, by Anthropic).
