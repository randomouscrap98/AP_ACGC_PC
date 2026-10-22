# Intro: Skip Intro (no train / Nook job, start by the house with a map) and the player's identity from options.
# The starting shirt applies either way.
NAME_MAX = 8  # PLAYER_NAME_LEN / LAND_NAME_SIZE in the game
DEFAULT_TOWN = "Archi"
DEFAULT_PLAYER_NAME = "-"  # a blank name breaks the game's name compares (no one is ever "you")


def clamp_name(text: str, max_len: int = NAME_MAX) -> str:
    # The client does the real charset conversion; keep printable ASCII and the length here.
    text = "".join(c for c in text if " " <= c <= "~")
    return text.strip()[:max_len]


def setup(world) -> None:
    world.ac_player_name = (clamp_name(world.options.player_name.value) or clamp_name(world.player_name)
                            or DEFAULT_PLAYER_NAME)
    world.ac_town_name = clamp_name(world.options.town_name.value) or DEFAULT_TOWN


def slot_data(world) -> dict:
    o = world.options
    return {
        "skip_intro": o.skip_intro.value,
        "player_name": world.ac_player_name,
        "town_name": world.ac_town_name,
        "gender": o.gender.value,
        "face": o.face.value,
        "starting_shirt": o.starting_shirt.value,
        "house": o.house.value,
    }


# Unused hooks (world.py calls every hook of every feature)
ITEMS: dict[str, int] = {}
LOCATIONS: dict[str, int] = {}


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
