/* Mod content values: the changes to weapons and stratagems other mods add (Beelzebub's Strat Pack, Scope of Authority),
 * from mod-content-stats.json in this mod's folder, applied in game the way HellForge 0.3.0-beta's F8 editor applies
 * its own mod-stats.json (native/editor/app.cpp AddStratagemPage / ApplyModValues, catalog.cpp AddCopy):
 *
 *   "mod:<stratagem identity>": a mod stratagem. Its page is a copy of the page of the stratagem it is like
 *     (HfContent.editor_like), each field pointed at the stratagem's own records: its settings row, the entities it
 *     shows (editor_entities), the projectiles and damage those entities fire where they differ from the page's, and the
 *     projectiles and damage its mod copied once from the page's. A field of an entity record is kept only where the
 *     stratagem's entity has one HellForge can read.
 *   "mod:giblett.scope-of-authority": Scope of Authority's R-40 Sightliner and P-44 Mist pages, which that mod adds to
 *     F8 itself: the weapons' own records, and the round and damage each weapon fires.
 *
 * The pages are in mod_pages.h (generated from F8's catalogue and Scope of Authority's pages). Each value is a live edit
 * (set_live): HellForge keeps it for the session and renews it when the game reloads its data. F8's own saved values for
 * the same fields, if any, sit above these (an editor's live edit outranks a code mod's). Nothing is written to any
 * file. Every value that cannot be applied is named in this mod's log with the reason. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "hellforge_mod.h"
#include "cJSON.h"
#include "mod_pages.h"

#define VALUES_FILE "mod-content-stats.json"
#define LAYOUT 25480438u
#define RETRY_SECONDS 1.0f
#define GIVE_UP_SECONDS 90.0f
#define MAX_MAP 64
#define MAX_CONTENT 4096

static const HfApi* g_hf;
static HfMod* g_mod;
static int g_state;              /* 0 waiting for the game's data, 1 trying, 2 done */
static int g_entitiesReady, g_projectilesReady;
static float g_wait, g_waited;
static cJSON* g_values;

#define LOG(...) g_hf->log(g_mod, __VA_ARGS__)

/* ---- reading the values file ------------------------------------------------------------------------------------ */

static cJSON* ReadValues(void)
{
    const char* folder = g_hf->mod_folder(g_mod);
    char path[MAX_PATH * 3];
    snprintf(path, sizeof path, "%s\\%s", folder ? folder : ".", VALUES_FILE);
    wchar_t wide[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, MAX_PATH * 2)) return NULL;
    HANDLE f = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { LOG("mod content: %s not found; no mod content values", VALUES_FILE); return NULL; }
    LARGE_INTEGER size;
    char* text = NULL;
    DWORD got = 0;
    if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (16ll << 20) && (text = (char*)malloc((size_t)size.QuadPart + 1)))
    {
        if (!ReadFile(f, text, (DWORD)size.QuadPart, &got, NULL) || got != (DWORD)size.QuadPart) { free(text); text = NULL; }
        else text[got] = 0;
    }
    CloseHandle(f);
    if (!text) { LOG("mod content: %s could not be read", VALUES_FILE); return NULL; }
    const char* body = text;
    if ((unsigned char)body[0] == 0xEF && (unsigned char)body[1] == 0xBB && (unsigned char)body[2] == 0xBF) body += 3;
    cJSON* root = cJSON_Parse(body);
    if (!root || !cJSON_IsObject(root))
    {
        const char* at = cJSON_GetErrorPtr();
        LOG("mod content: %s is not valid JSON (near: %.40s); no mod content values", VALUES_FILE, at ? at : "?");
        cJSON_Delete(root);
        root = NULL;
    }
    free(text);
    return root;
}

/* ---- live reads ------------------------------------------------------------------------------------------------- */

static int ReadU32(const char* component, uint64_t target, uint16_t offset, uint64_t* out)
{
    HfField field = {component, LAYOUT, offset, 4, HF_TYPE_U32, 0};
    HfLiveValue value;
    if (!g_hf->get_live(g_mod, target, &field, &value)) return 0;
    *out = (uint64_t)value.now.number;
    return 1;
}

