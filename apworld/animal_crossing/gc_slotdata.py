# slot_data -> the game's ap_slotdata struct (apclient/include/ap_slotdata.h), for the GameCube client
# (gc_client.py), which writes it big-endian into the game's mailbox. The PC client DLL builds the same struct
# from the same json in apclient/src/ap_slotdata.cpp (ap_slotdata_fill); this is a line-by-line copy of it.
# Keep the two in step: tests/test_gc_slotdata (ignore/claude) fills both from the same json and compares bytes,
# and the game refuses a mirror whose size or field offsets differ from its own (mailbox slotdata_check).
import ctypes
from typing import Any

from .slot_data_upgrade import upgrade

# ap_slotdata.h
AP_SLOTDATA_VERSION = 6
AP_LOAN_NUM = 5
AP_BELLCREDIT_NUM = 3
AP_NPC_NUM = 236
AP_HOUR_NUM = 24
AP_FURNITURE_SET_MAX = 15
AP_FOSSIL_NUM = 25
AP_CRITTER_NUM = 40
AP_BUG_PASS_NUM = 5
AP_FISH_PASS_NUM = 5
AP_FOSSIL_SERIES_NUM = 8
AP_FISH_AREA_NUM = 6
AP_BEACH_PER_MONTH_MAX = 20
AP_TREE_PER_FRUIT_MAX = 20
AP_CATALOG_MAX = 575
AP_STORE_LEVELS = 3

AP_GOAL_STATUE = 1 << 0
AP_GOAL_MUSEUM = 1 << 1
AP_GOAL_FROGGY = 1 << 2
AP_GOAL_CATALOG = 1 << 3
AP_GOAL_FURNITURE_SET = 1 << 4
AP_GOAL_CHESS_SET = 1 << 5
AP_MUSEUM_FIND = 1 << 0
AP_MUSEUM_DONATE = 1 << 1
AP_STORE_VANILLA, AP_STORE_CATALOG = 0, 3
AP_CRITTER_ODDS_VANILLA, AP_CRITTER_ODDS_DYNAMIC = 0, 2
AP_BUG_AVAIL_ALL, AP_BUG_AVAIL_PROGRESSIVE = 0, 1
AP_FISH_AVAIL_ALL, AP_FISH_AVAIL_PROGRESSIVE = 0, 1
AP_FOSSIL_AVAIL_ALL, AP_FOSSIL_AVAIL_PROGRESSIVE = 0, 2
AP_FOSSIL_ODDS_VANILLA, AP_FOSSIL_ODDS_DYNAMIC = 0, 2
INT_MAX = 2**31 - 1

c_int, c_char, c_ubyte, c_ushort = ctypes.c_int32, ctypes.c_char, ctypes.c_ubyte, ctypes.c_uint16

# Same order and types as ap_slotdata (int = 32 bits, unsigned short = 16, char = 8 on both the PC and the GC)
FIELDS = [
    ("player_name", c_char * 9),
    ("town_name", c_char * 9),
    ("house", c_int),
    ("gender", c_int),
    ("face", c_int),
    ("starting_shirt", c_int),
    ("town_fruit", c_int),
    ("grass_shape", c_int),
    ("train_station", c_int),
    ("town_day", c_int),
    ("letter_paper", c_int),
    ("letter_sender", c_char * 33),
    ("loan_letter_text", c_char * 193),
    ("villager_blacklist", c_ubyte * AP_NPC_NUM),
    ("starting_villagers", c_ubyte * AP_NPC_NUM),
    ("hourly_music", c_ubyte * AP_HOUR_NUM),
    ("skip_intro", c_int),
    ("shops_always_open", c_int),
    ("global_storage", c_int),
    ("no_weeds", c_int),
    ("more_favors", c_int),
    ("villagers_dont_leave", c_int),
    ("villagers_never_sleep", c_int),
    ("turnips_never_spoil", c_int),
    ("no_falling_stalks", c_int),
    ("skip_hra_talk", c_int),
    ("shop_never_sells_out", c_int),
    ("coconuts_wash_up", c_int),
    ("normalized_time_travel", c_int),
    ("goal", c_int),
    ("loansanity", c_int),
    ("loans", c_int * AP_LOAN_NUM),
    ("loan_checks", c_int * AP_LOAN_NUM),
    ("favorsanity", c_int),
    ("favors_per_pool", c_int),
    ("tools_in_pool", c_int),
    ("store_upgrades", c_int),
    ("store_thresholds", c_int * AP_STORE_LEVELS),
    ("museumsanity", c_int),
    ("bug_checks", c_int),
    ("fish_checks", c_int),
    ("fossil_checks", c_int),
    ("painting_checks", c_int),
    ("museum_goal_count", c_int),
    ("critter_odds", c_int),
    ("bug_availability", c_int),
    ("fish_availability", c_int),
    ("fossil_availability", c_int),
    ("fossil_odds", c_int),
    ("fossil_seasons", c_int * AP_FOSSIL_NUM),
    ("bug_passes", c_int * AP_CRITTER_NUM),
    ("fish_area_passes", (c_int * AP_FISH_AREA_NUM) * AP_CRITTER_NUM),
    ("fossil_series", c_int * AP_FOSSIL_NUM),
    ("timesanity", c_int),
    ("starting_month", c_int),
    ("starting_time", c_int),
    ("start_year", c_int),
    ("catalogsanity", c_int),
    ("catalog_count", c_int),
    ("catalog_items", c_ushort * AP_CATALOG_MAX),
    ("catalog_pool", c_ubyte * AP_CATALOG_MAX),
    ("new_item_chance", c_int),
    ("catalog_sources", c_int),
    ("beach_per_month", c_int),
    ("shellsanity_item_chance", c_int),
    ("tree_per_fruit", c_int),
    ("catalog_goal_count", c_int),
    ("bell_credits", c_int * AP_BELLCREDIT_NUM),
    ("furniture_set_count", c_int),
    ("furniture_set_items", c_ushort * AP_FURNITURE_SET_MAX),
    ("world_version", c_char * 16),
    ("valid", c_int),
]

