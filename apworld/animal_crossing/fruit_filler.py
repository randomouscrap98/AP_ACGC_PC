# Fruit Filler: one of each fruit the town doesn't grow (the game's own fruit items, game_items.py), mailed by the
# client with the sender's name. Extra Bells come first, then fruit, then the Bell Credits: fruit only takes slots the
# bags can never need (one per 10,000 Bells), so the count doesn't depend on the random bag roll (Universal Tracker
# regenerates it).
from BaseClasses import ItemClassification

FRUITS = ("apple", "cherry", "pear", "peach", "orange")  # Town Fruit option values, in order


def filler_items(world, free: int) -> list[str]:
    if not world.options.fruit_filler or world.options.treesanity:  # Treesanity puts them in the pool itself
        return []
    names = [name for value, name in enumerate(FRUITS) if value != world.options.town_fruit.value]
    return names[:max(0, free - world.options.extra_bells.value)]


def classify(world, name: str):
    # Useful, not filler: wanted for an orchard
    return ItemClassification.useful if name in FRUITS else None


def slot_data(world) -> dict:
    # Universal Tracker only (the client mails whatever fruit it gets)
    return {"fruit_filler": world.options.fruit_filler.value}


# Unused hooks (world.py calls every hook of every feature)
ITEMS: dict[str, int] = {}
LOCATIONS: dict[str, int] = {}


def setup(world) -> None:
    pass


def locations(world) -> list[str]:
    return []


def items(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []


def rules(world) -> None:
    pass