static int Readable(const PageRow* r, uint64_t ref)
{
    HfField field = {r->component, LAYOUT, r->offset, (uint16_t)(r->type == HF_TYPE_U8 ? 1 : 4), r->type, 0};
    HfLiveValue value;
    return g_hf->get_live(g_mod, ref, &field, &value) != 0;
}

/* ---- F8's remap: where each field of the page points for a mod stratagem ---------------------------------------- */

typedef struct { uint64_t from, to; } Pair;
typedef struct { Pair stratagem[MAX_MAP], entity[MAX_MAP], projectile[MAX_MAP], damage[MAX_MAP]; int ns, ne, np, nd; } Remap;

static int Find(const Pair* m, int n, uint64_t from, uint64_t* to)
{
    for (int i = 0; i < n; i++) if (m[i].from == from) { *to = m[i].to; return 1; }
    return 0;
}

static void Put(Pair* m, int* n, uint64_t from, uint64_t to, int replace)
{
    for (int i = 0; i < *n; i++) if (m[i].from == from) { if (replace) m[i].to = to; return; }
    if (*n < MAX_MAP) m[(*n)++] = (Pair){from, to};
}

static HfContent g_content[MAX_CONTENT];
static int g_contentCount;

static void ReadContent(void)
{
    g_contentCount = 0;
    if (g_hf->size < offsetof(HfApi, content) + sizeof(void*) || !g_hf->content) return;
    for (uint32_t i = 0; i < MAX_CONTENT; i++)
    {
        HfContent c;
        memset(&c, 0, sizeof c);
        c.size = sizeof c;
        if (!g_hf->content(g_mod, i, &c)) break;
        g_content[g_contentCount++] = c;
    }
}

static int SameMod(const HfContent* a, const HfContent* b) { return a->mod && b->mod && !strcmp(a->mod, b->mod); }

static void BuildRemap(const HfContent* s, Remap* r)
{
    memset(r, 0, sizeof *r);
    Put(r->stratagem, &r->ns, s->editor_like, s->id, 1);
    for (const HfEditorEntity* e = s->editor_entities; e && e->entity; e++) Put(r->entity, &r->ne, e->like, e->entity, 1);
    /* what each entity standing in fires, against what the page's entity fires, and the damage of each */
    for (int i = 0; i < r->ne; i++)
    {
        uint64_t a, b;
        if (ReadU32("ProjectileWeaponComponentData", r->entity[i].from, 0, &a) && ReadU32("ProjectileWeaponComponentData", r->entity[i].to, 0, &b) &&
            a != b && b != 0) Put(r->projectile, &r->np, a, b, 1);
    }
    for (int i = 0; i < r->np; i++)
    {
        uint64_t da, db;
        if (ReadU32("ProjectileSettings", r->projectile[i].from, 0x3C, &da) && ReadU32("ProjectileSettings", r->projectile[i].to, 0x3C, &db) &&
            da != db && db != 0) Put(r->damage, &r->nd, da, db, 1);
    }
    /* else the projectiles and damage its mod copied from the page's, one copy of each base */
    for (int i = 0; i < g_contentCount; i++)
    {
        const HfContent* c = &g_content[i];
        if (!SameMod(c, s) || (c->kind != HF_KIND_PROJECTILE && c->kind != HF_KIND_DAMAGE)) continue;
        int copies = 0;
        for (int j = 0; j < g_contentCount; j++) if (SameMod(&g_content[j], s) && g_content[j].kind == c->kind && g_content[j].base == c->base) copies++;
        if (copies != 1) continue;
        if (c->kind == HF_KIND_PROJECTILE) Put(r->projectile, &r->np, c->base, c->id, 0);
        else Put(r->damage, &r->nd, c->base, c->id, 0);
    }
}

/* where a row of the page points in the copy; 0 when it has nothing to point at */
static int Pointed(const PageRow* row, const Remap* r, uint64_t* ref)
{
    if (!strcmp(row->component, "StratagemDefinition")) return Find(r->stratagem, r->ns, row->ref, ref);
    if (!strcmp(row->component, "ProjectileSettings")) return Find(r->projectile, r->np, row->ref, ref);
    if (!strcmp(row->component, "DamageInfo")) return Find(r->damage, r->nd, row->ref, ref);
    if (strstr(row->component, "ComponentData")) return Find(r->entity, r->ne, row->ref, ref);
    return 0;
}

/* ---- applying -------------------------------------------------------------------------------------------------- */

