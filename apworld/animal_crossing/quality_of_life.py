# Quality of Life options plus Shops Always Open, Turnips Never Spoil and Global Storage. Logic reads the first two
# where it matters (timesanity.shop_open_rule, the Ant rule in museumsanity.py); the rest only go to the client.
# Global Storage is per seed only (not in the Settings > AP tab). The QoL group's
# values only seed the save's Settings > AP tab (ap_settings.ini) on its first load.
# No cockroaches, no fishing trash, easier critter catch, hidden bug/item hints and short shop talk are game settings
# since 0.5.3 (Settings > Gameplay): their yaml options are ignored and not in slot_data.
def slot_data(world) -> dict:
    o = world.options
    return {
        "shops_always_open": o.shops_always_open.value,
        "global_storage": o.global_storage.value,
        "no_weeds": o.no_weeds.value,
        "more_favors": o.more_favors.value,
        "villagers_dont_leave": o.villagers_dont_leave.value,
        "villagers_never_sleep": o.villagers_never_sleep.value,
        "turnips_never_spoil": o.turnips_never_spoil.value,
        "no_falling_stalks": o.no_falling_stalks.value,
        "skip_hra_talk": o.skip_hra_talk.value,
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