# The fields whose offsets the game puts in the mailbox header (gc_ap_backend.c slotdata_check: same order)
SLOTDATA_CHECKS = ["skip_intro", "goal", "museumsanity", "bug_passes", "timesanity", "new_item_chance",
                   "world_version", "valid"]


class SlotDataBE(ctypes.BigEndianStructure):
    _fields_ = FIELDS  # GameCube


class SlotDataLE(ctypes.LittleEndianStructure):
    _fields_ = FIELDS  # PC (tests compare it with the DLL's bytes)


def layout_matches(size: int, offsets: list[int]) -> bool:
    # The game's sizeof(ap_slotdata) and slotdata_check offsets against this mirror
    return size == ctypes.sizeof(SlotDataBE) and \
        offsets == [getattr(SlotDataBE, name).offset for name in SLOTDATA_CHECKS]


def _warn(msg: str) -> None:
    import logging
    logging.getLogger("Client").warning("slot_data: %s", msg)


def _is_int(v: Any) -> bool:
    return isinstance(v, int) and not isinstance(v, bool)


def _c_str(v: Any, size: int) -> bytes:
    # snprintf(buf, size, "%s", ...): cut to size - 1 bytes
    return str(v).encode("utf-8")[:size - 1]


# read_int: an int key in [lo, hi]; missing = default, anything else = default + a warning; true/false = 1/0
def _read_int(sd: dict[str, Any], key: str, default: int, lo: int, hi: int) -> int:
    if key not in sd:
        return default
    raw = sd[key]
    v = lo - 1
    if isinstance(raw, bool):
        v = 1 if raw else 0
    elif _is_int(raw):
        v = raw
    if lo <= v <= hi:
        return v
    _warn(f"{key}: {raw!r} is not in {lo}..{hi}, using {default}")
    return default


def _read_bool(sd: dict[str, Any], key: str, default: int) -> int:
    return _read_int(sd, key, default, 0, 1)


def _read_int_array(sd: dict[str, Any], key: str, defaults: list[int]) -> list[int]:
    out = list(defaults)
    arr = sd.get(key)
    if not isinstance(arr, list):
        return out
    for i, v in enumerate(arr[:len(out)]):
        if _is_int(v):
            out[i] = _to_c_int(v)
    return out


def _to_c_int(v: int) -> int:
    # nlohmann get<int>() of a big json number wraps like a C cast
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v >= 1 << 31 else v


def _read_pass_array(sd: dict[str, Any], key: str, n: int, num: int) -> list[int]:
    out = [-1] * n
    arr = sd.get(key)
    if not isinstance(arr, list):
        return out
    for i, v in enumerate(arr[:n]):
        if _is_int(v) and 0 <= v < num:
            out[i] = v
        else:
            _warn(f"{key}[{i}]: ignoring {v!r}")
    return out


