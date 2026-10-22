# Tools in Pool: the net, fishing rod and shovel are AP items (not sold; lost & found gives a sapling; contests
# need the rod/net). Catching, digging up and donating need the tool. Also Starting Tool (a freebie, mailed at the
# start, also without Tools in Pool). The tools are the game's own items (game_items.py).
# Reads: world.museum_modes (museumsanity.setup; classify only, so the setup order doesn't matter).
from BaseClasses import ItemClassification

from . import options
from .museumsanity import MUSEUM_BUG, MUSEUM_FISH, MUSEUM_FOSSIL, MUSEUM_NAMES, add_museum_rule

NET, FISHING_ROD, SHOVEL = "net", "fishing rod", "shovel"
TOOLS = [NET, FISHING_ROD, SHOVEL]  # StartingTool option values 1-3, in order
# The tool each museum category needs
CATEGORY_TOOLS = {MUSEUM_BUG: NET, MUSEUM_FISH: FISHING_ROD, MUSEUM_FOSSIL: SHOVEL}
# Bugs that also need the shovel: the mole cricket is dug up, the pill bug hides under a rock (hit it)
SHOVEL_BUGS = ["mole cricket", "pill bug"]


def setup(world) -> None:
    world.starting_tool = None
    choice = world.options.starting_tool.value
    if choice == options.StartingTool.option_random_tool:
        world.starting_tool = world.random.choice(TOOLS)
    elif choice != options.StartingTool.option_none:
        world.starting_tool = TOOLS[choice - 1]
    if world.ut is not None and "starting_tool" in world.ut:
        world.starting_tool = world.ut["starting_tool"]


def items(world) -> list[str]:
    if not world.options.tools_in_pool:
        return []
    return [tool for tool in TOOLS if tool != world.starting_tool]


def precollected(world) -> list[str]:
    return [world.starting_tool] if world.starting_tool else []


def classify(world, name: str):
    if name not in TOOLS:
        return None
    # Progression when a museum category with checks needs it (only with Tools in Pool); bugs need the shovel too,
    # and so do the loans after the first (loansanity.fossil_money_rule) and planting non-town fruit (treesanity.rules)
    categories = [c for c, tool in CATEGORY_TOOLS.items() if tool == name] + ([MUSEUM_BUG] if name == SHOVEL else [])
    later_loans = world.options.loansanity and world.options.fossil_loans
    needed = (any(world.museum_modes.get(c) for c in categories)
              or (name == SHOVEL and (later_loans or world.options.treesanity)))
    needed = world.options.tools_in_pool and needed
    return ItemClassification.progression if needed else ItemClassification.useful


def rules(world) -> None:
    # Catching, digging up and donating need the tool (on top of the other museum rules)
    if world.options.tools_in_pool:
        for c, tool in CATEGORY_TOOLS.items():
            for name in MUSEUM_NAMES[c]:
                add_museum_rule(world, c, name, lambda state, tool=tool: state.has(tool, world.player))
        for name in SHOVEL_BUGS:
            add_museum_rule(world, MUSEUM_BUG, name, lambda state: state.has(SHOVEL, world.player))


def slot_data(world) -> dict:
    return {
        "tools_in_pool": world.options.tools_in_pool.value,
        # Resolved starting tool (item name, None for none); for Universal Tracker
        "starting_tool": world.starting_tool,
    }


# Unused hooks (world.py calls every hook of every feature)
ITEMS: dict[str, int] = {}
LOCATIONS: dict[str, int] = {}


def locations(world) -> list[str]:
    return []
