/* Titan Stun (local.titan-stun): a HellForge code mod for game build 25480438.
 *
 * Makes the Bile Titan, the Spore Burst Bile Titan and the Impaler stunnable: each gets a Stun entry in its status-effect
 * receiver (StatusEffectReceiverComponentData) with the Charger's stun thresholds. Their stun immunity is the absence of
 * that entry. Tested in game (0.5.0): Titans given the entry in their first empty slot are stunned by the G-23.
 *
 * Also sets the G-6 Frag grenade's shrapnel count to 400 (vanilla 35): ExplosionSettings row 257 ("G-6 Frag
 * (detonation)"), field shrapnel_count (+0x50). That one is a named HellForge field, so it goes through HellForge's own
 * live edit (set_live), retried once a second from HF_EVENT_PROJECTILES_READY until the row is reachable; HellForge
 * finds the row by its id and keeps the edit across reloads.
 * HellForge's and HD2Runtime's reviewed range for this field is 0..64; set_live checks only that the value fits a u32,
 * so 400 is allowed but untested. Lower FRAG_SHRAPNEL if it causes trouble.
 *
 * The change has to be made while the game loads its entity data, so the mod hooks the game's entity loader (game.dll +
 * 0xFDB440, the function HellForge's own stats pass hooks) and writes as soon as it returns: the same moment HellForge's
 * load-time stats edits are made. The hook goes in at HF_EVENT_GAME_LOADED (game.dll decoded, before its entity data
 * loads), and only if the function's first bytes are the ones HellForge pins for this build.
 *
 * HellForge has no named field for this component, so the mod reads and writes the game's memory itself:
 *   1. game.dll + 0x346BF98 holds the entity state; state + 0xF12478 is the game's table array, one pointer per component
 *      (the same two addresses HellForge's own loader uses for this build, native/mod-loader/registry.c).
 *   2. The table for index 307 must carry the header [307][0x6062C233]["LDLD"][1][0x6062C233][size][1][0] just before it.
 *      If 307 does not, every index is checked and exactly one must match.
 *   3. size = 16 * indices + 46600 * records; 648 index rows (HellForge's count for table 307). The index rows are
 *      surveyed into the log; the rows of the Charger and the units changed must each name a record below `records`, flags 0.
 *   4. A record: 8 zones of 5816 bytes; zone 0 holds 33 entries of 176 bytes from +8; an entry's type is at +140, its
 *      min and max thresholds at +144 and +148. An empty entry is type 0, min 0.0, max 1.0.
 *   5. The Charger must have a type-22 (Stun) entry; its min and max are what the units get. A unit's entries are packed
 *      from slot 0 and the game reads them up to the first empty one, so each unit's FIRST empty slot in zone 0 is filled
 *      (0.4.0 used the last one, slot 32, which the game never reached): min and max first, the type last. A unit that
 *      already has a type-22 entry, or whose entries are not packed, is left alone.
 * Every check that fails stops the mod for that load and says why in its log. Nothing is written to the game's files:
 * the change lives in memory for the session, and it is made again every time the game loads its entity data. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "hellforge_mod.h"

#define GAME_BUILD "25480438"
#define STATE_GLOBAL 0x346BF98u /* game.dll: the entity state pointer (registry.c REG_STATE_GLOBAL) */
#define TABLES 0xF12478u        /* the state's table array (registry.c REG_TABLES) */
#define TABLE_INDEX 307u
#define TABLE_TYPE 0x6062C233u  /* StatusEffectReceiverComponentData */
#define TABLE_ROWS 648u         /* registry.c kSlots {307, 648} */
#define RECORD_SIZE 46600u
#define SCAN_LIMIT 512u
#define ENTRIES 33u
#define ENTRY_SIZE 176u
#define ENTRY_TYPE 140u
#define STUN 22u                /* StatusEffectSusceptibilityType_Stun (checked against the Charger below) */
#define ENTITY_LOADER 0xFDB440u /* game.dll: the entity loader (registry.c REG_ENTITY_LOADER) */
#define LOADER_PIN "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57" /* its first bytes (registry.c kLoaderPin) */

#define CHARGER 0x1A7FCDFF98C664B0ull

#define FRAG_EXPLOSION 257u   /* ExplosionSettings row: G-6 Frag (detonation) */
#define FRAG_SHRAPNEL 400u    /* vanilla 35 */

