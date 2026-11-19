# Writes apworld/animal_crossing/museum_data.py: every museum thing (fossils, paintings, bugs, fish) in
# museum index order, with when bugs and fish can be caught (for logic).
# Also writes pc/src/pc_ap_critter_data.c + pc/include/pc_ap_critter_data.h for the Bugs/Fish pause
# pages: the same slots per month and half (fish tables split days 1-15 / 16-end; bugs have no
# halves) and a weather hint from the spawn areas.
# Names: apworld game_items.py (ROM item names, as written: lowercase but proper nouns), by item number:
#   fossils   FTR_DIN_TRIKERA_HEAD..FTR_DIN_TRILOBITE (include/m_ftr_def.h), index = furniture number
#   paintings FTR_SUM_ART01..FTR_SUM_ART15
#   bugs      ITM_INSECT_START + aINS_INSECT_TYPE_*, fish ITM_FISH_START + aGYO_TYPE_*
# Spawn times: town tables only (island, fishing tourney ignored).
#   bugs: l_insect_month[month][term] (src/actor/ac_set_ovl_insect.c), 6 terms of hours
#   fish: r_month/s_month/p_month[month][half] + time_no (src/actor/ac_set_ovl_gyoei.c), time_no = our 4 slots
# A species counts in a slot if it spawns there for >= 2 hours of that month (user, 2026-10-06).
# Special spawns (not in the tables) are set by hand, see SPECIAL.
# Fish passes (Fish Availability, 2026-10-15): the pass items of each fish, from its spawn areas (town tables,
# not the tourney/island lists) and its shadow size (gyoei_type[], src/actor/ac_gyoei_type.c_inc).
# Bug passes (Bug Availability, 2026-10-15): one pass item per bug, by where it lives, set by hand (BUG_PASS_OF_TYPE;
# the cicada/beetle split of the tree bugs isn't in the game's data).
# usage: python3 dump_museum.py [--check]   (--check: exit 1 if a file is out of date, write nothing)
import ast, os, re, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
OUT = os.path.join(REPO, "apworld", "animal_crossing", "museum_data.py")
OUT_C = os.path.join(REPO, "pc", "src", "pc_ap_critter_data.c")
OUT_H = os.path.join(REPO, "pc", "include", "pc_ap_critter_data.h")

SLOTS = ["Morning", "Day", "Evening", "Night"]  # timesanity.TIME_SLOTS order
SLOT_HOURS = [range(4, 9), range(9, 16), range(16, 21), [21, 22, 23, 0, 1, 2, 3]]
# aSOI_TERM0..5 (include/ac_set_ovl_insect.h)
BUG_TERM_HOURS = [[23, 0, 1, 2, 3], range(4, 8), range(8, 16), [16], [17, 18], range(19, 23)]
MIN_HOURS = 2

ALL_MONTHS = list(range(12))
ALL_SLOTS = list(range(4))
# name: (months, slots), for critters the spawn tables don't describe
SPECIAL = {
    # bee trees are placed daily, shaking one works any time (m_all_grow_ovl.c mAGrw_SetHoneycombTree)
    ("bug", "BEE"): (ALL_MONTHS, ALL_SLOTS),
    # on candy or a spoiled turnip; turnips need Joan (Sunday 6-11): Morning or Day
    ("bug", "ANT"): (ALL_MONTHS, [0, 1]),
    # sea, rain, not 9-15 (aSOG_add_kaseki_range_data). Rain: never in January or December
    # (mEnv_RandomWeather); the port makes the 13th of Feb-Nov rain (pc_ap_weather.h)
    ("fish", "COELACANTH"): (list(range(1, 11)), [0, 2, 3]),
}

# Weather from spawn areas (aSOI_chk_live_area_data, ac_set_ovl_insect.c): every area of a species
# rain-only -> RAIN_ONLY, every area not in rain -> NO_RAIN, else ANY
RAIN_AREAS = {"RAINING_ON_FLOWER"}
NO_RAIN_AREAS = {"ON_FLOWER", "FLYING_NEAR_FLOWERS_OR_AROUND", "ON_CANDY", "ON_TRASH"}
SPECIAL_WEATHER = {
    ("bug", "FIREFLY"): "NO_RAIN",  # deletes itself in rain (ac_ins_hotaru.c)
    ("fish", "COELACANTH"): "RAIN_ONLY",  # aSOG_add_kaseki_range_data
}


