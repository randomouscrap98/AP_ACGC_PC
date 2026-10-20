# Treesanity: a bearing fruit tree drops AP items in place of its fruit while that fruit has unsent checks; picking
# one up sends the next unsent "Tree: <Fruit> k". Tree Checks per Fruit 1-20. The 4 non-town fruit are progression
# items (planting one grows that fruit's trees); the town fruit's trees are there from the start. No coconuts.
# Fruit Filler reads this: with Treesanity on, the fruit are in the pool from here, not there.
# WARN: the client mirrors the ids and the max (PC_AP_LOC_TREE_BASE, AP_TREE_PER_FRUIT_MAX)!
from BaseClasses import ItemClassification
from worlds.generic.Rules import add_rule

from .fruit_filler import FRUITS

TREE_PER_FRUIT_MAX = 20  # Tree Checks per Fruit max
TREE_BASE_ID = 0x60000  # fruit f (Town Fruit value: apple, cherry, pear, peach, orange), check k (1-based): base + f * 0x100 + k


def tree_location_name(fruit: int, check: int) -> str:
    return f"Tree: {FRUITS[fruit].capitalize()} {check}"


LOCATIONS = {tree_location_name(f, k): TREE_BASE_ID + f * 0x100 + k
             for f in range(len(FRUITS)) for k in range(1, TREE_PER_FRUIT_MAX + 1)}


def per_fruit(world) -> int:
    return world.options.tree_checks_per_fruit.value if world.options.treesanity else 0


def non_town_fruit(world) -> list[str]:
    return [name for value, name in enumerate(FRUITS) if value != world.options.town_fruit.value]


def locations(world) -> list[str]:
    return [tree_location_name(f, k) for f in range(len(FRUITS)) for k in range(1, per_fruit(world) + 1)]


def items(world) -> list[str]:
    return non_town_fruit(world) if world.options.treesanity else []


def classify(world, name: str):
    if world.options.treesanity and name in non_town_fruit(world):
        return ItemClassification.progression
    return None


def rules(world) -> None:
    # A non-town fruit's trees only grow once it's received (and planted)
    for f in range(len(FRUITS)):
        if f == world.options.town_fruit.value:
            continue
        for k in range(1, per_fruit(world) + 1):
            add_rule(world.get_location(tree_location_name(f, k)),
                     lambda state, name=FRUITS[f]: state.has(name, world.player))


def slot_data(world) -> dict:
    return {
        "treesanity": world.options.treesanity.value,
        "tree_checks_per_fruit": world.options.tree_checks_per_fruit.value,
    }
