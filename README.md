# hellforge-rebalanced
Rebalance mod for Hellforge. Increases high-end difficulty, improves gameplay 'feel' and weapon handling, buffs underperforming equipment, and reworks equipment without an effective role in the arsenal.

**Current version: 1.0.7** · Made for game build 25480438, the build HellForge runs.

[Full patch notes](docs/index.html) · [Releases](../../releases)

## What it does

- **Increased difficulty at the high end.** Higher difficulties get more combat pressure to make up for the equipment power creep of recent years. D7–D10 get bigger encounter and guard budgets and more frequent, more simultaneous patrols. The original D10 settings now sit between D8 and D9, and D10 becomes a bridge to the HellForge D11 missions. Only enemy density and patrol density are touched, through settings the F8 menu exposes.
- **Snappier handling.** Game-wide changes to handling, ergonomics, sway, recoil, reload speed and ammo economy make weapons feel more responsive.
- **A narrower power gap.** The gap between the most meta equipment and the weakest is closed mostly by raising the underperformers. A few systems that weren't worth equipping, such as Stun Grenades, the Re-Educator and Shields, get light reworks.
- **More build variety.** More equipment can fill the utility role of plugging bug holes and interacting with mission objectives such as the Solo Silo. For example, the Plasma weapons, the Bolt Pistol, the Loyalist's charged shot and the Re-Educator can now destroy bug holes, bot fabricators and warp ships.
- **Limited, necessary nerfs.** A few systems warp the game's power balance too aggressively, and the Supply Bag is the main one: it now holds 2 supplies instead of 4. Ammo for the Ultimatum, Breacher, Missile Pistol, Grenade Pistol and stims was rebuilt around that change, so the totals stay similar.

## What's covered

Every change is listed old → new in the [patch notes](docs/index.html), with a Balance Intent explaining the goal for most weapons and categories.

- **Primary weapons:** Assault Rifles, Marksman Rifles, Submachine Guns, Shotguns, Energy Weapons and Explosive Weapons
- **Secondary weapons:** Pistols and Special
- **Throwables:** grenades and mines, with the aim that each has a niche where it performs well
- **Support weapons:** Ballistic, Explosive/Launcher, Chemical/Fire, Energy and Expendable weapons
- **Stratagems:** Orbitals, Airstrikes, Sentries and Backpacks
- **Attachments:** muzzle devices, underbarrel attachments and optics
- **Armor passives and boosters**
- **Mission difficulty:** D7–D10 encounter and patrol settings
- **Enemies:** Terminids, Automatons and Illuminate
- **Mod-added content:** this mod also adjusts weapons and stratagems from other creators' mods so they sit within the same power level: Beelzebub's Stratagem Pack (0.4 release), Skyeshade's Stratagem Compendium and the Scope of Authority Backport. More will follow as they are playtested, and the official 0.5 release is expected to improve how well these can be balanced.

A recommended partner mod is Helldivers Survival Rework, which pushes the handling changes further.

## Install

1. Close HellForge.
2. Download `hellforge-rebalanced-1.0.7.zip` from [Releases](../../releases) and copy the mod folder from it into `<your HellForge folder>\mods\`, replacing any older copy of this mod.
3. Clear F8's Local Stats. Changes you apply in the HellForge Editor are saved as Local Stats and always win over mods, so while they are present you will see a stats-conflict notice on the changed fields. The mod folder's own `README.md` explains how.
4. Start HellForge and press **PLAY**.

If you don't like one of my changes, you can change it back in the Editor. Save your preset before clearing it.

## Feedback

Balance changes come from 600+ hours of gameplay testing over about two months. The system is not perfect and feedback is welcome, especially on grenades. Please be specific: use enemy break points and equipment comparisons to justify recommendations, and be open to discussion.

## Repo layout

- `titan-stun/` is the mod folder (manifest, stats files and C source).
- `docs/` is the patch-notes site (enable GitHub Pages with source `main`, folder `/docs`).

## License

MIT, see `LICENSE`. `titan-stun/src/cJSON.*` is under its own license (`titan-stun/cJSON-LICENSE.txt`).