# Fish pass items (index = apworld museum.FISH_PASS_ITEMS, client PC_AP_FISH_PASS_*): river by shadow size,
# the river lake ("POOL") and ponds together, the sea. Waterfall and river mouth count as river.
FISH_PASS_NAMES = ["Small River Fish", "Medium River Fish", "Large River Fish", "Pond Fish", "Sea Fish"]
RIVER_PASS_OF_SIZE = {"XXS": 0, "XS": 1, "S": 1, "M": 2, "L": 2, "XL": 2, "XXL": 2}
AREA_PASS = {"POOL": 3, "POND": 3, "SEA": 4}
RIVER_AREAS = {"RIVER", "RIVER_MOUTH", "WATERFALL"}
# slot_data area names (the client maps aSOG_SPAWN_AREA_* to them; "" in a table entry = every area)
AREA_NAME = {"RIVER": "river", "WATERFALL": "waterfall", "RIVER_MOUTH": "river_mouth", "POOL": "lake", "POND": "pond",
             "SEA": "sea"}
SPECIAL_FISH_AREAS = {"COELACANTH": {"SEA"}}  # aSOG_add_kaseki_range_data, outside the tables

# Bug pass items (index = apworld museum.BUG_PASS_ITEMS, client PC_AP_BUG_PASS_*), by aINS_INSECT_TYPE_*.
# The Bee comes from bee trees, not the spawn tables.
BUG_PASS_NAMES = ["Tree Bugs", "Beetles", "Flower Bugs", "Ground Bugs", "Flying Bugs"]
_BUG_TYPES_OF_PASS = [
    ["ROBUST_CICADA", "WALKER_CICADA", "EVENING_CICADA", "BROWN_CICADA", "BAGWORM", "SPIDER", "PURPLE_BUTTERFLY",
     "BEE"],
    ["DRONE_BEETLE", "DYNASTID_BEETLE", "FLAT_STAG_BEETLE", "JEWEL_BEETLE", "LONGHORN_BEETLE", "SAW_STAG_BEETLE",
     "MOUNTAIN_BEETLE", "GIANT_BEETLE"],
    ["COMMON_BUTTERFLY", "YELLOW_BUTTERFLY", "TIGER_BUTTERFLY", "LADYBUG", "SPOTTED_LADYBUG", "MANTIS", "SNAIL"],
    ["LONG_LOCUST", "MIGRATORY_LOCUST", "CRICKET", "GRASSHOPPER", "BELL_CRICKET", "PINE_CRICKET", "MOLE_CRICKET",
     "PILL_BUG", "ANT", "COCKROACH"],
    ["COMMON_DRAGONFLY", "RED_DRAGONFLY", "DARNER_DRAGONFLY", "BANDED_DRAGONFLY", "MOSQUITO", "FIREFLY",
     "POND_SKATER"],
]
BUG_PASS_OF_TYPE = {t: p for p, types in enumerate(_BUG_TYPES_OF_PASS) for t in types}


def read(path):
    return open(os.path.join(REPO, path)).read()


def enum(path, prefix, stop):
    body = read(path)
    names = []
    for m in re.finditer(r"^\s*" + prefix + r"(\w+),", body, re.M):
        if m.group(1) == stop:
            break
        names.append(m.group(1))
    return names


def item_names():
    src = read("apworld/animal_crossing/game_items.py")
    body = re.search(r"ITEM_NAME_TO_ID = (\{.*?\n\})", src, re.S).group(1)
    names = ast.literal_eval(body)
    by_id = {}
    for name, iid in names.items():
        by_id.setdefault(iid, re.sub(r" \((furniture|0x[0-9a-f]+)\)$", "", name))
    return by_id


def spawn_entries(src, macro):
    out = {}
    for m in re.finditer(r"static \w+ (\w+)\[[^\]]*\] = \{(.*?)\};", src, re.S):
        rows = re.findall(macro + r"\((\w+), (\w+), ([\d.]+)f?\)", m.group(2))
        if rows:
            out[m.group(1)] = [(t, float(w)) for t, a, w in rows]
    return out


