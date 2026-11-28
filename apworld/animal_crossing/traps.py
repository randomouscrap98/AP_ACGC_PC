# Traps / DeathLink: Trap Percent of the seed's items are traps, filler items that hurt when they arrive (they sit
# in anyone's locations and always hit you; Animal Crossing can't send one). They take the slots after Fruit Filler
# and the Extra Bells bags, before the Bell Credits, and never more than 3/4 of them: the credits keep at least 25%
# (rounded up). Trap Weights picks the kinds (0 = never, all 0 = no traps).
# DeathLink is AP's own option; a received death applies one trap picked by Death Link Trap Weights (all 0 = only the
# toast). Accumulate Resetti: trap and Death Link Resettis add to the permanent reset count.
# The traps themselves are item ids only (the client does the hurting): see research/traps.md.
# WARN: the client mirrors the ids, the count and the order (apclient/include/ap_slotdata.h AP_TRAP_NUM)!
from BaseClasses import ItemClassification

TRAP_ID_BASE = 0x10380  # trap t = base + t; 32 ids reserved (0x10380-0x1039F)
# The option keys, in id order (the client's death_link_trap_weights list is in this order)
TRAP_NAMES = ("Bee Trap", "Gyroid Face Trap", "Resetti Trap", "Pitfall Trap", "Weed Trap", "Cockroach Trap",
              "Color Trap")
ITEMS = {name: TRAP_ID_BASE + t for t, name in enumerate(TRAP_NAMES)}


def weights(world, option) -> list[int]:
    # Per trap in TRAP_NAMES order. A key missing from the yaml is 0: the option only lists what it wants
    return [max(0, option.value.get(name, 0)) for name in TRAP_NAMES]


def count(world, free: int) -> int:
    # Trap Percent of the seed's locations, but never more than 3/4 of the free filler slots left after the bags
    # (the Bell Credits keep at least 25%, rounded up) and never more than there are
    percent = world.options.trap_percent.value
    if percent <= 0 or free <= 0:
        return 0
    return min(round(len(world.seed_locations()) * percent / 100), free * 3 // 4)


def pick(world, weights: list[int], n: int) -> list[str]:
    # n traps by weight; all weights 0 = none (Trap Percent has nothing to pick from then)
    if n <= 0 or sum(weights) <= 0:
        return []
    return world.random.choices(TRAP_NAMES, weights=weights, k=n)


def filler_items(world, free: int) -> list[str]:
    # The trap slots, after Fruit Filler and the Extra Bells bags (world.create_items). Universal Tracker: the real
    # seed's traps (slot_data trap_counts), not a new roll
    if world.ut is not None and "trap_counts" in world.ut:
        world.trap_items = [name for name, n in zip(TRAP_NAMES, world.ut["trap_counts"]) for _ in range(n)][:free]
    else:
        world.trap_items = pick(world, weights(world, world.options.trap_weights), count(world, free))
    return world.trap_items


def classify(world, name: str):
    if name in ITEMS:
        return ItemClassification.trap
    return None


def slot_data(world) -> dict:
    return {
        "death_link": world.options.death_link.value,
        "death_link_trap_weights": weights(world, world.options.death_link_trap_weights),
        "accumulate_resetti": world.options.accumulate_resetti.value,
        # Universal Tracker only (the client takes traps as they arrive): traps per kind in TRAP_NAMES order
        "trap_counts": [getattr(world, "trap_items", []).count(name) for name in TRAP_NAMES],
    }


# Unused hooks (world.py calls every hook of every feature)
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