def _read_fish_passes(sd: dict[str, Any]) -> list[list[int]]:
    areas = ["river", "waterfall", "river_mouth", "lake", "pond", "sea"]
    out = [[0] * AP_FISH_AREA_NUM for _ in range(AP_CRITTER_NUM)]
    passes = sd.get("fish_passes")
    if not isinstance(passes, list):
        return out
    for p, entries in enumerate(passes[:AP_FISH_PASS_NUM]):
        if not isinstance(entries, list):
            _warn(f"fish_passes[{p}]: ignoring {entries!r}")
            continue
        for e in entries:
            fish, area = -1, -2  # area -1: every area
            if isinstance(e, list) and len(e) == 2 and _is_int(e[0]) and isinstance(e[1], str):
                if 0 <= e[0] < AP_CRITTER_NUM:
                    fish = e[0]
                if e[1] == "":
                    area = -1
                if e[1] in areas:
                    area = areas.index(e[1])
            if fish < 0 or area == -2:
                _warn(f"fish_passes[{p}]: ignoring {e!r}")
                continue
            for a in range(AP_FISH_AREA_NUM):
                if area in (-1, a):
                    out[fish][a] |= 1 << p
    return out


def _read_npc_list(sd: dict[str, Any], key: str) -> list[int]:
    out = [0] * AP_NPC_NUM
    arr = sd.get(key)
    if not isinstance(arr, list):
        return out
    for v in arr:
        if _is_int(v) and 0 <= v < AP_NPC_NUM:
            out[v] = 1
        else:
            _warn(f"{key}: ignoring bad entry {v!r}")
    return out


def _read_hourly_music(sd: dict[str, Any]) -> list[int]:
    # read_hourly_music: 24 songs, each of 1-24 once; anything else = all 0 (the game's own)
    arr = sd.get("hourly_music")
    if not isinstance(arr, list) or not arr:
        return [0] * AP_HOUR_NUM
    if len(arr) != AP_HOUR_NUM:
        _warn(f"hourly_music: {len(arr)} entries, not {AP_HOUR_NUM}: ignored")
        return [0] * AP_HOUR_NUM
    seen = set()
    for v in arr:
        if not _is_int(v) or not 1 <= v <= AP_HOUR_NUM or v in seen:
            _warn(f"hourly_music: bad entry {v!r}: ignored")
            return [0] * AP_HOUR_NUM
        seen.add(v)
    return list(arr)


def _read_catalog_pools(sd: dict[str, Any]) -> tuple[list[int], list[int]]:
    items: list[int] = []
    pools: list[int] = []
    arr = sd.get("catalog_pools")
    if not isinstance(arr, list):
        return items, pools
    for pool, entries in enumerate(arr):
        if not isinstance(entries, list):
            continue
        for v in entries:
            if not _is_int(v) or v <= 0 or v > 0xFFFF:
                _warn(f"catalog_pools: ignoring bad entry {v!r}")
            elif len(items) >= AP_CATALOG_MAX:
                _warn(f"catalog_pools: more than {AP_CATALOG_MAX} items, ignoring {v!r}")
            else:
                items.append(v)
                pools.append(min(pool, 255))
    return items, pools


def _read_furniture_set(sd: dict[str, Any]) -> list[int]:
    out: list[int] = []
    arr = sd.get("furniture_set_items")
    if not isinstance(arr, list):
        return out
    for v in arr:
        if not _is_int(v) or v <= 0 or v > 0xFFFF or len(out) >= AP_FURNITURE_SET_MAX:
            _warn(f"furniture_set_items: ignoring {v!r}")
        else:
            out.append(v)
    return out


def _read_catalog_sources(sd: dict[str, Any]) -> int:
    names = ["Favors", "Trades", "Lost and Found", "Dump", "Letters", "Trees", "Presents", "Grab Bags", "Wisp"]
    arr = sd.get("catalog_sources")
    if not isinstance(arr, list):
        return 0
    bits = 0
    for v in arr:
        if isinstance(v, str) and v in names:
            bits |= 1 << names.index(v)
    return bits


def _read_goal(sd: dict[str, Any]) -> int:
    names = {"Statue": AP_GOAL_STATUE, "Museum": AP_GOAL_MUSEUM, "Froggy Chair": AP_GOAL_FROGGY,
             "Catalog": AP_GOAL_CATALOG, "Furniture Set": AP_GOAL_FURNITURE_SET, "Chess Set": AP_GOAL_CHESS_SET}
    arr = sd.get("goal")
    if not isinstance(arr, list):
        return AP_GOAL_STATUE
    goal = 0
    for g in arr:
        if isinstance(g, str):
            goal |= names.get(g, 0)
    return goal