/* The units given Stun, when they have no Stun entry of their own. The Impaler's tentacles are separate units, also
 * without Stun; add {0x672F7DA17F3BA34Aull, "Impaler Tentacle"} to stun them along with the body. */
static const struct { uint64_t resource; const char* name; } kTargets[] = {
    {0x9E2E17F2CCCCAFDDull, "Bile Titan"},
    {0xEF04CB84D097A497ull, "Spore Burst Bile Titan"},
    {0xDCF8E74212FBEE3Bull, "Impaler"},
};

static const HfApi* g_hf;
static HfMod* g_mod;
static uintptr_t g_game;
static uintptr_t g_doneTable; /* the table this load's work was done (or refused) for */
typedef void (*EntityLoaderFn)(void* state);
static EntityLoaderFn g_loader; /* the next hook in the loader's chain, or the game's own code */

#define LOG(...) g_hf->log(g_mod, __VA_ARGS__)

static int Read(uintptr_t address, void* out, size_t size) { return address != 0 && g_hf->read(address, out, size) == 0; }

static uintptr_t Pointer(uintptr_t address)
{
    uintptr_t value = 0;
    return Read(address, &value, sizeof value) ? value : 0;
}

typedef struct Table
{
    uintptr_t base;   /* the first index row */
    uint32_t index, rows, records;
} Table;

static uintptr_t TableAt(uintptr_t state, uint32_t index)
{
    return Pointer(state + TABLES + 8u * index);
}

static int HeaderMatches(uintptr_t table, uint32_t index, uint32_t* size)
{
    uint32_t w[8];
    if (!Read(table - 32, w, sizeof w)) return 0;
    if (w[0] != index || w[1] != TABLE_TYPE || w[2] != 0x444C444Cu || w[3] != 1 || w[4] != TABLE_TYPE || w[6] != 1 || w[7] != 0) return 0;
    *size = w[5];
    return 1;
}

/* The status-effect receiver table now, checked; 0 (logged once per table) when it does not check out. */
static int FindTable(uintptr_t state, Table* out, uintptr_t* seen, const char** why)
{
    uint32_t size = 0, index = TABLE_INDEX;
    uintptr_t table = TableAt(state, index);
    *seen = table;
    if (table == 0 || !HeaderMatches(table, index, &size))
    {
        int hits = 0;
        uintptr_t found = 0;
        uint32_t foundSize = 0, foundIndex = 0;
        for (uint32_t i = 0; i < SCAN_LIMIT; i++)
        {
            uintptr_t candidate = TableAt(state, i);
            uint32_t candidateSize;
            if (candidate != 0 && HeaderMatches(candidate, i, &candidateSize)) { hits++; found = candidate; foundSize = candidateSize; foundIndex = i; }
        }
        *seen = found;
        if (hits != 1) { *why = hits > 1 ? "several tables match, so none is used" : "not found"; return 0; }
        table = found; size = foundSize; index = foundIndex;
    }
    uint32_t rows = TABLE_ROWS;
    if (index != TABLE_INDEX || (uint64_t)rows * 16u >= size || (size - rows * 16u) % RECORD_SIZE != 0)
    {
        /* not HellForge's known table 307: the one solution with records <= rows <= 2 * records + 2 */
        int solutions = 0;
        for (uint32_t r = 1; (uint64_t)r * RECORD_SIZE <= size; r++)
        {
            uint32_t rest = size - r * RECORD_SIZE;
            if (rest % 16 == 0 && rest / 16 >= r && rest / 16 <= 2 * r + 2) { rows = rest / 16; solutions++; }
        }
        if (solutions != 1) { *why = "its size does not resolve to index rows and records"; return 0; }
    }
    uint32_t records = (size - rows * 16u) / RECORD_SIZE;
    uint8_t probe[16];
    if (!Read(table, probe, 16) || !Read(table + (uintptr_t)rows * 16u - 16u, probe, 16)) { *why = "its index rows are unreadable"; return 0; }
    out->base = table; out->index = index; out->rows = rows; out->records = records;
    return 1;
}

/* The index rows, once per table, in the log: how many are used, and the first few that are not a plain
 * (entity id, record below `records`, flags 0) row. v0.2.0 refused the whole table over these; now only the rows of the
 * units the mod reads must be plain. */
