# Spell Mechanics Framework

An SKSE plugin for Skyrim that gives perk authors new things a perk can do to spells:
explosion radius, projectile range, charge time, extra projectiles per cast and the rune limit.
Plus globals for which school of magic is in each hand. Every value is set in the Creation Kit
with ordinary perk entries, routed through
[Perk Entry Point Extender](https://github.com/NoahBoddie/perk-entry-point-extender) categories.
The framework adds no perks of its own.

- **Download and full documentation:** https://www.nexusmods.com/profile/FRIENDLYGAL/mods
- **How to author perks for it:** [`data/FG03_SpellMechanicsFramework - Readme.txt`](data/FG03_SpellMechanicsFramework%20-%20Readme.txt)

## Repository layout

| Path | What it is |
|---|---|
| `src/` | The plugin's C++ source |
| `src/PEPE.h` | This project's own declaration of Perk Entry Point Extender's public interface |
| `lib/commonlibsse-ng/` | [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR) as a git submodule, pinned to the version the release was built with |
| `data/` | The rest of the mod as released: the plugin (`.esp`), the ini and the readme |
| `xmake.lua`, `build.bat` | Build scripts |
| `LICENSE` | GPL-3.0 |
| `THIRD-PARTY-NOTICES.txt` | Licenses of the libraries compiled into the DLL |

## Building

You need:
- Windows 10 or 11, 64-bit
- Visual Studio 2022 or the Build Tools for Visual Studio 2022, with the **Desktop development with C++** workload (MSVC v143 and a Windows SDK)
- [xmake](https://xmake.io) 2.8.2 or later (built with 3.1.2)
- git

Clone with the submodules:

```
git clone --recursive <this repository's URL>
```

For an existing clone without them, run `git submodule update --init --recursive`.

Then run `build.bat`. It finds Visual Studio through `vswhere`. If yours isn't found, set
`VSDEVCMD` to the full path of your `VsDevCmd.bat`. If xmake isn't on your PATH, set
`XMAKE_DIR` to its folder. It always does a full rebuild, which takes a few minutes; on the
first build xmake also downloads CommonLibSSE-NG's own dependencies. It passes the compiler
`/d1trimfile` (through the `CL` variable), so the DLL records source paths relative to the
project folder rather than your full paths.

The DLL is written to `build\windows\x64\releasedbg\FG03_SpellMechanicsFramework.dll`.

To build by hand from a Visual Studio developer prompt:

```
xmake config -m releasedbg -y
xmake build -y
```

## Installing a build

Copy the DLL to `Data\SKSE\Plugins\`, and the contents of `data\` to `Data\`. At runtime the
plugin needs SKSE64, Address Library for SKSE Plugins and Perk Entry Point Extender.

## License

GPL-3.0-or-later; see [LICENSE](LICENSE). The plugin statically links CommonLibSSE-NG, whose
license requires plugins built with it to be GPL-licensed. Notices for every library compiled
into the DLL are in [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt). Perk Entry Point
Extender is not included; it is a separate runtime requirement.

Copyright (C) 2026 Roger Herrera

The code was written with AI assistance (Claude, by Anthropic).