def bug_weather(types):
    areas = {t: set() for t in types}
    for t, a in re.findall(r"INSECT_SPAWN\((\w+), (\w+), [\d.]+f?\)", read("src/actor/ac_set_ovl_insect.c")):
        if t in areas:
            areas[t].add(a)
    out = {}
    for t in types:
        if ("bug", t) in SPECIAL_WEATHER:
            out[t] = SPECIAL_WEATHER[("bug", t)]
        elif areas[t] and areas[t] <= RAIN_AREAS:
            out[t] = "RAIN_ONLY"
        elif areas[t] and areas[t] <= NO_RAIN_AREAS:
            out[t] = "NO_RAIN"
        else:
            out[t] = "ANY"
    return out


def term_lists(src, kind):
    # static <kind> NAME[...] = { { count, array }, ... };
    out = {}
    for m in re.finditer(r"static " + kind + r" (\w+)\[[^\]]*\] = \{(.*?)\};", src, re.S):
        out[m.group(1)] = [(int(n), a) for n, a in re.findall(r"\{\s*(\d+),\s*(\w+)\s*\}", m.group(2))]
    return out


def species(entries, count):
    return {t for t, w in entries[:count] if w > 0}


def slots_from_hours(hours):
    return [s for s in range(4) if len(hours & set(SLOT_HOURS[s])) >= MIN_HOURS]


def bug_times(types):
    src = read("src/actor/ac_set_ovl_insect.c")
    entries = spawn_entries(src, "INSECT_SPAWN")
    body = re.search(r"l_insect_month\[[^\]]*\]\[[^\]]*\] = \{(.*?)\n\};", src, re.S).group(1)
    pairs = re.findall(r"\{\s*(\d+),\s*(\w+)\s*\}", body)
    months = [pairs[i:i + 6] for i in range(0, len(pairs), 6)]
    assert len(months) == 12 and all(len(m) == 6 for m in months), [len(m) for m in months]
    hours = {t: [set() for _ in range(12)] for t in types}
    for mo, terms in enumerate(months):
        for term, (count, name) in enumerate(terms):
            for t in species(entries[name], int(count)):
                if t in hours:
                    hours[t][mo] |= set(BUG_TERM_HOURS[term])
    # no halves: the same hours in both
    return {t: [[h, h] for h in months_] for t, months_ in hours.items()}


def fish_times(types):
    src = read("src/actor/ac_set_ovl_gyoei.c")
    entries = spawn_entries(src, "FISH_SPAWN")
    lists = term_lists(src, "aSOG_term_list_c")
    hours = {t: [[set(), set()] for _ in range(12)] for t in types}  # [month][half]
    for table in ("r_month", "s_month", "p_month"):
        body = re.search(table + r"\[[^\]]*\]\[[^\]]*\] = \{(.*?)\n\};", src, re.S).group(1)
        rows = re.findall(r"\{\s*(\w+),\s*(\w+)\s*\}", body)
        assert len(rows) == 12, (table, len(rows))
        for mo, halves in enumerate(rows):
            for h, half in enumerate(halves):
                if half == "NULL":
                    continue
                for time_no, (count, name) in enumerate(lists[half]):
                    slot = [3, 0, 1, 2][time_no]  # time_no 0 = 21-3 (Night), 1 = 4-8, 2 = 9-15, 3 = 16-20
                    for t in species(entries[name], count):
                        t = "SALMON" if t == "SALMON2" else t  # aGYO_TYPE_SALMON2 becomes SALMON on spawn
                        if t in hours:
                            hours[t][mo][h] |= set(SLOT_HOURS[slot])
    return hours


def fish_sizes(types):
    # aGYO_TYPE_* -> shadow size ("XS", ...)
    sizes = dict((t, size) for size, t in re.findall(r"aGYO_SIZE_(\w+),[^}]*\}, // aGYO_TYPE_(\w+)",
                                                     read("src/actor/ac_gyoei_type.c_inc")))
    return {t: sizes[t] for t in types}


def fish_areas(types):
    # aGYO_TYPE_* -> spawn areas in the town tables (tourney f_bs_*, island f_il_* left out)
    src = read("src/actor/ac_set_ovl_gyoei.c")
    areas = {t: set(SPECIAL_FISH_AREAS.get(t, ())) for t in types}
    for m in re.finditer(r"static \w+ (\w+)\[[^\]]*\] = \{(.*?)\};", src, re.S):
        if m.group(1).startswith(("f_bs_", "f_il_")):
            continue
        for t, a in re.findall(r"FISH_SPAWN\((\w+), (\w+), [\d.]+f?\)", m.group(2)):
            t = "SALMON" if t == "SALMON2" else t
            if t in areas:
                areas[t].add(a)
    assert all(areas.values()), [t for t, a in areas.items() if not a]
    return areas