static void Survey(const Table* t)
{
    uint32_t used = 0, flagged = 0, beyond = 0, shown = 0;
    for (uint32_t row = 0; row < t->rows; row++)
    {
        uint8_t entry[16];
        uint64_t id;
        uint32_t record, flags;
        if (!Read(t->base + row * 16u, entry, 16)) { LOG("index row %u is unreadable", row); return; }
        memcpy(&id, entry, 8); memcpy(&record, entry + 8, 4); memcpy(&flags, entry + 12, 4);
        if (id == 0 && record == 0 && flags == 0) continue;
        used++;
        int odd = flags != 0 || record >= t->records || id == 0;
        if (flags != 0) flagged++;
        if (record >= t->records) beyond++;
        if (odd && shown < 8)
        {
            shown++;
            LOG("index row %u: id %08x%08x record %u flags 0x%08x", row, (uint32_t)(id >> 32), (uint32_t)id, record, flags);
        }
    }
    LOG("index rows: %u used of %u, %u with flags set, %u naming a record past %u", used, t->rows, flagged, beyond, t->records);
}

/* The one record the entity's index row names, or 0. */
static uintptr_t RecordOf(const Table* t, uint64_t resource)
{
    int owners = 0;
    uint32_t selected = 0;
    for (uint32_t row = 0; row < t->rows; row++)
    {
        uint8_t entry[16];
        uint64_t id;
        uint32_t record, flags;
        if (!Read(t->base + row * 16u, entry, 16)) return 0;
        memcpy(&id, entry, 8); memcpy(&record, entry + 8, 4); memcpy(&flags, entry + 12, 4);
        if (id == resource)
        {
            if (flags || record >= t->records) { LOG("index row %u for %08x%08x: record %u flags 0x%08x, not a plain row", row, (uint32_t)(resource >> 32), (uint32_t)resource, record, flags); return 0; }
            selected = record; owners++;
        }
    }
    return owners == 1 ? t->base + (uintptr_t)t->rows * 16u + (uintptr_t)selected * RECORD_SIZE : 0;
}

typedef struct Entry
{
    uint32_t type;
    float min, max;
} Entry;

static int EntryAt(uintptr_t record, uint32_t slot, Entry* e)
{
    uint8_t bytes[12];
    if (!Read(record + 8 + slot * ENTRY_SIZE + ENTRY_TYPE, bytes, 12)) return 0;
    memcpy(&e->type, bytes, 4); memcpy(&e->min, bytes + 4, 4); memcpy(&e->max, bytes + 8, 4);
    return 1;
}

static int Empty(const Entry* e) { return e->type == 0 && e->min == 0.0f && e->max == 1.0f; }

/* The unit's zone-0 entries in the log; *stunSlot its type-22 slot (or -1), *firstEmpty its first empty slot (or -1).
 * The entries are packed from slot 0 (the game reads them in order up to the first empty one), so a new entry goes in
 * the first empty slot; *packed says whether every slot after it is empty too. */
static int Describe(const char* name, uintptr_t record, int* stunSlot, int* firstEmpty, int* packed)
{
    *stunSlot = -1; *firstEmpty = -1; *packed = 1;
    int empty = 0;
    for (uint32_t slot = 0; slot < ENTRIES; slot++)
    {
        Entry e;
        if (!EntryAt(record, slot, &e)) { LOG("%s: entry %u is unreadable", name, slot); return 0; }
        if (Empty(&e)) { empty++; if (*firstEmpty < 0) *firstEmpty = (int)slot; continue; }
        if (*firstEmpty >= 0) *packed = 0;
        if (e.type == STUN && *stunSlot < 0) *stunSlot = (int)slot;
        LOG("%s: slot %2u type %u min %.2f max %.2f", name, slot, e.type, (double)e.min, (double)e.max);
    }
    LOG("%s: %d empty slots%s%s", name, empty, empty ? "" : " (none free)", *packed ? "" : "; entries after an empty slot (not packed)");
    return 1;
}