typedef struct { int applied, refused, missing, waiting; } Tally;

/* ---- direct writes: the workaround for HellForge 0.3.0-beta ----------------------------------------------------------
 * 0.3.0-beta refuses live edits (F8's and mods') of a mod weapon's own_all records unless its mod named that component in
 * the weapon's definition (fixed on HellForge's dev branch, cc17276). Beelzebub's ARC-11, B/TX-80 and GL-14 are own_all
 * copies, so their handling, rounds and reload refuse. For those components this writes the copy's own record itself,
 * found the way HellForge's registry finds it (the table the game reads, its hashed index rows), and only when the copy's
 * index row names a record other than its base's (so the vanilla weapon is never touched). Checked every second, so a
 * reload of the entity data (HellForge makes the copies again) is written again. Once HellForge takes the edit itself,
 * set_live succeeds and nothing is queued here. */
#define STATE_GLOBAL 0x346BF98u /* game.dll: the entity state pointer */
#define TABLES 0xF12478u        /* state: the component table pointers */
typedef struct { const char* name; uint32_t index, slots, stride; } DirectComponent;
static const DirectComponent kDirect[] = { /* component_sizes.h / registry.c kSlots, build 25480438 */
    {"WeaponDataComponentData", 236, 730, 1232},
    {"WeaponReloadComponentData", 113, 498, 80},
    {"WeaponRoundsComponentData", 117, 50, 136},
};
typedef struct { const DirectComponent* c; uint64_t entity, base; uint16_t offset; uint8_t type; uint32_t bytes; char what[96]; int written; } Direct;
#define MAX_DIRECT 64
static Direct g_direct[MAX_DIRECT];
static int g_directCount, g_directLogged;
static float g_directWait;
static uintptr_t g_game;

static int Peek(uintptr_t at, void* out, size_t size) { return at != 0 && g_hf->read(at, out, size) == 0; }

/* The record table c's index row for `entity` names (in the table the game reads now), or 0; *record its number. */
static uintptr_t RecordAt(const DirectComponent* c, uint64_t entity, uint32_t* record)
{
    uintptr_t state = 0, table = 0;
    if (!g_game) g_game = g_hf->module_base("game.dll", NULL);
    if (!Peek(g_game + STATE_GLOBAL, &state, 8) || !Peek(state + TABLES + 8u * c->index, &table, 8) || !table) return 0;
    uint32_t at = (uint32_t)(entity % c->slots);
    for (uint32_t i = 0; i < c->slots; i++)
    {
        uint8_t row[16];
        if (!Peek(table + (uintptr_t)at * 16u, row, 16)) return 0;
        uint64_t key;
        memcpy(&key, row, 8);
        if (key == entity) { memcpy(record, row + 8, 4); return table + (uintptr_t)c->slots * 16u + (uintptr_t)*record * c->stride; }
        if (key == 0) return 0;
        at = at + 1 == c->slots ? 0 : at + 1;
    }
    return 0;
}

/* Queues a direct write for a refused edit of a mod entity's record in one of kDirect's components: 1 when queued. */
static int QueueDirect(const PageRow* row, uint64_t ref, double value, const char* where, const char* key)
{
    if (ref == row->ref || g_directCount >= MAX_DIRECT) return 0; /* only a mod's copy, never the page's own entity */
    for (size_t i = 0; i < sizeof kDirect / sizeof kDirect[0]; i++)
    {
        if (strcmp(kDirect[i].name, row->component)) continue;
        if (row->type != HF_TYPE_F32 && row->type != HF_TYPE_U32 && row->type != HF_TYPE_I32) return 0;
        if (row->offset + 4u > kDirect[i].stride) return 0;
        Direct* d = &g_direct[g_directCount++];
        memset(d, 0, sizeof *d);
        d->c = &kDirect[i]; d->entity = ref; d->base = row->ref; d->offset = row->offset; d->type = row->type;
        if (row->type == HF_TYPE_F32) { float f = (float)value; memcpy(&d->bytes, &f, 4); }
        else if (row->type == HF_TYPE_I32) { int32_t s = (int32_t)value; memcpy(&d->bytes, &s, 4); }
        else d->bytes = (uint32_t)value;
        snprintf(d->what, sizeof d->what, "%s %s = %g", where, key, value);
        return 1;
    }
    return 0;
}

