-- SPDX-License-Identifier: GPL-3.0-or-later
-- FG03 Spell Mechanics Framework: perk-driven spell radius, projectile range, cast time,
-- extra projectiles and rune limit. Values are authored as normal perk entries and read
-- through Perk Entry Point Extender categories.
set_xmakever("2.8.2")

includes("lib/commonlibsse-ng")

set_project("FG03_SpellMechanicsFramework")
set_version("1.0.0")
set_license("GPL-3.0-or-later")

set_languages("c++23")
set_warnings("allextra")
set_policy("package.requires_lock", false)

add_rules("mode.debug", "mode.releasedbg")

target("FG03_SpellMechanicsFramework")
    add_rules("commonlibsse-ng.plugin", {
        name = "FG03_SpellMechanicsFramework",
        author = "AKULAKHAN",
        description = "Spell Mechanics Framework: perk categories for spell radius, range, cast time, extra projectiles and rune limit"
    })

    -- Record only the .pdb file name in the DLL, not its full path.
    add_shflags("/PDBALTPATH:%_PDB%", { force = true })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