/* The address of the unit's Stun type after this (written now or already there), or 0. */
static uintptr_t GiveStun(const Table* t, uint64_t resource, const char* name, const Entry* stun)
{
    uintptr_t record = RecordOf(t, resource);
    if (record == 0) { LOG("%s: no single status-effect record; skipped", name); return 0; }
    int stunSlot, firstEmpty, packed;
    if (!Describe(name, record, &stunSlot, &firstEmpty, &packed)) return 0;
    if (stunSlot >= 0) { LOG("%s: already has a Stun entry in slot %d; left alone", name, stunSlot); return record + 8 + (uint32_t)stunSlot * ENTRY_SIZE + ENTRY_TYPE; }
    if (firstEmpty < 0) { LOG("%s: no empty slot; skipped", name); return 0; }
    if (!packed) { LOG("%s: its entries are not packed from slot 0, so where the game stops reading is unclear; skipped", name); return 0; }
    int lastEmpty = firstEmpty; /* the slot written: the first empty one, right after the unit's last entry */
    uintptr_t at = record + 8 + (uint32_t)lastEmpty * ENTRY_SIZE + ENTRY_TYPE;
    Entry check;
    float pair[2] = {stun->min, stun->max};
    uint32_t type = STUN;
    if (g_hf->write(at + 4, pair, sizeof pair) || g_hf->write(at, &type, sizeof type) || !EntryAt(record, (uint32_t)lastEmpty, &check) ||
        check.type != STUN || check.min != stun->min || check.max != stun->max)
    { LOG("%s: writing slot %d failed", name, lastEmpty); return 0; }
    LOG("%s: Stun added in slot %d (offset 0x%04X): min %.2f max %.2f", name, lastEmpty, 8 + (uint32_t)lastEmpty * ENTRY_SIZE + ENTRY_TYPE,
        (double)stun->min, (double)stun->max);
    return at;
}

static uintptr_t g_doneStun; /* the Bile Titan's Stun type field after the last apply, or 0 */

/* Whether this load is already handled: the same table, and (when the Titan was given Stun) the entry still there. */
static int Handled(uintptr_t seen)
{
    if (seen == 0 || seen != g_doneTable) return 0;
    uint32_t type = 0;
    return g_doneStun == 0 || (Read(g_doneStun, &type, 4) && type == STUN);
}

static void Apply(uintptr_t state)
{
    if (Handled(TableAt(state, TABLE_INDEX))) return;
    Table t;
    uintptr_t seen = 0;
    const char* why = "";
    int ok = FindTable(state, &t, &seen, &why);
    if (Handled(seen)) return;
    if (!ok)
    {
        if (seen != 0) { LOG("status-effect table: %s; nothing changed", why); g_doneTable = seen; g_doneStun = 0; return; }
        static uintptr_t missing; /* a table at 307 that is not this one (logged once), or none yet: the data has not loaded */
        uintptr_t at = TableAt(state, TABLE_INDEX);
        if (at != 0 && at != missing) { LOG("status-effect table: %s (index %u holds another table); nothing changed", why, TABLE_INDEX); missing = at; }
        return;
    }
    g_doneTable = t.base; g_doneStun = 0;
    if (t.index != TABLE_INDEX) LOG("status-effect table found at index %u instead of %u", t.index, TABLE_INDEX);
    LOG("status-effect table: index %u, %u index rows, %u records of %u bytes", t.index, t.rows, t.records, RECORD_SIZE);
    Survey(&t);
    uintptr_t charger = RecordOf(&t, CHARGER);
    int stunSlot, firstEmpty, packed;
    if (charger == 0 || !Describe("Charger", charger, &stunSlot, &firstEmpty, &packed)) { LOG("Charger: no single status-effect record; nothing changed"); return; }
    Entry stun;
    if (stunSlot < 0 || !EntryAt(charger, (uint32_t)stunSlot, &stun))
    { LOG("Charger: no type-%u entry, so %u may not be Stun on this build; nothing changed", STUN, STUN); return; }
    for (size_t i = 0; i < sizeof kTargets / sizeof kTargets[0]; i++)
    {
        uintptr_t written = GiveStun(&t, kTargets[i].resource, kTargets[i].name, &stun);
        if (i == 0) g_doneStun = written;
    }
}

/* The game's entity loader, hooked: the data loads, then the change, before the game goes on to use it. */
static void LoaderDetour(void* state)
{
    g_loader(state);
    uintptr_t entities = Pointer(g_game + STATE_GLOBAL);
    g_doneTable = 0; g_doneStun = 0; /* a new load: always look again */
    if (entities != 0) Apply(entities);
    else LOG("entity data loaded, but the entity state is not readable; nothing changed");
}