def fish_pass_table(types):
    # Per pass: [fish index, area] entries. A fish with one pass gets [i, ""] (everywhere it spawns); a fish whose
    # areas need different passes (the killifish: river Small, pond Pond) one entry per area.
    sizes, areas = fish_sizes(types), fish_areas(types)
    table = [[] for _ in FISH_PASS_NAMES]
    for i, t in enumerate(types):
        river = RIVER_PASS_OF_SIZE[sizes[t]]
        pass_of_area = {a: river if a in RIVER_AREAS else AREA_PASS[a] for a in sorted(areas[t])}
        if len(set(pass_of_area.values())) == 1:
            table[next(iter(pass_of_area.values()))].append([i, ""])
        else:
            for a, p in pass_of_area.items():
                table[p].append([i, AREA_NAME[a]])
    return table


# hours[t][month][half] -> {month: slots} (logic: either half)
def times(kind, t, hours):
    if (kind, t) in SPECIAL:
        months, slots = SPECIAL[(kind, t)]
        return {m: list(slots) for m in months}
    out = {}
    for mo in range(12):
        slots = slots_from_hours(hours[t][mo][0] | hours[t][mo][1])
        if slots:
            out[mo] = slots
    assert out, (kind, t)
    return out


# Same rules per half: [month][half] -> slot bits (bit s = SLOTS[s])
def half_bits(kind, t, hours):
    if (kind, t) in SPECIAL:
        months, slots = SPECIAL[(kind, t)]
        bits = sum(1 << s for s in slots)
        return [[bits if mo in months else 0] * 2 for mo in range(12)]
    return [[sum(1 << s for s in slots_from_hours(hours[t][mo][h])) for h in range(2)] for mo in range(12)]


def render_c(rows):
    def table(name, entries):
        lines = [f"const pc_ap_critter_data {name}[PC_AP_CRITTER_NUM] = {{"]
        for label, bits, weather in entries:
            months = ", ".join("{ %d, %d }" % (a, b) for a, b in bits)
            lines.append(f"  // {label}")
            lines.append(f"  {{ {{ {months} }}, PC_AP_WEATHER_{weather} }},")
        lines.append("};")
        return "\n".join(lines)

    c = ("// Generated by pc/tools/ap/dump_museum.py, don't edit by hand.\n"
         '#include "pc_ap_critter_data.h"\n\n'
         + table("pc_ap_bug_data", rows["bug"]) + "\n\n"
         + table("pc_ap_fish_data", rows["fish"]) + "\n")
    h = """// Generated by pc/tools/ap/dump_museum.py, don't edit by hand.
// When bugs and fish can be caught (town only), for the Bugs/Fish pause pages: same
// rules as the apworld's museum_data.py (>= 2 hours of a slot, hand-set special cases),
// per month and half (days 1-15 / 16-end; bugs have no halves, both are the same).
#ifndef PC_AP_CRITTER_DATA_H
#define PC_AP_CRITTER_DATA_H

#include "types.h"
#include "pc_ap_museumsanity.h" // PC_AP_CRITTER_NUM

#ifdef __cplusplus
extern "C" {
#endif

// Weather hint from the spawn areas
#define PC_AP_WEATHER_ANY       0
#define PC_AP_WEATHER_RAIN_ONLY 1
#define PC_AP_WEATHER_NO_RAIN   2

typedef struct {
  u8 slots[12][2]; // [month 0-11][half]: bit s = time slot s (0 Morning .. 3 Night)
  u8 weather;      // PC_AP_WEATHER_*
} pc_ap_critter_data;

// Museum index order
extern const pc_ap_critter_data pc_ap_bug_data[PC_AP_CRITTER_NUM];
extern const pc_ap_critter_data pc_ap_fish_data[PC_AP_CRITTER_NUM];

// Bug / fish passes aren't here: they come with the seed (slot_data bug_passes, fish_passes
// from the apworld's museum_data.py), see ap_slotdata.h

#ifdef __cplusplus
}
#endif

#endif
"""
    return c, h


