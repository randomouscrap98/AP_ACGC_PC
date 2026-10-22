# Fossil Availability: which fossils can be dug up. All / Season Locked (each fossil only in its season, logic
# needs a month of it under Timesanity) / Progressive (one item per dinosaur series, "Single Fossils" for the
# rest; nothing is buried before the first). Progressive only with fossil checks, else the client buries all.
# WARN: the client mirrors the series order and ids (PC_AP_FOSSIL_SERIES_*, PC_AP_ITEM_FOSSIL_SERIES_BASE) and has a
# copy of the seasons for offline play (ap_slotdata.cpp default_fossil_seasons)! Seasons and series per fossil go to
# the client in slot_data: the seed's own tables count (Universal Tracker reads them back from the seed).
# Reads: world.museum_modes (museumsanity.setup).
from BaseClasses import ItemClassification

from . import options, timesanity
from .museum_data import FOSSILS
from .museumsanity import MUSEUM_FOSSIL, add_museum_rule
from .timesanity import MONTHS

# Season Locked: the season each fossil comes out in, dinosaurs kept together.
SEASON_MONTHS = [(2, 3, 4), (5, 6, 7), (8, 9, 10), (11, 0, 1)]  # Spring, Summer, Autumn, Winter: months 0-11
_SEASON_OF_FOSSIL = {
    # Spring: early life, hatching, flyers coming back
    "trilobite": 0, "ammonite": 0, "dinosaur egg": 0, "ptera skull": 0, "ptera right wing": 0, "ptera left wing": 0,
    # Summer: the Jurassic giants
    "apato skull": 1, "apato tail": 1, "apato torso": 1, "stego skull": 1, "stego tail": 1, "stego torso": 1,
    # Autumn: the Late Cretaceous, the dinosaurs' "fall"
    "T-rex skull": 2, "T-rex tail": 2, "T-rex torso": 2, "tricera skull": 2, "tricera tail": 2, "tricera torso": 2,
    # Winter: ice age, cold seas, tracks in the snow
    "mammoth skull": 3, "mammoth torso": 3, "plesio skull": 3, "plesio neck": 3, "plesio torso": 3,
    "amber": 3, "dinosaur track": 3,
}
FOSSIL_SEASONS = [_SEASON_OF_FOSSIL[name] for name in FOSSILS]  # museum fossil index -> season

# Progressive: one item per series opens its fossils; "Single Fossils" opens the rest (Amber, Dinosaur Track,
# Ammonite, Dinosaur Egg, Trilobite).
FOSSIL_SERIES = ["Tricera", "T-rex", "Apato", "Stego", "Ptera", "Plesio", "Mammoth", "Single"]
FOSSIL_SERIES_ID_BASE = 0x10200  # series s = base + s; 64 ids reserved (0x10200-0x1023F)


def fossil_series_item_name(series: str) -> str:
    return f"{series} Fossils"


FOSSIL_SERIES_ITEMS = [fossil_series_item_name(series) for series in FOSSIL_SERIES]
ITEMS = {name: FOSSIL_SERIES_ID_BASE + s for s, name in enumerate(FOSSIL_SERIES_ITEMS)}
# museum fossil index -> its series item name ("tricera skull" -> "Tricera Fossils", "amber" -> "Single Fossils")
FOSSIL_SERIES_OF = []
for _name in FOSSILS:
    # Game names are lowercase ("tricera skull", but "T-rex skull"); the series items are capitalized
    _series = [s for s in FOSSIL_SERIES if s.lower() == _name.split(" ")[0].lower()]
    FOSSIL_SERIES_OF.append(fossil_series_item_name(_series[0] if _series else "Single"))
FOSSIL_SERIES_INDEX = [FOSSIL_SERIES_ITEMS.index(item) for item in FOSSIL_SERIES_OF]  # museum fossil index -> series


def setup(world) -> None:
    # Per fossil (museum order): season 0-3, series index into FOSSIL_SERIES
    ut = world.ut or {}
    world.fossil_seasons = list(ut.get("fossil_seasons", FOSSIL_SEASONS))
    world.fossil_series = list(ut.get("fossil_series", FOSSIL_SERIES_INDEX))
    world.fossil_series_items = []
    if (world.options.fossil_availability == options.FossilAvailability.option_progressive
            and world.museum_modes[MUSEUM_FOSSIL]):
        world.fossil_series_items = list(FOSSIL_SERIES_ITEMS)


def items(world) -> list[str]:
    return world.fossil_series_items


def classify(world, name: str):
    return ItemClassification.progression if name in ITEMS else None


def rules(world) -> None:
    # Season Locked (dig up and donate): a month of its season
    if world.options.fossil_availability == options.FossilAvailability.option_season_locked:
        for name, season in zip(FOSSILS, world.fossil_seasons):
            months = timesanity.month_rule(world, [MONTHS[m] for m in SEASON_MONTHS[season]])
            if months:
                add_museum_rule(world, MUSEUM_FOSSIL, name, months)
    # Progressive: a fossil needs its series item
    if world.fossil_series_items:
        for name, s in zip(FOSSILS, world.fossil_series):
            item = FOSSIL_SERIES_ITEMS[s]
            add_museum_rule(world, MUSEUM_FOSSIL, name, lambda state, item=item: state.has(item, world.player))


def slot_data(world) -> dict:
    return {
        "fossil_availability": world.options.fossil_availability.value,
        "fossil_seasons": world.fossil_seasons,
        "fossil_series": world.fossil_series,
    }


# Unused hooks (world.py calls every hook of every feature)
LOCATIONS: dict[str, int] = {}


def locations(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []
