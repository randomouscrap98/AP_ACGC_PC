# Bug Availability: All / Progressive. Progressive: a bug spawns once its item is received (BUG_PASSES, generated:
# split by where bugs live, trees split into cicadas & co. and beetles); no bee trees before Tree Bugs. Only with
# bug checks, else the client spawns every bug. Island bugs are untouched.
# WARN: the client mirrors the order and ids (PC_AP_BUG_PASS_*, PC_AP_ITEM_BUG_PASS_BASE)!
# The table goes to the client in slot_data (bug_passes): the seed's own table is what counts, so a later change
# to BUG_PASSES can't change a generated seed (Universal Tracker reads it back from the seed).
# Reads: world.museum_modes (museumsanity.setup).
from BaseClasses import ItemClassification

from . import options
from .museum_data import BUG_PASSES, BUGS
from .museumsanity import MUSEUM_BUG, add_museum_rule

BUG_PASS_ITEMS = ["Tree Bugs", "Beetles", "Flower Bugs", "Ground Bugs", "Flying Bugs"]
BUG_PASS_ID_BASE = 0x10280  # pass p = base + p; 64 ids reserved (0x10280-0x102BF)
ITEMS = {name: BUG_PASS_ID_BASE + p for p, name in enumerate(BUG_PASS_ITEMS)}


def setup(world) -> None:
    # Pass index per bug (BUGS order)
    world.bug_passes = list((world.ut or {}).get("bug_passes", BUG_PASSES))
    world.bug_pass_items = []
    if (world.options.bug_availability == options.BugAvailability.option_progressive
            and world.museum_modes[MUSEUM_BUG]):
        world.bug_pass_items = list(BUG_PASS_ITEMS)


def items(world) -> list[str]:
    return world.bug_pass_items


def classify(world, name: str):
    return ItemClassification.progression if name in ITEMS else None


def rules(world) -> None:
    # A bug (catch and donate) needs its item
    if world.bug_pass_items:
        for (name, _), p in zip(BUGS, world.bug_passes):
            item = BUG_PASS_ITEMS[p]
            add_museum_rule(world, MUSEUM_BUG, name, lambda state, item=item: state.has(item, world.player))


def slot_data(world) -> dict:
    return {"bug_availability": world.options.bug_availability.value, "bug_passes": world.bug_passes}


# Unused hooks (world.py calls every hook of every feature)
LOCATIONS: dict[str, int] = {}


def locations(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []
