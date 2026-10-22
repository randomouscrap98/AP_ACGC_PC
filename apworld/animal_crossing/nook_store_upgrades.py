# Nook Store Upgrades: how Nook's store grows (one level per renovation, never needs a visitor). Progressive (3
# Progressive Nook Store items, useful), Sales (total spent) or Catalog (catalog entries), with settable
# thresholds.
# WARN: the client mirrors the id and the level count (PC_AP_ITEM_PROGRESSIVE_NOOK_STORE, AP_STORE_LEVELS)!
from BaseClasses import ItemClassification

from . import options

PROGRESSIVE_NOOK_STORE = "Progressive Nook Store"
ITEMS = {PROGRESSIVE_NOOK_STORE: 0x10004}
NOOK_STORE_UPGRADES = 3  # Nook 'n' Go, Nookway, Nookington's


def setup(world) -> None:
    o = world.options
    # Progressive Nook Store items, or the thresholds of the chosen mode
    world.store_items = 0
    world.store_thresholds = []
    mode = o.nook_store_upgrades.value
    if mode == options.NookStoreUpgrades.option_progressive:
        world.store_items = NOOK_STORE_UPGRADES
    elif mode == options.NookStoreUpgrades.option_sales:
        world.store_thresholds = [o.nook_n_go_sales.value, o.nookway_sales.value, o.nookingtons_sales.value]
    else:
        world.store_thresholds = [o.nook_n_go_catalog.value, o.nookway_catalog.value, o.nookingtons_catalog.value]


def items(world) -> list[str]:
    return [PROGRESSIVE_NOOK_STORE] * world.store_items


def classify(world, name: str):
    return ItemClassification.useful if name == PROGRESSIVE_NOOK_STORE else None


def slot_data(world) -> dict:
    return {
        "nook_store_upgrades": world.options.nook_store_upgrades.value,
        # Bells (sales) or catalog entries (catalog) per upgrade; empty with progressive
        "nook_store_thresholds": world.store_thresholds,
    }


# Unused hooks (world.py calls every hook of every feature)
LOCATIONS: dict[str, int] = {}


def locations(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []


def rules(world) -> None:
    pass