/* game.dll is decoded and its entity data has not loaded yet: hook the loader. */
static void GameLoaded(HfMod* mod, HfEvent event, void* user)
{
    g_game = g_hf->module_base("game.dll", NULL);
    if (g_game == 0) { LOG("game.dll is not loaded; nothing changed"); return; }
    if (!g_hf->code_matches(g_game + ENTITY_LOADER, LOADER_PIN))
    { LOG("the entity loader's code is not the reviewed build's; nothing hooked, nothing changed"); return; }
    if (g_hf->hook(mod, g_game + ENTITY_LOADER, (void*)LoaderDetour, (void**)&g_loader) != 0)
    { LOG("could not hook the entity loader; nothing changed"); return; }
    LOG("entity loader hooked; the change is made each time the game loads its entity data");
    /* if the data was already in (it should not be at this point), change it now and say so */
    uintptr_t entities = Pointer(g_game + STATE_GLOBAL);
    if (entities != 0 && TableAt(entities, TABLE_INDEX) != 0)
    {
        LOG("the entity data had already loaded before the hook; changing it now, which may be too late for this load");
        Apply(entities);
    }
}

/* The Frag grenade's shrapnel count, through HellForge's live edit. HellForge finds the explosion table a little after
 * HF_EVENT_PROJECTILES_READY (1.1.1 tried at the event and found the row unreadable), so from that event the mod tries
 * once a second on the game thread until the row reads, then makes the change once; HellForge keeps it for the session
 * and renews it when the game reloads its data. */
#define FRAG_RETRY_SECONDS 1.0f
#define FRAG_GIVE_UP_SECONDS 180.0f
static int g_fragState;     /* 0 waiting for the event, 1 trying, 2 done (or given up) */
static float g_fragWait, g_fragWaited;

static int TryFrag(HfMod* mod)
{
    HfField field = HF_FIELD(ExplosionSettings, shrapnel_count);
    HfLiveValue before;
    if (!g_hf->get_live(mod, FRAG_EXPLOSION, &field, &before)) return 0; /* not reachable yet */
    HfValue value = HF_U32(FRAG_SHRAPNEL);
    int applies = g_hf->set_live(mod, FRAG_EXPLOSION, &field, &value, 0);
    if (!applies)
    {
        LOG("G-6 Frag: not changed as its own row (%s); trying as a shared row", g_hf->last_log(mod));
        applies = g_hf->set_live(mod, FRAG_EXPLOSION, &field, &value, HF_LIVE_SHARED);
    }
    if (!applies) LOG("G-6 Frag: shrapnel count not changed (%s)", g_hf->last_log(mod));
    else LOG("G-6 Frag: shrapnel count %.0f -> %u (game's own %.0f)", before.now.number, FRAG_SHRAPNEL, before.base.number);
    return 1;
}

static void ProjectilesReady(HfMod* mod, HfEvent event, void* user)
{
    if (g_fragState == 0) { g_fragState = 1; g_fragWait = 0.0f; g_fragWaited = 0.0f; }
}

void ModContentInit(const HfApi* hf, HfMod* mod); /* mod_content.c */
void ModContentTick(float dt);

static void FragTick(HfMod* mod, float dt, void* user)
{
    ModContentTick(dt);
    if (g_fragState != 1) return;
    g_fragWaited += dt;
    g_fragWait -= dt;
    if (g_fragWait > 0.0f) return;
    g_fragWait = FRAG_RETRY_SECONDS;
    if (TryFrag(mod)) { g_fragState = 2; return; }
    if (g_fragWaited >= FRAG_GIVE_UP_SECONDS)
    { LOG("G-6 Frag: explosion row %u was still not reachable after %.0f s; shrapnel left as it is", FRAG_EXPLOSION, (double)g_fragWaited); g_fragState = 2; }
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* hf, HfMod* mod)
{
    g_hf = hf; g_mod = mod;
    if (hf->version < 13 || hf->size < offsetof(HfApi, code_matches) + sizeof hf->code_matches)
    { hf->log(mod, "needs HellForge's mod SDK 13 or later"); return 1; }
    if (strncmp(hf->build_id(), GAME_BUILD, strlen(GAME_BUILD)) != 0)
    { hf->log(mod, "made for game build %s, not %s: its addresses would be wrong, so it does nothing", GAME_BUILD, hf->build_id()); return 1; }
    if (hf->on(mod, HF_EVENT_GAME_LOADED, GameLoaded, NULL) != 0) { hf->log(mod, "could not wait for the game to load"); return 1; }
    if (hf->on(mod, HF_EVENT_PROJECTILES_READY, ProjectilesReady, NULL) != 0 || !hf->on_game_thread(mod, FragTick, NULL))
        hf->log(mod, "could not wait for the explosion table; the Frag stays vanilla");
    ModContentInit(hf, mod);
    hf->log(mod, "waiting for game.dll");
    return 0;
}
