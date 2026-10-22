# Timesanity: the clock is frozen; months and the 4 time slots are items. The starting month and slot are given
# at the start. Also the rule helpers other features use for "when can this be done" (they return None when the
# clock isn't gated, so callers skip the rule).
# WARN: the client mirrors the ids (PC_AP_ITEM_MONTH_BASE / _SLOT_BASE) and the slot hours!
import datetime

from BaseClasses import ItemClassification

# One item per month and per time slot (Morning 4-8, Day 9-15, Evening 16-20, Night 21-3)
MONTHS = ["January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December"]
TIME_SLOTS = ["Morning Hours", "Day Hours", "Evening Hours", "Night Hours"]
MONTH_ID_BASE = 0x10010  # month m (0-11) = base + m
TIME_SLOT_ID_BASE = 0x10020  # slot s (0-3) = base + s
ITEMS = {name: MONTH_ID_BASE + m for m, name in enumerate(MONTHS)}
ITEMS.update({name: TIME_SLOT_ID_BASE + s for s, name in enumerate(TIME_SLOTS)})

# No shop is surely open in Morning (only Nook 'n' Go, from 7); every other slot has at least one open hour
# (Night: 21)
SHOP_SLOTS = ("Day Hours", "Evening Hours", "Night Hours")
# K.K. Slider plays Saturday 20:00-23:59
KK_SLOTS = ("Evening Hours", "Night Hours")
MIN_YEAR, MAX_YEAR = 2001, 2100  # mTM_MIN_YEAR / mTM_MAX_YEAR (PC)


def _start_items(world) -> tuple[str, str]:
    return MONTHS[world.options.starting_month.value], TIME_SLOTS[world.options.starting_time.value]


def items(world) -> list[str]:
    if not world.options.timesanity:
        return []
    start = _start_items(world)
    return [name for name in MONTHS + TIME_SLOTS if name not in start]


def precollected(world) -> list[str]:
    return list(_start_items(world)) if world.options.timesanity else []


def classify(world, name: str):
    return ItemClassification.progression if name in ITEMS else None


def slot_data(world) -> dict:
    return {
        "timesanity": world.options.timesanity.value,
        "starting_month": world.options.starting_month.value,
        "starting_time": world.options.starting_time.value,
        # Timesanity clock year: the year the seed was made, fixed for the whole save
        "start_year": min(max(datetime.date.today().year, MIN_YEAR), MAX_YEAR),
    }


# Rule helpers for other features. None = no rule (Timesanity off, or the option makes it always doable).

def shop_open_rule(world):
    # Selling or buying needs an open shop: a slot with shop hours, unless Shops Always Open
    if not world.options.timesanity or world.options.shops_always_open:
        return None
    return lambda state: state.has_any(SHOP_SLOTS, world.player)


def month_rule(world, months: list[str]):
    # Any of these months
    if not world.options.timesanity:
        return None
    return lambda state: state.has_any(months, world.player)


def kk_rule(world):
    # K.K. Slider's show: a slot with his hours, any month
    if not world.options.timesanity:
        return None
    return lambda state: state.has_any(KK_SLOTS, world.player)


def spawn_rule(world, when: dict[int, tuple[int, ...]]):
    # when: {month: time slots} (museum_data.py); any owned month with an owned slot of it
    if not world.options.timesanity:
        return None
    pairs = [(MONTHS[m], [TIME_SLOTS[s] for s in slots]) for m, slots in when.items()]
    return lambda state: any(state.has(month, world.player) and state.has_any(slots, world.player)
                             for month, slots in pairs)


# Unused hooks (world.py calls every hook of every feature)
LOCATIONS: dict[str, int] = {}


def setup(world) -> None:
    pass


def locations(world) -> list[str]:
    return []


def rules(world) -> None:
    pass
