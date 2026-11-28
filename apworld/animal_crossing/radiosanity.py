# Radiosanity: K.K. Slider songs are AP items. Receiving one puts the aircheck in your home's music box (no pocket
# item, no mail); playing it for 2 seconds on a music player in your own house sends its "Aircheck: <Song>" check.
# Radiosanity Song Count 1-55: the seed's random subset of the 55 songs (listed in slot_data) is what becomes items
# and checks. Radiosanity Aircheck (out of logic): the same check also sends when the song plays in a villager's
# house. After a show K.K. hands out no song for a subset song (vanilla for the songs outside it).
# Also the Music goal: Music Goal Percent of the subset listened to.
# WARN: the client mirrors the ids and the song count (pc/include/pc_ap_radiosanity.h, AP_RADIO_SONG_NUM)!
import math

from BaseClasses import ItemClassification
from worlds.generic.Rules import add_rule

from . import timesanity
from .game_items import ITEM_NAME_TO_ID as GAME_ITEMS

SONG_COUNT_MAX = 55  # ITM_MINIDISK00-54 (the game's MINIDISK_NUM), the 3 secret songs included
RADIO_BASE_ID = 0x70000  # song s (0-54): "Aircheck: <Song>" = base + s
RADIO_ITEM_ID_BASE = 0x10340  # song s: "Radio: <Song>" = base + s (55 ids, after the Bell Credits)
MINIDISK_START = 0x2A00  # ITM_MINIDISK_START
# Song index (0-54) -> the game's own item name (game_items.py: the minidisk items in id order)
SONG_NAMES = [name for name, item_id in sorted(GAME_ITEMS.items(), key=lambda kv: kv[1])
              if MINIDISK_START <= item_id < MINIDISK_START + SONG_COUNT_MAX]
assert len(SONG_NAMES) == SONG_COUNT_MAX, "minidisk items missing from game_items.py"


def radio_item_name(song: int) -> str:
    # "Radio: <name>" for the song item, "Aircheck: <name>" for its check (below), so the two read apart in the
    # spoiler and the logs; the game's own item name would double up ("K.K. K.K. Blues")
    return "Radio: " + SONG_NAMES[song]


def radio_location_name(song: int) -> str:
    return "Aircheck: " + SONG_NAMES[song]


ITEMS = {radio_item_name(s): RADIO_ITEM_ID_BASE + s for s in range(SONG_COUNT_MAX)}
LOCATIONS = {radio_location_name(s): RADIO_BASE_ID + s for s in range(SONG_COUNT_MAX)}


def songs(world) -> list[int]:
    # The seed's subset, in song order (empty with Radiosanity off)
    return world.radio_songs if world.options.radiosanity else []


def setup(world) -> None:
    count = world.options.radiosanity_song_count.value if world.options.radiosanity else 0
    world.radio_songs = sorted(world.random.sample(range(SONG_COUNT_MAX), count))
    if world.ut is not None and "radiosanity_songs" in world.ut:
        world.radio_songs = list(world.ut["radiosanity_songs"])
    world.radio_goal_count = math.ceil(len(world.radio_songs) * world.options.music_goal_percent.value / 100)
    if world.ut is not None and "radio_goal_count" in world.ut:
        world.radio_goal_count = world.ut["radio_goal_count"]


def locations(world) -> list[str]:
    return [radio_location_name(s) for s in songs(world)]


def items(world) -> list[str]:
    return [radio_item_name(s) for s in songs(world)]


def classify(world, name: str):
    # Always progression, Music goal or not: a song is the only way to its own radio check, so a "useful" song isn't
    # in the fill's exploration state and its check can't hold another song (generation fails with "no more spots"
    # once there are more songs than checks that don't need one).
    if name in [radio_item_name(s) for s in songs(world)]:
        return ItemClassification.progression
    return None


def rules(world) -> None:
    # A song is listened to on a music player at home: the house starts with one, and a trashed one is re-ordered
    # from the catalog, which needs the shop (the rule other "buy it again" checks use)
    shop = timesanity.shop_open_rule(world)
    for s in songs(world):
        def rule(state, name=radio_item_name(s), shop=shop):
            return state.has(name, world.player) and (shop is None or shop(state))
        add_rule(world.get_location(radio_location_name(s)), rule)


def music_goal(world):
    # Music goal: Music Goal Percent of the subset listened to, so that many song items
    return lambda state: state.has_from_list([radio_item_name(s) for s in songs(world)], world.player,
                                             world.radio_goal_count)


def slot_data(world) -> dict:
    return {
        "radiosanity": world.options.radiosanity.value,
        "radiosanity_aircheck": world.options.radiosanity_aircheck.value,
        "radiosanity_song_count": world.options.radiosanity_song_count.value,
        "radiosanity_songs": songs(world),
        "radio_goal_count": world.radio_goal_count,
    }


# Unused hooks (world.py calls every hook of every feature)
def precollected(world) -> list[str]:
    return []
