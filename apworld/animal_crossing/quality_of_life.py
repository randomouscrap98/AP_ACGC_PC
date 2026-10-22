# Quality of Life options plus Shops Always Open and Turnips Never Spoil. Logic reads the last two where it matters
# (timesanity.shop_open_rule, the Ant rule in museumsanity.py); the rest only go to the client. The QoL group's
# values only seed the save's Settings > AP tab (ap_settings.ini) on its first load.
def slot_data(world) -> dict:
    o = world.options
    return {
        "no_cockroaches": o.no_cockroaches.value,
        "shops_always_open": o.shops_always_open.value,
        "no_weeds": o.no_weeds.value,
        "more_favors": o.more_favors.value,
        "villagers_dont_leave": o.villagers_dont_leave.value,
        "villagers_never_sleep": o.villagers_never_sleep.value,
        "turnips_never_spoil": o.turnips_never_spoil.value,
        "no_falling_stalks": o.no_falling_stalks.value,
        "no_fishing_trash": o.no_fishing_trash.value,
        "easier_critter_catch": o.easier_critter_catch.value,
        "hidden_bug_hints": o.hidden_bug_hints.value,
        "hidden_item_hints": o.hidden_item_hints.value,
        "skip_hra_talk": o.skip_hra_talk.value,
        "short_shop_talk": o.short_shop_talk.value,
        "shop_never_sells_out": o.shop_never_sells_out.value,
        "coconuts_wash_up": o.coconuts_wash_up.value,
        "normalized_time_travel": o.normalized_time_travel.value,
        "critter_odds": o.critter_odds.value,
        "fossil_odds": o.fossil_odds.value,
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