/* Writes each queued value where the record does not hold it yet. */
static void DirectTick(void)
{
    int wrote = 0, unsafe = 0, missing = 0;
    for (int i = 0; i < g_directCount; i++)
    {
        Direct* d = &g_direct[i];
        uint32_t mine = 0, theirs = 0, now = 0;
        uintptr_t record = RecordAt(d->c, d->entity, &mine);
        if (!record) { missing++; continue; }
        if (!RecordAt(d->c, d->base, &theirs) || theirs == mine) { unsafe++; continue; } /* shared with the vanilla weapon */
        if (!Peek(record + d->offset, &now, 4)) { missing++; continue; }
        if (now == d->bytes) continue;
        if (g_hf->write(record + d->offset, &d->bytes, 4) != 0) { missing++; continue; }
        if (!d->written) LOG("mod content: %s written directly (HellForge 0.3.0-beta refuses live edits of this record)", d->what);
        d->written++;
        wrote++;
    }
    if (!g_directLogged && (unsafe || missing))
    {
        LOG("mod content: %d direct write(s) waiting: %d record(s) not found yet, %d shared with the vanilla weapon (left alone)", unsafe + missing,
            missing, unsafe);
        g_directLogged = 1;
    }
    (void)wrote;
}

static int Write(const PageRow* row, uint64_t ref, double value, const char* where, const char* key)
{
    HfField field = {row->component, LAYOUT, row->offset, (uint16_t)(row->type == HF_TYPE_U8 ? 1 : 4), row->type, 0};
    HfValue v = {row->type, row->unlimited && value == -1 ? 4294967295.0 : value, 0};
    if (g_hf->set_live(g_mod, ref, &field, &v, row->shared ? HF_LIVE_SHARED : 0)) return 1;
    if (QueueDirect(row, ref, value, where, key)) return 1;
    LOG("mod content: %s %s = %g refused (%s)", where, key, value, g_hf->last_log(g_mod));
    return 0;
}

/* A mod stratagem's values; *waiting when something it needs is not readable yet. */
static void ApplyStratagem(const char* name, const cJSON* values, int final, Tally* t)
{
    const HfContent* s = NULL;
    for (int i = 0; i < g_contentCount && !s; i++) if (g_content[i].kind == HF_KIND_STRATAGEM && g_content[i].name && !strcmp(g_content[i].name, name)) s = &g_content[i];
    int count = cJSON_GetArraySize(values);
    /* the content list is complete once every mod's init has run, so a stratagem missing now is not coming */
    if (!s) { if (final) { LOG("mod content: %s is not added by any mod this session; its %d value(s) skipped", name, count); t->missing += count; } return; }
    const Page* page = NULL;
    for (size_t i = 0; i < sizeof kPages / sizeof kPages[0] && !page; i++) if (!kPages[i].category && kPages[i].likeType == s->editor_like) page = &kPages[i];
    if (!page)
    {
        if (final) { LOG("mod content: %s takes the page of stratagem type %llu, which this mod has no table for; its %d value(s) skipped", name,
            (unsigned long long)s->editor_like, count); t->missing += count; }
        return;
    }
    Remap remap;
    BuildRemap(s, &remap);
    const cJSON* v;
    cJSON_ArrayForEach(v, values)
    {
        if (!cJSON_IsNumber(v)) { if (final) { LOG("mod content: %s %s is not a number; skipped", name, v->string); t->missing++; } continue; }
        /* the page's last row of that key that points at something of the stratagem's own, as F8's copy keeps it */
        const PageRow* use = NULL;
        uint64_t ref = 0;
        int unreadable = 0;
        for (uint32_t i = 0; i < page->count; i++)
        {
            const PageRow* row = &page->rows[i];
            uint64_t to;
            if (strcmp(row->key, v->string) || !Pointed(row, &remap, &to)) continue;
            if (strstr(row->component, "ComponentData") && !Readable(row, to)) { unreadable = 1; continue; }
            use = row; ref = to;
        }
        if (!use)
        {
            if (!final && unreadable) { t->waiting++; continue; }
            if (final) { LOG("mod content: %s %s is not on its page (%s's), so F8 has no field for it either; skipped", name, v->string, page->name); t->missing++; }
            continue;
        }
        if (!strstr(use->component, "ComponentData") && !Readable(use, ref) && !final) { t->waiting++; continue; }
        if (!final) continue;
        if (Write(use, ref, v->valuedouble, name, v->string)) t->applied++; else t->refused++;
    }
}

