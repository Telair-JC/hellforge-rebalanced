# hellforge-rebalanced
Rebalance mod for Hellforge. Increases high-end difficulty, improves gameplay 'feel' and weapon handling, buffs underperforming equipment, and reworks equipment without an effective role in the arsenal.

**Current version: 1.0.7** · Made for game build 25480438, the build HellForge runs.

[Full patch notes](https://telair-jc.github.io/hellforge-rebalanced/) · [Releases](../../releases)

## What it does

- **Increased difficulty at the high end.** Higher difficulties get more combat pressure to make up for the equipment power creep of recent years. D7–D10 get bigger encounter and guard budgets and more frequent, more simultaneous patrols. The original D10 settings now sit between D8 and D9, and D10 becomes a bridge to the HellForge D11 missions. Only enemy density and patrol density are touched, through settings the F8 menu exposes.
- **Snappier handling.** Game-wide changes to handling, ergonomics, sway, recoil, reload speed and ammo economy make weapons feel more responsive.
- **A narrower power gap.** The gap between the most meta equipment and the weakest is closed mostly by raising the underperformers. A few systems that weren't worth equipping, such as Stun Grenades, the Re-Educator and Shields, get light reworks.
- **More build variety.** More equipment can fill the utility role of plugging bug holes and interacting with mission objectives such as the Solo Silo. For example, the Plasma weapons, the Bolt Pistol, the Loyalist's charged shot and the Re-Educator can now destroy bug holes, bot fabricators and warp ships.
- **Limited, necessary nerfs.** A few systems warp the game's power balance too aggressively, and the Supply Bag is the main one: it now holds 2 supplies instead of 4. Ammo for the Ultimatum, Breacher, Missile Pistol, Grenade Pistol and stims was rebuilt around that change, so the totals stay similar.

## What's Changed?

[Almost everything - link to full changelog](https://telair-jc.github.io/hellforge-rebalanced/)

## Partner Mods

### Required

These are required for the intended gameplay experience:

- **[Weakpoint Lock-On](https://github.com/Telair-JC/weakpoint-lockon)**: Balance of tracking weapons, guard dogs, sentries, and arc weapons is all tested with this mod in mind. Without this mod, the Spear is likely unsalvageable. Without it, gameplay experience won't work as-intended for any weapons which track, and arc weapons in particular.
- **[Smarter Guard Dogs & Sentries](https://hf-mod.com/Mods/Details/80)**: Balance of guard dogs and sentries was tuned with this mod in mind. Without it, gameplay experience isn't going to work quite as-intended, especially for some guard dogs.

### Recommended

These are highly recommended pairings for the gameplay experience:

- **[Helldiver Health, Ragdoll & Cover Rework](https://hf-mod.com/Mods/Details/14)**: Significantly reduces stim tax, noticeably increases Helldiver survivability, and reduces total reinforcements. It's a different feel for the game, but in my opinion it's an improvement, especially if playing with increased enemy levels.

### Optional

These are mods where I am actively trying to maintain equipment balance for:

- **[Scope of Authority Backport](https://hf-mod.com/Mods/Details/52)**: Backport for Scope of Authority warbond, currently a bit buggy in 0.4.0 release but will be improved with 0.5.0. Yes, the Sightliner one-shots Hulk eyes (and pays for it with reduced ammunition).
- **[Beelzebub's Stratagem Pack](https://hf-mod.com/Mods/Details/4)**: Stratagem pack by Beelzebub, adds a number of interesting tools to the arsenal. Balance of many of these was inconsistent with the system I'd been working on, and have been adjusted accordingly. This does include some nerfs. More adjustments likely after 0.5.0 release.
- **[Custom Stratagem Compendium](https://hf-mod.com/Mods/Details/1)**: Similar to above, a number of interesting new arsenal tools. Rebalancing of these is ongoing, and will include both buffs and nerfs. More adjustments likely after 0.5.0 release.
- **[Pick your Bots/Bugs/Squids](https://hf-mod.com/Mods/Details/17)**: This balance pack is balancing around the top ~20% of equipment in the game, and let's face it... the game just isn't that difficult with a competent team and gear that powerful, even at the increased threat levels of this mod. This lets you make it genuinely hard. Personally, if I'm playing with at least 2 people, I recommend making breaches more scary by reducing the reinforcement cost of units in the breach, but up to you to experiment. In the future I may make presets to recommend, but that's a ways down the road.
- **[Armored Overhaul](https://hf-mod.com/Mods/Details/15)**: This mod is very cool, and very fun, but if you use this mod you should almost certainly limit all versions of the tank stratagems to a single use per game. If I build a modpack in the future, this will be in it and tanks will be single use.

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
