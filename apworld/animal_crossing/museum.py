# Museumsanity: categories, check modes, item and location names/ids, fossil seasons.
# The museum things themselves are generated (museum_data.py); this file is hand-written.
# WARN: the client mirrors the ids, bits and seasons (pc_ap_museumsanity.h, ap_slotdata.h/.cpp)!
from .museum_data import BUGS, FISH, FOSSILS, PAINTINGS

# Categories in the game's order (mMmd_CATEGORY_*), things in museum index order.
MUSEUM_FOSSIL, MUSEUM_PAINTING, MUSEUM_BUG, MUSEUM_FISH = range(4)
MUSEUM_NAMES = [FOSSILS, PAINTINGS, [name for name, _ in BUGS], [name for name, _ in FISH]]

# Check mode bits per category: the check option values and the client's AP_MUSEUM_*. 0 = vanilla.
CHECK_FIND = 1    # catching a bug/fish, digging up a fossil (pre-appraised)
CHECK_DONATE = 2  # donating it

# Museum slot = category * 0x40 + index: the same number for the item and both of its locations.
MUSEUM_ITEM_BASE_ID = 0x10100    # "Museum: X" (in the AP item range of items.py)
MUSEUM_DONATE_BASE_ID = 0x30000  # "Donate X" (in the location range of locations.py)
MUSEUM_FIND_BASE_ID = 0x30100    # "Catch X" / "Dig Up X"
# Categories with find locations. Paintings have none yet (room left for "Buy").
FIND_VERBS = {MUSEUM_FOSSIL: "Dig Up", MUSEUM_BUG: "Catch", MUSEUM_FISH: "Catch"}


def museum_slot(category: int, index: int) -> int:
    return category * 0x40 + index


def museum_item_name(name: str) -> str:
    return f"Museum: {name}"


def museum_donate_name(name: str) -> str:
    return f"Donate {name}"


def museum_find_name(category: int, name: str) -> str:
    # KeyError for a category without find locations (paintings)
    return f"{FIND_VERBS[category]} {name}"


MUSEUM_ITEM_NAME_TO_ID = {}
MUSEUM_LOCATION_NAME_TO_ID = {}
for _c, _names in enumerate(MUSEUM_NAMES):
    for _i, _name in enumerate(_names):
        _slot = museum_slot(_c, _i)
        MUSEUM_ITEM_NAME_TO_ID[museum_item_name(_name)] = MUSEUM_ITEM_BASE_ID + _slot
        MUSEUM_LOCATION_NAME_TO_ID[museum_donate_name(_name)] = MUSEUM_DONATE_BASE_ID + _slot
        if _c in FIND_VERBS:
            MUSEUM_LOCATION_NAME_TO_ID[museum_find_name(_c, _name)] = MUSEUM_FIND_BASE_ID + _slot

# Fossil Spawns "Season Locked": the season each fossil comes out in, dinosaurs kept together.
# WARN: the client has a copy for offline play (ap_slotdata.cpp default_fossil_seasons)!
SEASONS = ["Spring", "Summer", "Autumn", "Winter"]
SEASON_MONTHS = [(2, 3, 4), (5, 6, 7), (8, 9, 10), (11, 0, 1)]  # month 0-11
_SEASON_OF_FOSSIL = {
    # Spring: early life, hatching, flyers coming back
    "Trilobite": 0, "Ammonite": 0, "Dinosaur Egg": 0, "Ptera Skull": 0, "Ptera Right Wing": 0, "Ptera Left Wing": 0,
    # Summer: the Jurassic giants
    "Apato Skull": 1, "Apato Tail": 1, "Apato Torso": 1, "Stego Skull": 1, "Stego Tail": 1, "Stego Torso": 1,
    # Autumn: the Late Cretaceous, the dinosaurs' "fall"
    "T-rex Skull": 2, "T-rex Tail": 2, "T-rex Torso": 2, "Tricera Skull": 2, "Tricera Tail": 2, "Tricera Torso": 2,
    # Winter: ice age, cold seas, tracks in the snow
    "Mammoth Skull": 3, "Mammoth Torso": 3, "Plesio Skull": 3, "Plesio Neck": 3, "Plesio Torso": 3,
    "Amber": 3, "Dinosaur Track": 3,
}
FOSSIL_SEASONS = [_SEASON_OF_FOSSIL[name] for name in FOSSILS]  # museum fossil index -> season