/* Scope of Authority's R-40 Sightliner or P-44 Mist page. */
static void ApplyWeapon(const char* category, const char* target, const cJSON* values, int final, Tally* t)
{
    const Page* page = NULL;
    for (size_t i = 0; i < sizeof kPages / sizeof kPages[0] && !page; i++)
        if (kPages[i].category && !strcmp(kPages[i].category, category) && !strcmp(kPages[i].target, target)) page = &kPages[i];
    int count = cJSON_GetArraySize(values);
    if (!page) { if (final) { LOG("mod content: %s %s is not a page this mod knows; its %d value(s) skipped", category, target, count); t->missing += count; } return; }
    const cJSON* v;
    cJSON_ArrayForEach(v, values)
    {
        const PageRow* use = NULL;
        for (uint32_t i = 0; i < page->count; i++) if (!strcmp(page->rows[i].key, v->string)) use = &page->rows[i];
        if (!use || !cJSON_IsNumber(v)) { if (final) { LOG("mod content: %s %s has no field %s; skipped", category, target, v->string); t->missing++; } continue; }
        uint64_t ref = use->ref, round = 0, damage = 0;
        if (use->refKind >= 1)
        {
            if (!ReadU32("ProjectileWeaponComponentData", use->ref, 0, &round) || !round) { if (!final) t->waiting++; else { LOG("mod content: %s's round is not readable; %s skipped", target, v->string); t->missing++; } continue; }
            ref = round;
        }
        if (use->refKind == 2)
        {
            if (!ReadU32("ProjectileSettings", round, 0x3C, &damage) || !damage) { if (!final) t->waiting++; else { LOG("mod content: %s's damage is not readable; %s skipped", target, v->string); t->missing++; } continue; }
            ref = damage;
        }
        if (!Readable(use, ref)) { if (!final) t->waiting++; else { LOG("mod content: %s %s is not readable (is Scope of Authority on?); skipped", target, v->string); t->missing++; } continue; }
        if (!final) continue;
        if (Write(use, ref, v->valuedouble, target, v->string)) t->applied++; else t->refused++;
    }
}

/* ---- AR-23L Liberator Legacy (alazynoob.ar23l-backport), optional --------------------------------------------------
 * That mod adds the weapon and writes its own wiki stats (660 rpm, no burst) as load-time edits. This mod has no editor page
 * for it, so its values are applied by hand as live edits on the weapon's entity (found by its resource name), which sit over
 * the other mod's load-time edits whatever the load order, and are renewed if the game reloads its data. Nothing here needs
 * the Legacy: when it is not installed the values are skipped with one log line.
 * Burst replaces the first fire mode (auto): writing it into the third slot added burst as a third option. */
#define LEGACY_CATEGORY "mod:alazynoob/ar23l_backport/assault_rifle_legacy"
#define LEGACY_UNIT "alazynoob/ar23l_backport/assault_rifle_legacy"
typedef struct { const char* key; const char* component; uint16_t offset; uint8_t type; } LegacyRow;
static const LegacyRow kLegacy[] = {
    {"fire_mode.modes.1", "WeaponDataComponentData", 0x90, HF_TYPE_U32}, /* fire_modes[0], the first mode (auto) becomes burst; the slot the Legacy mod zeroed (2) stays off */
    {"weapon.fire_rate", "ProjectileWeaponComponentData", 0x04 + 1 * 4, HF_TYPE_F32}, /* fire_rate[1], the default slot */
    {"weapon.sway", "WeaponDataComponentData", 0x68, HF_TYPE_F32},
    {"weapon.recoil_climb_horizontal", "WeaponDataComponentData", 0x1C, HF_TYPE_F32},
    {"weapon.recoil_climb_vertical", "WeaponDataComponentData", 0x20, HF_TYPE_F32},
    {"weapon.recoil_drift_horizontal", "WeaponDataComponentData", 0x00, HF_TYPE_F32},
    {"weapon.recoil_drift_vertical", "WeaponDataComponentData", 0x04, HF_TYPE_F32},
};

