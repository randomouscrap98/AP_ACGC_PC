# Furniture Set and Chess Set goals: the pieces are AP items (mailed by the client), and nothing else hands out a
# piece before its item arrives. They leave the Catalogsanity pool. Furniture Set: every piece of the chosen base
# series (the HRA's 10-piece series, all orderable once in the catalog) somewhere in the house. Chess Set: a full
# board's count of each piece (one item per kind; the rest are ordered from the catalog). Both need the basement
# and an open shop (to order pieces again if one is lost).
# The pieces are the game's own items (game_items.py); the series come from mMkRm_ftr_info (m_mark_room_ovl.c).
# Reads: nothing. catalogsanity.setup reads world.goal_pieces (run this setup first).
from BaseClasses import ItemClassification

from . import loansanity, timesanity
from .game_items import ITEM_NAME_TO_ID as GAME_ITEMS
from .options import FurnitureSet

FURNITURE_SET_GOAL, CHESS_SET_GOAL = "Furniture Set", "Chess Set"

# Option value order (FurnitureSet), 10 pieces each
FURNITURE_SETS = {
    "Exotic": ["exotic wardrobe", "exotic bureau", "exotic bench", "exotic chair", "exotic chest", "exotic lamp",
               "exotic screen", "exotic table", "exotic bed", "exotic end table"],
    "Lovely": ["lovely armoire", "lovely dresser", "lovely end table", "lovely armchair", "lovely lamp",
               "lovely kitchen", "lovely chair", "lovely bed", "lovely vanity", "lovely table"],
    "Classic": ["classic wardrobe", "classic vanity", "classic hutch", "classic chair", "classic desk",
                "classic table", "classic cabinet", "classic sofa", "classic clock", "classic bed"],
    "Ranch": ["ranch wardrobe", "ranch dresser", "ranch couch", "ranch armchair", "ranch tea table", "ranch hutch",
              "ranch bookcase", "ranch chair", "ranch bed", "ranch table"],
    "Cabana": ["cabana wardrobe", "cabana dresser", "cabana armchair", "cabana bed", "cabana lamp", "cabana table",
               "cabana screen", "cabana vanity", "cabana chair", "cabana bookcase"],
    "Blue": ["blue wardrobe", "blue cabinet", "blue bureau", "blue bed", "blue bench", "blue chair", "blue dresser",
             "blue bookcase", "blue table", "blue clock"],
    "Modern": ["modern wardrobe", "modern dresser", "modern bed", "modern cabinet", "modern desk", "modern sofa",
               "modern table", "modern chair", "modern end table", "modern lamp"],
    "Regal": ["regal armoire", "regal dresser", "regal cupboard", "regal chair", "regal table", "regal bookcase",
              "regal bed", "regal vanity", "regal sofa", "regal lamp"],
    "Green": ["green wardrobe", "green dresser", "green bed", "green bench", "green chair", "green pantry",
              "green counter", "green lamp", "green table", "green desk"],
    "Cabin": ["cabin wardrobe", "cabin dresser", "cabin clock", "cabin bed", "cabin couch", "cabin armchair",
              "cabin bookcase", "cabin low table", "cabin chair", "cabin table"],
    "Kiddie": ["kiddie dresser", "kiddie bureau", "kiddie wardrobe", "kiddie clock", "kiddie bed", "kiddie table",
               "kiddie couch", "kiddie stereo", "kiddie chair", "kiddie bookcase"],
}
# WARN: the client mirrors this (pc_ap_furniture.c)! Pieces needed for a board, per kind
CHESS_PIECES = {
    "white rook": 2, "black rook": 2, "white queen": 1, "black queen": 1, "white bishop": 2, "black bishop": 2,
    "white king": 1, "black king": 1, "white knight": 2, "black knight": 2, "white pawn": 8, "black pawn": 8,
}
assert len(FURNITURE_SETS) == len(FurnitureSet.name_lookup), "FurnitureSet options vs FURNITURE_SETS"


def setup(world) -> None:
    goals = world.options.goal.value
    # Option value -> set name (option keys are the lowercase names)
    world.furniture_set = None
    if FURNITURE_SET_GOAL in goals:
        world.furniture_set = list(FURNITURE_SETS)[world.options.furniture_set.value]
    world.goal_pieces = []
    if world.furniture_set:
        world.goal_pieces += FURNITURE_SETS[world.furniture_set]
    if CHESS_SET_GOAL in goals:
        world.goal_pieces += list(CHESS_PIECES)


def items(world) -> list[str]:
    return list(world.goal_pieces)


def classify(world, name: str):
    return ItemClassification.progression if name in world.goal_pieces else None


def catalog_excluded(world) -> set[int]:
    # Game item ids Catalogsanity must not pool
    return {GAME_ITEMS[name] for name in world.goal_pieces}


def _goal(world, pieces: list[str]):
    # The pieces, the basement (2nd Progressive House with Loansanity), and an open shop to order lost ones
    needs = [lambda state: state.has_all(pieces, world.player)]
    if world.options.loansanity:
        needs.append(lambda state: state.has(loansanity.PROGRESSIVE_HOUSE, world.player, 2))
    shop = timesanity.shop_open_rule(world)
    if shop:
        needs.append(shop)
    return lambda state: all(need(state) for need in needs)


def furniture_set_goal(world):
    return _goal(world, FURNITURE_SETS[world.furniture_set])


def chess_set_goal(world):
    return _goal(world, list(CHESS_PIECES))


def slot_data(world) -> dict:
    return {
        "furniture_set": world.options.furniture_set.value,
        # The chosen set's game item ids, empty without the Furniture Set goal (the client needs only these)
        "furniture_set_items": [GAME_ITEMS[name] for name in FURNITURE_SETS[world.furniture_set]]
        if world.furniture_set else [],
    }


# Unused hooks (world.py calls every hook of every feature)
ITEMS: dict[str, int] = {}
LOCATIONS: dict[str, int] = {}


def locations(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []


def rules(world) -> None:
    pass
