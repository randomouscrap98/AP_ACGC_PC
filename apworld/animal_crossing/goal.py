# Goal: every chosen goal is required. Each goal's rule lives with its feature; here they're checked and combined.
# Goals are only sent when K.K. Slider plays (Saturday night), so with Timesanity a slot with his hours is needed.
from . import catalogsanity, furniture_sets, loansanity, museumsanity, timesanity


def setup(world) -> None:
    goals = world.options.goal.value
    if not goals:
        raise world.option_error("Goal needs at least one goal.")
    if "Statue" in goals and not world.options.loansanity:
        raise world.option_error("The Statue goal needs Loansanity on.")
    if "Museum" in goals and not world.options.museumsanity:
        raise world.option_error("The Museum goal needs Museumsanity on.")
    if "Catalog" in goals and not world.options.catalogsanity:
        raise world.option_error("The Catalog goal needs Catalogsanity on.")


def froggy_chair_goal(world):
    # No AP item gates it: Nook's shop, trees, villagers, ... can all hand it out
    return lambda state: True


GOALS = {
    "Statue": loansanity.statue_goal,
    "Museum": museumsanity.museum_goal,
    "Froggy Chair": froggy_chair_goal,
    "Catalog": catalogsanity.catalog_goal,
    furniture_sets.FURNITURE_SET_GOAL: furniture_sets.furniture_set_goal,
    furniture_sets.CHESS_SET_GOAL: furniture_sets.chess_set_goal,
}


def rules(world) -> None:
    chosen = [GOALS[name](world) for name in sorted(world.options.goal.value)]
    kk = timesanity.kk_rule(world)
    if kk:
        chosen.append(kk)
    world.multiworld.completion_condition[world.player] = lambda state: all(goal(state) for goal in chosen)


def slot_data(world) -> dict:
    return {"goal": sorted(world.options.goal.value)}


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
