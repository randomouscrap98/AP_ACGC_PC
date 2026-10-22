# Museumsanity: per category (bugs, fish, fossils; paintings not yet) a check mode: find (catch / dig up), donate,
# or both. With the Museum goal each museum thing is also an AP item ("Museum: X") that fills its museum spot, and
# Blathers only takes a thing while its donate check is unsent; the goal is Museum Goal Percent of these items.
# Without the goal there are no donation items: the museum is vanilla and donating sends the donate check.
# The museum things are generated (museum_data.py).
# WARN: the client mirrors the ids and the slot numbering (pc_ap_museumsanity.h)!
import math

from BaseClasses import ItemClassification
from worlds.generic.Rules import add_rule

from . import timesanity
from .museum_data import BUGS, FISH, FOSSILS, PAINTINGS
from .options import CHECK_DONATE, CHECK_FIND

# Categories in the game's order (mMmd_CATEGORY_*), things in museum index order.
MUSEUM_FOSSIL, MUSEUM_PAINTING, MUSEUM_BUG, MUSEUM_FISH = range(4)
MUSEUM_NAMES = [FOSSILS, PAINTINGS, [name for name, _ in BUGS], [name for name, _ in FISH]]

# Museum slot = category * 0x40 + index: the same number for the item and both of its locations.
MUSEUM_ITEM_BASE_ID = 0x10100    # "Museum: X"
MUSEUM_DONATE_BASE_ID = 0x30000  # "Donate X"
MUSEUM_FIND_BASE_ID = 0x30100    # "Catch X" / "Dig up X"
# Categories with find locations. Paintings have none yet (room left for "Buy").
FIND_VERBS = {MUSEUM_FOSSIL: "Dig up", MUSEUM_BUG: "Catch", MUSEUM_FISH: "Catch"}


def museum_slot(category: int, index: int) -> int:
    return category * 0x40 + index


def museum_item_name(name: str) -> str:
    return f"Museum: {name}"


def museum_donate_name(name: str) -> str:
    return f"Donate {name}"


def museum_find_name(category: int, name: str) -> str:
    # KeyError for a category without find locations (paintings)
    return f"{FIND_VERBS[category]} {name}"


ITEMS = {}
LOCATIONS = {}
for _c, _names in enumerate(MUSEUM_NAMES):
    for _i, _name in enumerate(_names):
        _slot = museum_slot(_c, _i)
        ITEMS[museum_item_name(_name)] = MUSEUM_ITEM_BASE_ID + _slot
        LOCATIONS[museum_donate_name(_name)] = MUSEUM_DONATE_BASE_ID + _slot
        if _c in FIND_VERBS:
            LOCATIONS[museum_find_name(_c, _name)] = MUSEUM_FIND_BASE_ID + _slot


def setup(world) -> None:
    o = world.options
    # Check mode per category (CHECK_* bits), 0 = vanilla, no items. All 0 without Museumsanity.
    world.museum_modes = {MUSEUM_FOSSIL: 0, MUSEUM_PAINTING: 0, MUSEUM_BUG: 0, MUSEUM_FISH: 0}
    if o.museumsanity:
        world.museum_modes = {
            MUSEUM_FOSSIL: o.fossil_checks.value,
            MUSEUM_PAINTING: o.painting_checks.value,
            MUSEUM_BUG: o.bug_checks.value,
            MUSEUM_FISH: o.fish_checks.value,
        }
        if not any(world.museum_modes.values()):
            raise world.option_error("Museumsanity needs at least one of Bug, Fish or Fossil Checks on.")
    # Museum goal: one donation item per thing in each category with checks. Locations per mode bit.
    with_items = "Museum" in o.goal.value
    world.museum_item_names = []
    world.museum_locations = []
    for c, names in enumerate(MUSEUM_NAMES):
        for name in names if world.museum_modes[c] else []:
            if with_items:
                world.museum_item_names.append(museum_item_name(name))
            if world.museum_modes[c] & CHECK_FIND:
                world.museum_locations.append(museum_find_name(c, name))
            if world.museum_modes[c] & CHECK_DONATE:
                world.museum_locations.append(museum_donate_name(name))

    world.museum_goal_count = math.ceil(len(world.museum_item_names) * o.museum_goal_percent.value / 100)
    if world.ut is not None and "museum_goal_count" in world.ut:
        world.museum_goal_count = world.ut["museum_goal_count"]
    # Museum goal: only the donation items it needs are progression (picked at random), the rest are useful.
    # The client counts every donation item received, so a useful one still counts in game.
    world.museum_progression = set(world.random.sample(world.museum_item_names, world.museum_goal_count))
    if world.ut is not None and "museum_progression" in world.ut:
        world.museum_progression = {world.item_id_to_name[item_id] for item_id in world.ut["museum_progression"]}


def locations(world) -> list[str]:
    return world.museum_locations


def items(world) -> list[str]:
    return world.museum_item_names


def classify(world, name: str):
    if name not in ITEMS:
        return None
    if name in world.museum_progression:
        return ItemClassification.progression
    return ItemClassification.useful


def add_museum_rule(world, category: int, name: str, rule) -> None:
    # On the find and donate locations of one museum thing, the ones that exist. Other features use it too
    # (availability, tools).
    assert name in MUSEUM_NAMES[category], f"not a museum name: {name}"
    for location in (museum_find_name(category, name), museum_donate_name(name)):
        if location in world.museum_locations:
            add_rule(world.get_location(location), rule)


def rules(world) -> None:
    # Bugs and fish (catch and donate): a month and a time slot it spawns in
    if not world.options.timesanity:
        return
    for c, critters in ((MUSEUM_BUG, BUGS), (MUSEUM_FISH, FISH)):
        for name, when in critters:
            add_museum_rule(world, c, name, timesanity.spawn_rule(world, when))
    shop = timesanity.shop_open_rule(world)
    # Ants spawn on spoiled turnips or candy. With turnips never spoiling only candy is left, sold by Nook
    # Oct 16-30: needs October and a slot with an open shop
    if world.options.turnips_never_spoil:
        add_museum_rule(world, MUSEUM_BUG, "ant", timesanity.month_rule(world, ["October"]))
        if shop:
            add_museum_rule(world, MUSEUM_BUG, "ant", shop)
    # Snails spawn on flowers (in the rain). New towns have none and only Nook sells seeds, so it needs a slot
    # with an open shop. Ladybugs and the Mantis only spawn in Day Hours: already covered.
    if shop:
        add_museum_rule(world, MUSEUM_BUG, "snail", shop)


def museum_goal(world):
    return lambda state: state.has_from_list(world.museum_item_names, world.player, world.museum_goal_count)


def slot_data(world) -> dict:
    return {
        "museumsanity": world.options.museumsanity.value,
        # Effective modes: 0 without Museumsanity
        "bug_checks": world.museum_modes[MUSEUM_BUG],
        "fish_checks": world.museum_modes[MUSEUM_FISH],
        "fossil_checks": world.museum_modes[MUSEUM_FOSSIL],
        "painting_checks": world.museum_modes[MUSEUM_PAINTING],
        "museum_goal_count": world.museum_goal_count,
        # Museum donation items that are progression (Museum goal); for Universal Tracker
        "museum_progression": sorted(ITEMS[name] for name in world.museum_progression),
    }


# Unused hooks (world.py calls every hook of every feature)
def precollected(world) -> list[str]:
    return []
