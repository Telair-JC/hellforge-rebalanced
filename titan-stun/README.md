# Hellforge Rebalanced (HellForge mod)

This mod does four things:

1. **Applies your base-game rebalance.** `hellforge-stats.json` holds the changes to the game's own weapons,
   stratagems, throwables, enemies and so on. HellForge applies it every time the game loads.
2. **Makes stun-immune enemies stunnable again.** The Bile Titan, the Spore Burst Bile Titan and the Impaler get a Stun
   entry with the Charger's stun thresholds.
3. **Raises the G-6 Frag grenade's shrapnel** from 35 to 400, and **gives the G-109 Urchin the G-23 Stun's statuses**.
   Status 2 (type and strength) is in `hellforge-stats.json` with your G-23 values. Status 1 is copied by the code from
   the G-23 in the running game each session, so it always matches the G-23 (log line:
   `mod content: G-23 Stun -> G-109 Urchin explosion.damage.status_1_type = ...`).
4. **Applies your changes to mod-added weapons and stratagems** (Giblett's R-40 Sightliner, P-44 Mist, Apex, Nova,
   Incinerator, Pylon Launcher, Sanitiser and the orbital barrages, and the Stratagem Compendium's Orbital Gas and EMS
   Barrages). `mod-content-stats.json` holds them; the mod's code applies
   them in game, the same way F8 does, so there's no extra file to copy anywhere.

Made for game build 25480438, the build HellForge runs.

## Install

1. Close HellForge. In `<your HellForge folder>\mods\`, replace the old `titan-stun` folder with this one.
2. **Clear F8's Local Stats** (see the next section). These changes now live in the mod.
3. Start HellForge and press **PLAY**.

## Important: F8's Local Stats override this mod

Changes you APPLY in F8 are saved as "Local Stats" (`mods\zz.local.stats\hellforge-stats.json`), and Local Stats
always wins over mods. This mod's `hellforge-stats.json` is a copy of your Local Stats, so while the old copy is still
there, every field shows a "stats conflict" notice and the Local Stats value wins.

To clear it:

1. Close HellForge.
2. Back up `mods\zz.local.stats\hellforge-stats.json`.
3. Delete the `zz.local.stats` folder. HellForge makes a new one the next time you APPLY something in F8.

Don't use F8's **RESET TO DEFAULTS** for this. It would also reset your mod-added weapons and stratagems (see below).

## Changing the base stats

There are two ways to change them:

- **Edit the file directly.** Open `hellforge-stats.json` in a text editor. The layout is category, then item, then
  field, then value.
- **Use F8.** Make your changes in F8, APPLY, then copy `mods\zz.local.stats\hellforge-stats.json` over this mod's
  `hellforge-stats.json`. Delete `zz.local.stats` again afterwards so it doesn't override the mod.

Changes apply from the next PLAY. HellForge's stats pass applies each one at the right moment while the game loads,
and again whenever the game reloads its data:

- **Enemy and item stats:** right after the game's entity loader.
- **Projectile, damage and stratagem settings:** as soon as the game installs those tables.
- **Explosions, arcs and status effects:** as soon as HellForge finds those tables.

If HellForge ever refuses the mod because the stats file names a field it doesn't know (after a HellForge update,
say), the Mods page and the mod's log say which entry. Fix or remove that entry.

## Mod-added weapons and stratagems

HellForge's stats pass can't change weapons and stratagems that other mods add, so this mod's code does it instead.
It reads `mod-content-stats.json` (same layout as F8's `mods\hellforge-editor\mod-stats.json`: category
`mod:<mod id>`, then item, then field, then value) and applies each value as a live edit once the game has loaded,
exactly as F8 would.

- **Needs** Giblett's Beelzebub pack, Scope of Authority and SkyeShade's Stratagem Compendium installed and enabled. Items whose mod isn't loaded are
  skipped and named in the log.
- **AR-23L Liberator Legacy (optional):** AR-Liberator-Legacy backport by alazynoob. Its fire rate, sway, recoil and
  and burst (it replaces the first fire mode, auto) are written as live edits on top of that mod's own stats, so they win whatever the load order
  (keeping Rebalanced above it is still tidy). If the Legacy isn't installed, the values are skipped with one log line.
  Check `mod-local.titan-stun.log` for
  `Legacy first fire mode` and `Legacy ... =` lines.
- **Your own `mod-stats.json`:** you no longer need it for these. You can leave it (F8 applies the same values on top,
  which is harmless) or back it up and remove the `mod:` entries. If you keep editing these items in F8, F8's values
  win for that session; copy them into `mod-content-stats.json` to make them part of the mod.
- **To change a value:** edit `mod-content-stats.json` in a text editor. Changes apply from the next PLAY.

Some of these match the vanilla weapon each one is copied from, using your values from `hellforge-stats.json`:

- **ARC-11 Pylon Launcher:** the S-11 Speargun's ergonomics, sway, spread and recoil.
- **B/TX-80 Sanitiser:** the B/FLAM-80 Cremator's ergonomics.
- **GL-14 Incinerator:** the GL-52 De-Escalator's ergonomics, sway, spare, starting and supply rounds, and reload duration.

These are fixed numbers, not links: if you change the vanilla weapon later, change the copy in
`mod-content-stats.json` too.

**HellForge 0.3.0-beta workaround.** That version refuses edits (F8's too) to parts of a mod weapon that its mod
didn't name when it made the copy: the Pylon Launcher's and Sanitiser's handling (ergonomics, sway, spread, recoil) and
the Incinerator's rounds and reload. For those, this mod writes the weapon's own record directly, checks it every second
(so a data reload is written again), and never writes a record the copy still shares with the vanilla weapon. HellForge's
dev branch has fixed this (commit cc17276); once a release has it, the edits go through normally and the workaround
does nothing. F8 still can't edit those fields until then.

The log line to look for:

```
mod content: 83 value(s) applied (15 of them by direct write), 0 refused by HellForge, 0 skipped
```

## The stun and Frag changes

These are made by the code in `src/titan_stun.c`. Its log is `mod-local.titan-stun.log`, under the **CONSOLE** page's
**Open session logs**. A good run includes:

```
entity loader hooked; the change is made each time the game loads its entity data
Bile Titan: Stun added in slot 12 (offset 0x08D4): min 10.00 max 15.00
Spore Burst Bile Titan: Stun added in slot 12 (offset 0x08D4): min 10.00 max 15.00
Impaler: Stun added in slot 13 (offset 0x0984): min 10.00 max 15.00
G-6 Frag: shrapnel count 35 -> 400 (game's own 35)
```

- **More units:** add them to `kTargets` near the top of `src/titan_stun.c`. For example, the Impaler's tentacles:
  `{0x672F7DA17F3BA34Aull, "Impaler Tentacle"},`.
- **A different shrapnel count:** change `#define FRAG_SHRAPNEL 400u`. `35u` is the game's own value. HellForge's
  reviewed range for this field is 0–64, so watch for frame drops at high values.

## Co-op

Everyone needs this exact mod folder; HellForge compares every file in it, including `mod-content-stats.json`, so
everyone gets the same mod-added weapon and stratagem values. Everyone also needs the same Giblett mods.