static void TextStart(void);
static void ApplyLegacy(const char* target, const cJSON* values, int final, Tally* t)
{
    int count = cJSON_GetArraySize(values);
    uint64_t entity = g_hf->resource_hash(LEGACY_UNIT);
    HfLiveValue probe;
    HfField modes = {"WeaponDataComponentData", LAYOUT, 0x90, 4, HF_TYPE_U32, 0};
    if (!entity || !g_hf->get_live(g_mod, entity, &modes, &probe))
    {
        /* never counted as waiting: a Legacy that is not installed must not hold up the other values */
        if (final) { LOG("mod content: AR-23L Liberator Legacy is not installed or not readable; its %d value(s) skipped", count); t->missing += count; }
        return;
    }
    if (!final) return;
    const cJSON* v;
    cJSON_ArrayForEach(v, values)
    {
        const LegacyRow* use = NULL;
        for (size_t i = 0; i < sizeof kLegacy / sizeof kLegacy[0]; i++) if (!strcmp(kLegacy[i].key, v->string)) use = &kLegacy[i];
        if (!use || !cJSON_IsNumber(v)) { LOG("mod content: %s has no field %s; skipped", target, v->string); t->missing++; continue; }
        HfField field = {use->component, LAYOUT, use->offset, 4, use->type, 0};
        double value = v->valuedouble;
        if (!strcmp(use->key, "fire_mode.modes.1")) /* the file's value is the fire mode (3 = burst) */
        { HfLiveValue before; if (g_hf->get_live(g_mod, entity, &field, &before)) LOG("mod content: Legacy first fire mode was %g, setting %g (burst replaces auto)", before.now.number, value); }
        HfValue hv = {use->type, value, 0};
        if (g_hf->set_live(g_mod, entity, &field, &hv, 0)) { t->applied++; LOG("mod content: Legacy %s = %g", use->key, value); }
        else { t->refused++; LOG("mod content: Legacy %s = %g refused (%s)", use->key, value, g_hf->last_log(g_mod)); }
    }
    TextStart();
}

/* ---- AR-23L description text (experimental) -------------------------------------------------------------------------
 * The backport adds the weapon with its own description, which HellForge's SDK cannot change from another mod. When the
 * Legacy is installed this sweeps (until it finds and replaces the text, then stops) the process's private writable memory, a slice per tick, for that exact text (UTF-8 and
 * UTF-16) and writes ours over it in place, padded with spaces to the same length. Images (code and constants, including
 * the backport's and this mod's own copies) are never touched. Nothing is written if the text is not found. */
#define LEGACY_OLD_TEXT "The Liberty Day 2186 rifle, styled after the original Liberator, with its own magazines and scope: auto and semi, 660 rpm. Every squad member needs this mod and the updated live game."
#define LEGACY_NEW_TEXT "The Liberty Day 2186 rifle, styled after the original Liberator with its own magazines, scope, and historically accurate ammo-conserving burst fire mode."
#define TEXT_SWEEPS 2
#define TEXT_SLICE (4u << 20)
#define TEXT_GAP_SECONDS 15.0f
#define TEXT_CHUNK (1u << 20)
static char g_oldU8[256], g_newU8[256];
static wchar_t g_oldU16[256], g_newU16[256];
static size_t g_lenU8, g_lenU16;
static unsigned char g_chunk[TEXT_CHUNK + 1024];
static int g_textState, g_textSweeps, g_textReplaced, g_textRegions;
static uintptr_t g_scanAddr;
static float g_textWait;

static void TextStart(void)
{
    if (g_textState) return;
    snprintf(g_oldU8, sizeof g_oldU8, "%s", LEGACY_OLD_TEXT);
    snprintf(g_newU8, sizeof g_newU8, "%s", LEGACY_NEW_TEXT);
    g_lenU8 = strlen(g_oldU8);
    int a = MultiByteToWideChar(CP_UTF8, 0, LEGACY_OLD_TEXT, -1, g_oldU16, 256) - 1;
    int b = MultiByteToWideChar(CP_UTF8, 0, LEGACY_NEW_TEXT, -1, g_newU16, 256) - 1;
    if (a <= 0 || b <= 0 || (size_t)b > (size_t)a || strlen(g_newU8) > g_lenU8) { LOG("mod content: Legacy description text not used (new text too long)"); g_textState = 3; return; }
    g_lenU16 = (size_t)a;
    for (size_t i = (size_t)b; i < g_lenU16; i++) g_newU16[i] = L' ';
    for (size_t i = strlen(g_newU8); i < g_lenU8; i++) g_newU8[i] = ' ';
    g_newU8[g_lenU8] = 0;
    g_textState = 1; g_scanAddr = 0x10000; g_textSweeps = 0; g_textWait = 0.0f;
    LOG("mod content: Legacy description text: sweeping memory for the backport's text (experimental)");
}

