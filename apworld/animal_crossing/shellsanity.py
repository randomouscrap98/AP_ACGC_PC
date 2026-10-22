# Shellsanity: town shells sometimes wash up as AP items for the current month; picking one up sends the next
# unsent "Beach: <Month> k". Beach Checks per Month 1-20.
# WARN: the client mirrors the ids and the max (PC_AP_LOC_BEACH_BASE, AP_BEACH_PER_MONTH_MAX)!
from worlds.generic.Rules import add_rule

from . import timesanity
from .timesanity import MONTHS

BEACH_PER_MONTH_MAX = 20  # Beach Checks per Month max
BEACH_BASE_ID = 0x50000  # month m (0-11), check k (1-based): BEACH_BASE_ID + m * 0x100 + k


def beach_location_name(month: int, check: int) -> str:
    return f"Beach: {MONTHS[month]} {check}"


LOCATIONS = {beach_location_name(m, k): BEACH_BASE_ID + m * 0x100 + k
             for m in range(len(MONTHS)) for k in range(1, BEACH_PER_MONTH_MAX + 1)}


def per_month(world) -> int:
    return world.options.beach_checks_per_month.value if world.options.shellsanity else 0


def locations(world) -> list[str]:
    return [beach_location_name(m, k) for m in range(len(MONTHS)) for k in range(1, per_month(world) + 1)]


def rules(world) -> None:
    # An item washes up for the current month only
    for m in range(len(MONTHS)):
        month = timesanity.month_rule(world, [MONTHS[m]])
        if month:
            for k in range(1, per_month(world) + 1):
                add_rule(world.get_location(beach_location_name(m, k)), month)


def slot_data(world) -> dict:
    return {
        "shellsanity": world.options.shellsanity.value,
        "beach_checks_per_month": world.options.beach_checks_per_month.value,
        "shellsanity_item_chance": world.options.shellsanity_item_chance.value,
    }


# Unused hooks (world.py calls every hook of every feature)
ITEMS: dict[str, int] = {}


def setup(world) -> None:
    pass


def items(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []


def classify(world, name: str):
    return None
