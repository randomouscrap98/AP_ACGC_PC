# Fish Availability: All / Progressive. Progressive: a fish spawns once any of its items is received (FISH_PASS_TABLE,
# generated from the game's spawn areas and shadow sizes: river by size, ponds + the river's lake, sea). Only with
# fish checks, else the client spawns every fish. Island fish are untouched.
# WARN: the client mirrors the order and ids (PC_AP_FISH_PASS_*, PC_AP_ITEM_FISH_PASS_BASE)!
# The table goes to the client in slot_data (fish_passes: per pass, [fish index, area] entries, area "" = everywhere):
# the seed's own table is what counts, so a later change to it can't change a generated seed (Universal Tracker
# reads it back from the seed).
# Reads: world.museum_modes (museumsanity.setup).
from BaseClasses import ItemClassification

from . import options
from .museum_data import FISH, FISH_PASS_TABLE
from .museumsanity import MUSEUM_FISH, add_museum_rule

FISH_PASS_ITEMS = ["Small River Fish", "Medium River Fish", "Large River Fish", "Pond Fish", "Sea Fish"]
FISH_PASS_ID_BASE = 0x10240  # pass p = base + p; 64 ids reserved (0x10240-0x1027F)
ITEMS = {name: FISH_PASS_ID_BASE + p for p, name in enumerate(FISH_PASS_ITEMS)}


def setup(world) -> None:
    # Per pass: [fish index, area] entries (slot_data); per fish (FISH order): the passes that list it, any one opens it
    world.fish_pass_table = [[list(e) for e in entries]
                             for entries in (world.ut or {}).get("fish_passes", FISH_PASS_TABLE)]
    world.fish_passes = [sorted({p for p, entries in enumerate(world.fish_pass_table) for f, _ in entries if f == i})
                         for i in range(len(FISH))]
    world.fish_pass_items = []
    if (world.options.fish_availability == options.FishAvailability.option_progressive
            and world.museum_modes[MUSEUM_FISH]):
        world.fish_pass_items = list(FISH_PASS_ITEMS)


def items(world) -> list[str]:
    return world.fish_pass_items


def classify(world, name: str):
    return ItemClassification.progression if name in ITEMS else None


def rules(world) -> None:
    # A fish (catch and donate) needs any of its items
    if world.fish_pass_items:
        for (name, _), passes in zip(FISH, world.fish_passes):
            items = [FISH_PASS_ITEMS[p] for p in passes]
            add_museum_rule(world, MUSEUM_FISH, name, lambda state, items=items: state.has_any(items, world.player))


def slot_data(world) -> dict:
    return {
        "fish_availability": world.options.fish_availability.value,
        "fish_passes": world.fish_pass_table,
    }


# Unused hooks (world.py calls every hook of every feature)
LOCATIONS: dict[str, int] = {}


def locations(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []
