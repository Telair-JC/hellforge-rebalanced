# hellforge-rebalanced
Rebalance mod for Hellforge. Increases high-end difficulty, improves gameplay 'feel' and weapon handling, buffs underperforming equipment, and reworks equipment without an effective role in the arsenal.

**Current version: 1.0.7**

## Download

Grab `hellforge-rebalanced-1.0.7.zip` from the [Releases](../../releases) page, or copy the `titan-stun` folder from this repo.

## Install

1. Close HellForge. In `<your HellForge folder>\mods\`, replace the old `titan-stun` folder with the one from the download.
2. Clear F8's Local Stats, since Local Stats overrides mod values. The folder's own `README.md` explains how.
3. Start HellForge and press **PLAY**.

Made for game build 25480438, the build HellForge runs.

## Patch notes

Every change is listed old → new, grouped by weapon and enemy, with a Balance Intent for most entries. The patch notes live in [`docs/index.html`](docs/index.html) (GitHub Pages: enable it under Settings → Pages, source `main`, folder `/docs`).

## Repo layout

- `titan-stun/` is the mod folder: manifest, stats files and C source.
- `docs/` is the patch-notes site.

## Credits

See the mod folder's `README.md` and the patch notes for credits.

## License

MIT, see `LICENSE`. `titan-stun/src/cJSON.*` is under its own license (`titan-stun/cJSON-LICENSE.txt`).
