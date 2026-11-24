# Beach hints: an AP Scroll sometimes washes up on the town beach; picking it up asks the server for a real hint on
# one of the player's own unchecked, unhinted locations. Beach Hint Percentage 0-100 (0 = off) of the player's
# locations, fixed at generation. The client mirrors the slot_data keys (ap_slotdata.h) and the scroll item id.
def count(world) -> int:
    percentage = world.options.beach_hint_percentage.value
    if percentage <= 0:
        return 0
    return round(percentage / 100 * len(world.seed_locations()))


def slot_data(world) -> dict:
    # Universal Tracker regenerates from the real seed's slot_data: use its fixed count (old seeds: 0), not a new
    # roll from today's default percentage.
    if world.ut is not None:
        return {
            "beach_hint_count": world.ut.get("beach_hint_count", 0),
            "beach_hint_letter": world.ut.get("beach_hint_letter", 0),
        }
    return {
        "beach_hint_count": count(world),
        "beach_hint_letter": world.options.beach_hint_gives_letter.value,
    }


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


def classify(world, name: str):
    return None


def rules(world) -> None:
    pass
