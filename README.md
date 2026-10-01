# WetFunction NG

Standalone SKSE port of Wet Function Redux for Skyrim SE/AE, with native SexLab P+ and OStim
Standalone support (no OTracker/OSweat patch needed), and native SexLab Aroused NG / OSL Aroused /
Devious Devices NG integration. Mod page: https://dwemermods.com/mods/4453

This repository holds the plugin's source code. It does not include wet textures or a build of the
mod - get those from the mod page.

## Layout

- `src/` - the SKSE plugin (C++23, CommonLibSSE-NG).
- `papyrus-source/` - the three Papyrus scripts (global API/console commands, the SKSE Menu
  Framework page, and the player alias used for SexLab scene events).
- `tools/` - build and packaging scripts (`build_papyrus.ps1`, `build_esp.py`, `package.ps1`).
- `dist_template/` - the default `WetFunctionNG.ini` and the Custom Console command list shipped
  with the mod.
- `xmake.lua` - the build definition.

## Building

Requires [xmake](https://xmake.io), a copy of
[CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) at `lib/CommonLibSSE-NG`, and
[SKSE-Menu-Framework-3-API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API) at
`lib/SKSE-Menu-Framework-3-API`.

```
xmake build WetFunctionNG
```

Then `tools\build_papyrus.ps1` compiles the Papyrus scripts, `python tools\build_esp.py` generates
the plugin's ESP (an ESL with a quest that hosts the SKSE Menu Framework page), and
`tools\package.ps1` assembles a Data-folder layout (set `WFNG_DEPLOY_DIR` to also copy it into a
mod manager's mod folder).

## Architecture

- `Manager` - per-actor registry, a 250 ms timer thread that queues onto the main thread, the
  auto-apply scan, and the SKSE co-save.
- `Wetness` - the wetness formula (weather, activity, arousal, sex, fire) and the
  dry/sweat/soaked phase state machine.
- `Visuals` - pushes wet textures and specular/glossiness through RaceMenu's `IOverrideInterface`
  (skee64).
- `Settings` - the X-macro list of every setting in `src/Settings.h`, read from
  `Data/SKSE/Plugins/WetFunctionNG.ini`.
- `Menu` - the SKSE Menu Framework configuration page.
- `Widget` - the HUD wetness droplet, drawn with the menu framework's ImGui draw list.
- `OStim`, `Arousal`, `Devious`, `Heat` - optional integrations (OStim Standalone's native plugin
  API, SexLab Aroused NG / OSL Aroused / SexLab Aroused's faction rank, Devious Devices NG's
  native plugin API, and SunHelm Survival's heat-source lists), each a no-op when its mod is not
  installed.

## License

MIT - see `LICENSE`.