def _value_str(sd: dict[str, Any], key: str, default: str) -> str:
    # nlohmann json.value(key, default) for strings
    v = sd.get(key, default)
    return v if isinstance(v, str) else default


def fill(s: ctypes.Structure, slot_data: dict[str, Any]) -> None:
    # ap_slotdata_fill: slot_data (already upgraded) into s (zeroed)
    sd = slot_data
    s.town_name = _c_str(_value_str(sd, "town_name", "pelago"), 9)
    s.player_name = _c_str(_value_str(sd, "player_name", "archi"), 9)
    s.skip_intro = _read_int(sd, "skip_intro", 2, 0, 2)
    s.gender = _read_int(sd, "gender", 0, 0, 1)
    s.face = _read_int(sd, "face", 0, 0, 7)
    s.starting_shirt = _read_int(sd, "starting_shirt", 16, 0, 16)
    s.house = _read_int(sd, "house", 0, 0, 3)
    s.town_fruit = _read_int(sd, "town_fruit", 0, 0, 4)
    s.grass_shape = _read_int(sd, "grass_shape", 0, 0, 2)
    s.train_station = _read_int(sd, "train_station", -1, -1, 14)
    s.town_day = _read_int(sd, "town_day", 0, 0, 31)
    s.letter_paper = _read_int(sd, "letter_paper", 0, 0, 63)
    s.letter_sender = _c_str(_value_str(sd, "letter_sender", "Archipelago"), 33)
    s.loan_letter_text = _c_str(_value_str(sd, "loan_letter_text",
                                           "Your loan is ready for\npayoff at the post office!"), 193)
    s.villager_blacklist[:] = _read_npc_list(sd, "villager_blacklist")
    s.starting_villagers[:] = _read_npc_list(sd, "starting_villagers")
    s.hourly_music[:] = _read_hourly_music(sd)
    s.shops_always_open = _read_bool(sd, "shops_always_open", 0)
    s.global_storage = _read_int(sd, "global_storage", 0, 0, 2)
    s.no_weeds = _read_bool(sd, "no_weeds", 1)
    s.more_favors = _read_bool(sd, "more_favors", 1)
    s.villagers_dont_leave = _read_bool(sd, "villagers_dont_leave", 0)
    s.villagers_never_sleep = _read_bool(sd, "villagers_never_sleep", 0)
    s.turnips_never_spoil = _read_bool(sd, "turnips_never_spoil", 1)
    s.no_falling_stalks = _read_bool(sd, "no_falling_stalks", 0)
    s.skip_hra_talk = _read_bool(sd, "skip_hra_talk", 1)
    s.shop_never_sells_out = _read_bool(sd, "shop_never_sells_out", 0)
    s.coconuts_wash_up = _read_bool(sd, "coconuts_wash_up", 1)
    s.normalized_time_travel = _read_bool(sd, "normalized_time_travel", 1)

    s.goal = _read_goal(sd)
    s.loansanity = _read_bool(sd, "loansanity", 1)
    s.loans[:] = _read_int_array(sd, "loans", [15000, 45000, 30000, 60000, 80000])
    s.loan_checks[:] = _read_int_array(sd, "loan_checks", [0] * AP_LOAN_NUM)
    favor_pools = _read_int(sd, "favor_pools", 0, 0, 100) if _read_bool(sd, "favorsanity", 0) else 0
    s.favors_per_pool = _read_int(sd, "favors_per_pool", 0, 0, 100)
    s.favorsanity = favor_pools * s.favors_per_pool
    s.tools_in_pool = _read_bool(sd, "tools_in_pool", 0)
    s.store_upgrades = _read_int(sd, "nook_store_upgrades", AP_STORE_VANILLA, AP_STORE_VANILLA, AP_STORE_CATALOG)
    s.store_thresholds[:] = _read_int_array(sd, "nook_store_thresholds", [0] * AP_STORE_LEVELS)
    s.museumsanity = _read_bool(sd, "museumsanity", 0)
    both = AP_MUSEUM_FIND | AP_MUSEUM_DONATE
    s.bug_checks = _read_int(sd, "bug_checks", 0, 0, both)
    s.fish_checks = _read_int(sd, "fish_checks", 0, 0, both)
    s.fossil_checks = _read_int(sd, "fossil_checks", 0, 0, both)
    s.painting_checks = _read_int(sd, "painting_checks", 0, 0, both)
    s.museum_goal_count = _read_int(sd, "museum_goal_count", 0, 0, INT_MAX)
    s.critter_odds = _read_int(sd, "critter_odds", AP_CRITTER_ODDS_DYNAMIC, AP_CRITTER_ODDS_VANILLA,
                               AP_CRITTER_ODDS_DYNAMIC)
    s.bug_availability = _read_int(sd, "bug_availability", AP_BUG_AVAIL_ALL, AP_BUG_AVAIL_ALL,
                                   AP_BUG_AVAIL_PROGRESSIVE)
    s.fish_availability = _read_int(sd, "fish_availability", AP_FISH_AVAIL_ALL, AP_FISH_AVAIL_ALL,
                                    AP_FISH_AVAIL_PROGRESSIVE)
    s.fossil_availability = _read_int(sd, "fossil_availability", AP_FOSSIL_AVAIL_ALL, AP_FOSSIL_AVAIL_ALL,
                                      AP_FOSSIL_AVAIL_PROGRESSIVE)
    s.fossil_odds = _read_int(sd, "fossil_odds", AP_FOSSIL_ODDS_DYNAMIC, AP_FOSSIL_ODDS_VANILLA,
                              AP_FOSSIL_ODDS_DYNAMIC)
    # Copy of the apworld's FOSSIL_SEASONS, as in ap_slotdata.cpp
    default_seasons = [2] * 6 + [1] * 6 + [0] * 3 + [3] * 5 + [3] * 2 + [0] * 3
    s.fossil_seasons[:] = [v & 3 for v in _read_int_array(sd, "fossil_seasons", default_seasons)]
    s.bug_passes[:] = _read_pass_array(sd, "bug_passes", AP_CRITTER_NUM, AP_BUG_PASS_NUM)
    for fish, row in enumerate(_read_fish_passes(sd)):
        s.fish_area_passes[fish][:] = row
    s.fossil_series[:] = _read_pass_array(sd, "fossil_series", AP_FOSSIL_NUM, AP_FOSSIL_SERIES_NUM)
    s.timesanity = _read_bool(sd, "timesanity", 0)
    s.starting_month = _read_int(sd, "starting_month", 0, 0, 11)
    s.starting_time = _read_int(sd, "starting_time", 0, 0, 3)
    s.start_year = _read_int(sd, "start_year", 2001, 2001, 2100)
    s.catalogsanity = _read_bool(sd, "catalogsanity", 0)
    items, pools = _read_catalog_pools(sd)
    s.catalog_count = len(items)
    s.catalog_items[:len(items)] = items
    s.catalog_pool[:len(pools)] = pools
    s.new_item_chance = _read_int(sd, "new_item_chance", 0, 0, 100)
    s.catalog_sources = _read_catalog_sources(sd)
    s.catalog_goal_count = _read_int(sd, "catalog_goal_count", 0, 0, AP_CATALOG_MAX)
    pieces = _read_furniture_set(sd)
    s.furniture_set_count = len(pieces)
    s.furniture_set_items[:len(pieces)] = pieces
    s.beach_per_month = _read_int(sd, "beach_checks_per_month", 0, 0, AP_BEACH_PER_MONTH_MAX) \
        if _read_bool(sd, "shellsanity", 0) else 0
    s.shellsanity_item_chance = _read_int(sd, "shellsanity_item_chance", 20, 0, 100)
    s.tree_per_fruit = _read_int(sd, "tree_checks_per_fruit", 0, 0, AP_TREE_PER_FRUIT_MAX) \
        if _read_bool(sd, "treesanity", 0) else 0

    credits = sd.get("bell_credits")
    for i, name in enumerate(["Small Bell Credit", "Modest Bell Credit", "Large Bell Credit"]):
        v = credits.get(name) if isinstance(credits, dict) else None
        s.bell_credits[i] = _to_c_int(v) if _is_int(v) else 0
    s.world_version = _c_str(_value_str(sd, "world_version", ""), 16)
    s.valid = 1


def build(slot_data: dict[str, Any], big_endian: bool = True) -> bytes:
    # The upgraded slot_data as the game's struct bytes. ValueError if it's from a newer apworld than this client
    if int(slot_data.get("slot_data_version", 1)) > AP_SLOTDATA_VERSION:
        raise ValueError("slot_data from a newer apworld: update the client")
    upgraded = upgrade(dict(slot_data), AP_SLOTDATA_VERSION)
    s = SlotDataBE() if big_endian else SlotDataLE()
    fill(s, upgraded)
    return bytes(s)