static void TextPatch(uintptr_t at, const void* data, size_t len, const char* what)
{
    SIZE_T wrote = 0;
    if (WriteProcessMemory(GetCurrentProcess(), (LPVOID)at, data, len, &wrote) && wrote == len) { g_textReplaced++; g_textState = 2; LOG("mod content: Legacy description text replaced (%s copy at %p); sweep stopped", what, (void*)at); }
    else LOG("mod content: Legacy description text found at %p but not writable", (void*)at);
}

/* The Legacy's description in one chunk of memory. */
static void TextScanChunk(uintptr_t base, size_t got)
{
    for (size_t i = 0; i + g_lenU8 <= got; i++)
    {
        unsigned char* p = (unsigned char*)memchr(g_chunk + i, 'T', got - g_lenU8 - i + 1);
        if (!p) break;
        i = (size_t)(p - g_chunk);
        if (!memcmp(p, g_oldU8, g_lenU8)) TextPatch(base + i, g_newU8, g_lenU8, "UTF-8");
    }
    size_t bytes16 = g_lenU16 * 2;
    for (size_t i = (((base + 1) & ~(uintptr_t)1) - base); i + bytes16 <= got; i += 2)
    {
        if (g_chunk[i] != 'T' || g_chunk[i + 1] != 0) continue;
        if (!memcmp(g_chunk + i, g_oldU16, bytes16)) TextPatch(base + i, g_newU16, bytes16, "UTF-16");
    }
}

/* One slice of a sweep: at most TEXT_SLICE bytes of private, writable, committed memory. */
static void TextSlice(void)
{
    size_t budget = TEXT_SLICE;
    size_t overlap = (g_lenU16 * 2 > g_lenU8 ? g_lenU16 * 2 : g_lenU8);
    while (budget)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (g_scanAddr >= (uintptr_t)0x7FFFFFFE0000ull || !VirtualQuery((LPCVOID)g_scanAddr, &mbi, sizeof mbi))
        {
            g_textSweeps++;
            LOG("mod content: Legacy description text sweep %d done: %d region(s) read, %d replacement(s) so far", g_textSweeps, g_textRegions, g_textReplaced);
            g_textRegions = 0;
            g_scanAddr = 0x10000;
            g_textWait = TEXT_GAP_SECONDS;
            if (g_textSweeps >= TEXT_SWEEPS) g_textState = 2;
            return;
        }
        uintptr_t start = (uintptr_t)mbi.BaseAddress, end = start + mbi.RegionSize;
        int ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                 (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));
        if (!ok) { g_scanAddr = end; continue; }
        if (g_scanAddr < start) g_scanAddr = start;
        size_t want = end - g_scanAddr;
        if (want > TEXT_CHUNK) want = TEXT_CHUNK;
        if (want > budget) want = budget;
        SIZE_T got = 0;
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)g_scanAddr, g_chunk, want, &got) && got >= g_lenU8) { TextScanChunk(g_scanAddr, got); if (g_scanAddr == start) g_textRegions++; }
        if (g_textState != 1) return; /* replaced: the sweep is over */
        budget = budget > want ? budget - want : 0;
        /* step back so a text that crosses a chunk edge is still seen, unless the region ends here */
        g_scanAddr += want;
        if (g_scanAddr < end && want > overlap) g_scanAddr -= overlap;
    }
}

static void TextTick(float dt)
{
    if (g_textState != 1) return;
    if (g_textWait > 0.0f) { g_textWait -= dt; return; }
    TextSlice();
}

/* Fields copied from one record to another in the running game, so the copy always matches the source as the game (and
 * HellForge) has it. The G-109 Urchin's detonation damage takes the G-23 Stun's Status 1; its Status 2 is in
 * hellforge-stats.json (your G-23 values), since HellForge may still be applying the G-23's there when this runs. */