def main():
    names = item_names()
    ftr = enum("include/m_ftr_def.h", "FTR_", "")
    def ftr_item(n):
        return 0x1000 + ftr.index(n) * 4
    fossils = [names[ftr_item("DIN_TRIKERA_HEAD") + i * 4] for i in range(25)]
    assert ftr_item("DIN_TRIKERA_HEAD") + 24 * 4 == ftr_item("DIN_TRILOBITE")
    paintings = [names[ftr_item("SUM_ART01") + i * 4] for i in range(15)]
    assert ftr_item("SUM_ART01") + 14 * 4 == ftr_item("SUM_ART15")

    bug_types = enum("include/ac_insect_h.h", "aINS_INSECT_TYPE_", "NUM")
    fish_types = enum("include/ac_gyoei.h", "aGYO_TYPE_", "NUM")
    assert len(bug_types) == 40 and len(fish_types) == 40
    bug_hours = bug_times(bug_types)
    fish_hours = fish_times(fish_types)
    bugs = [(names[0x2D00 + i], times("bug", t, bug_hours)) for i, t in enumerate(bug_types)]
    fish = [(names[0x2300 + i], times("fish", t, fish_hours)) for i, t in enumerate(fish_types)]
    fish_weather = {t: SPECIAL_WEATHER.get(("fish", t), "ANY") for t in fish_types}
    pass_table = fish_pass_table(fish_types)
    assert sorted(BUG_PASS_OF_TYPE) == sorted(bug_types), set(BUG_PASS_OF_TYPE) ^ set(bug_types)
    bw = bug_weather(bug_types)
    c_rows = {
        "bug": [(bugs[i][0], half_bits("bug", t, bug_hours), bw[t]) for i, t in enumerate(bug_types)],
        "fish": [(fish[i][0], half_bits("fish", t, fish_hours), fish_weather[t]) for i, t in enumerate(fish_types)],
    }

    def critters(rows):
        lines = []
        for name, when in rows:
            lines.append(f"    ({name!r}, {{{', '.join(f'{m}: {tuple(s)}' for m, s in when.items())}}}),")
        return "\n".join(lines)

    py = ("# Generated by pc/tools/ap/dump_museum.py, don't edit by hand.\n"
          "# Museum things in museum index order (the game's donation slot numbers).\n"
          "# Bugs and fish: (name, {month 0-11: time slots}), slots index timesanity.TIME_SLOTS.\n\n"
          + "FOSSILS = [\n" + "".join(f"    {n!r},\n" for n in fossils) + "]\n\n"
          + "PAINTINGS = [\n" + "".join(f"    {n!r},\n" for n in paintings) + "]\n\n"
          + "BUGS = [\n" + critters(bugs) + "\n]\n\n"
          + "FISH = [\n" + critters(fish) + "\n]\n\n"
          + "# Fish passes: per pass (index into fish_availability.FISH_PASS_ITEMS), the fish it opens as [fish index\n"
          + "# (FISH order), area]; area \"\" = everywhere the fish spawns, else only there (river, waterfall, river_mouth,\n"
          + "# lake, pond, sea). A fish needs any pass that lists it.\n"
          + "FISH_PASS_TABLE = [\n" + "".join(
              f"    [  # {FISH_PASS_NAMES[p]}\n" + "".join(f"        [{i}, {a!r}],  # {fish[i][0]}\n".replace("'", '"')
                                                         for i, a in entries) + "    ],\n"
              for p, entries in enumerate(pass_table)) + "]\n\n"
          + "# Bug passes per bug (BUGS order): index into museum.BUG_PASS_ITEMS\n"
          + "BUG_PASSES = [\n" + "".join(f"    {BUG_PASS_OF_TYPE[t]},  # {bugs[i][0]}\n"
                                         for i, t in enumerate(bug_types)) + "]\n")
    c, h = render_c(c_rows)
    outputs = [(OUT, py), (OUT_C, c), (OUT_H, h)]
    if "--check" in sys.argv:
        bad = 0
        for path, text in outputs:
            same = os.path.exists(path) and open(path).read() == text
            bad += not same
            print(f"  {'ok  ' if same else 'FAIL'} {os.path.relpath(path, REPO)} up to date (dump_museum.py)")
        sys.exit(1 if bad else 0)
    for path, text in outputs:
        with open(path, "w") as f:
            f.write(text)
        print("wrote", path)


main()