typedef struct { const char* what; const char* component; uint64_t from, to; uint16_t offset; uint8_t type; } Copy;
static const Copy kCopies[] = {
    {"G-23 Stun -> G-109 Urchin explosion.damage.status_1_type", "DamageInfo", 279, 280, 44, HF_TYPE_U32},
    {"G-23 Stun -> G-109 Urchin explosion.damage.status_1_strength", "DamageInfo", 279, 280, 48, HF_TYPE_F32},
};

static void ApplyCopies(int final, Tally* t)
{
    for (size_t i = 0; i < sizeof kCopies / sizeof kCopies[0]; i++)
    {
        const Copy* c = &kCopies[i];
        HfField field = {c->component, LAYOUT, c->offset, 4, c->type, 0};
        HfLiveValue from;
        if (!g_hf->get_live(g_mod, c->from, &field, &from))
        {
            if (!final) t->waiting++;
            else { LOG("mod content: %s: the source is not readable; skipped", c->what); t->missing++; }
            continue;
        }
        if (!final) continue;
        HfValue v = {c->type, from.now.number, 0};
        if (g_hf->set_live(g_mod, c->to, &field, &v, 0)) { t->applied++; LOG("mod content: %s = %g", c->what, from.now.number); }
        else { t->refused++; LOG("mod content: %s = %g refused (%s)", c->what, from.now.number, g_hf->last_log(g_mod)); }
    }
}

/* One pass: final 0 only checks that everything is readable (counting what is not); final 1 writes and logs. */
static Tally Pass(int final)
{
    Tally t = {0, 0, 0, 0};
    ApplyCopies(final, &t);
    const cJSON* category;
    cJSON_ArrayForEach(category, g_values)
    {
        if (!category->string || strncmp(category->string, "mod:", 4) || !cJSON_IsObject(category)) continue;
        const cJSON* target;
        cJSON_ArrayForEach(target, category)
        {
            if (!cJSON_IsObject(target)) continue;
            if (!strcmp(category->string, "mod:giblett.scope-of-authority")) ApplyWeapon(category->string, target->string, target, final, &t);
            else if (!strcmp(category->string, LEGACY_CATEGORY)) ApplyLegacy(target->string, target, final, &t);
            else ApplyStratagem(category->string + 4, target, final, &t);
        }
    }
    return t;
}

static void Attempt(void)
{
    ReadContent();
    Tally check = Pass(0);
    if (check.waiting && g_waited < GIVE_UP_SECONDS) return; /* not all readable yet: again in a second */
    Tally t = Pass(1);
    LOG("mod content: %d value(s) applied (%d of them by direct write), %d refused by HellForge, %d skipped (see above)%s", t.applied,
        g_directCount, t.refused, t.missing, check.waiting ? "; some were still not readable after waiting" : "");
    g_state = 2;
    if (g_directCount) DirectTick();
}

void ModContentTick(float dt)
{
    TextTick(dt);
    if (g_state == 2 && g_directCount)
    {
        g_directWait -= dt;
        if (g_directWait <= 0.0f) { g_directWait = RETRY_SECONDS; DirectTick(); }
        return;
    }
    if (g_state != 1) return;
    g_waited += dt;
    g_wait -= dt;
    if (g_wait > 0.0f) return;
    g_wait = RETRY_SECONDS;
    Attempt();
}

static void Ready(void)
{
    if (g_state == 0 && g_entitiesReady && g_projectilesReady && g_values) { g_state = 1; g_wait = 0.0f; g_waited = 0.0f; }
}
static void EntitiesReady(HfMod* mod, HfEvent event, void* user) { g_entitiesReady = 1; Ready(); }
static void ProjectilesReady(HfMod* mod, HfEvent event, void* user) { g_projectilesReady = 1; Ready(); }

void ModContentInit(const HfApi* hf, HfMod* mod)
{
    g_hf = hf; g_mod = mod;
    if (hf->version < 7 || hf->size < offsetof(HfApi, content) + sizeof(void*)) { LOG("mod content: needs HellForge's mod SDK 7; skipped"); return; }
    g_values = ReadValues();
    if (!g_values) return;
    hf->on(mod, HF_EVENT_ENTITIES_READY, EntitiesReady, NULL);
    hf->on(mod, HF_EVENT_PROJECTILES_READY, ProjectilesReady, NULL);
}
