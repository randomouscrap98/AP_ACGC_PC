# Catalogsanity: N pools of M catalog items; registering an open-pool item in the catalog sends its check. No pool
# is open at the start, pool k (1-based) needs k Progressive Catalog Pool. Only Nook and the Catalog Sources hand
# out new pool items. Also the Catalog goal: Catalog Goal Percent of the pool items in the catalog.
# The eligible items are generated (catalog_data.py).
# WARN: the client mirrors the ids and the item count (pc_ap_catalogsanity.h, AP_CATALOG_MAX)!
# Reads: world.goal_pieces (furniture_sets.setup): the set goals' pieces are never pooled.
import logging
import math
import re
from random import Random

from BaseClasses import ItemClassification
from worlds.generic.Rules import add_rule

from . import furniture_sets, timesanity
from .catalog_data import CATALOG_ITEMS
from .game_items import ITEM_NAME_TO_ID as GAME_ITEMS

PROGRESSIVE_CATALOG_POOL = "Progressive Catalog Pool"
ITEMS = {PROGRESSIVE_CATALOG_POOL: 0x10005}

CATALOG_LOCATION_BASE_ID = 0x40000  # "Catalog: X" = base + game item id
CATALOG_ELIGIBLE = sum(len(ids) for ids in CATALOG_ITEMS.values())

_GAME_ID_TO_NAME = {item_id: name for name, item_id in GAME_ITEMS.items()}


def catalog_location_name(item_id: int) -> str:
    # Stationery (not eligible now) is sold in stacks of 4 ("airmail paper x4"); the entry is the paper itself
    return "Catalog: " + re.sub(r" x\d+$", "", _GAME_ID_TO_NAME[item_id])


LOCATIONS = {catalog_location_name(item_id): CATALOG_LOCATION_BASE_ID + item_id
             for ids in CATALOG_ITEMS.values() for item_id in ids}
assert len(LOCATIONS) == CATALOG_ELIGIBLE, "duplicate catalog location names"


def eligible(excluded: set[int]) -> dict[str, list[int]]:
    # CATALOG_ITEMS minus the excluded game item ids (the set goals' pieces)
    return {cat: [i for i in ids if i not in excluded] for cat, ids in CATALOG_ITEMS.items()}


def make_pools(random: Random, pools: int, size: int, excluded: set[int] = frozenset()) -> list[list[int]]:
    """
    `pools` lists of `size` game item ids, no item twice, none excluded. Categories get their share of all picks by
    size (largest remainder), and every pool gets a similar mix: picks are spread evenly over the pools.
    """
    items = eligible(excluded)
    count = sum(len(ids) for ids in items.values())
    total = pools * size
    shares = {cat: total * len(ids) / count for cat, ids in items.items()}
    counts = {cat: int(share) for cat, share in shares.items()}
    left = total - sum(counts.values())
    for cat in sorted(shares, key=lambda c: counts[c] - shares[c])[:left]:
        counts[cat] += 1

    # Pick j of n in a category sits at (j + 0.5) / n along the pool order, so each pool gets its part
    spread = []
    for cat, ids in items.items():
        picks = random.sample(ids, counts[cat])
        spread += [((j + 0.5) / len(picks), random.random(), item_id) for j, item_id in enumerate(picks)]
    order = [item_id for _, _, item_id in sorted(spread)]
    return [order[k * size:(k + 1) * size] for k in range(pools)]


def setup(world) -> None:
    o = world.options
    # Pools of game item ids; pool k (0-based) needs k + 1 Progressive Catalog Pool
    world.catalog_pools = []
    if o.catalogsanity:
        pools, size = o.catalog_pools.value, o.catalog_pool_size.value
        # The set goals' pieces aren't pooled
        excluded = furniture_sets.catalog_excluded(world)
        most = sum(len(ids) for ids in eligible(excluded).values())
        # More than fit (e.g. a set goal took more items out): smaller pools, same pool count, so a yaml that
        # fit once still generates
        if pools * size > most:
            logging.warning(f"[Animal Crossing - '{world.player_name}'] Catalog Pools x Catalog Pool Size is "
                            f"{pools * size}, at most {most} with these goals: pool size {size} lowered to "
                            f"{most // pools}.")
            size = most // pools
        world.catalog_pools = make_pools(world.random, pools, size, excluded)
    if world.ut is not None and "catalog_pools" in world.ut:
        world.catalog_pools = world.ut["catalog_pools"]
    total = sum(len(pool) for pool in world.catalog_pools)
    world.catalog_goal_count = math.ceil(total * o.catalog_goal_percent.value / 100)
    if world.ut is not None and "catalog_goal_count" in world.ut:
        world.catalog_goal_count = world.ut["catalog_goal_count"]
    # Catalog goal: pools needed to hold catalog_goal_count items, one Progressive Catalog Pool each
    world.catalog_goal_pools = 0
    held = 0
    while held < world.catalog_goal_count:
        held += len(world.catalog_pools[world.catalog_goal_pools])
        world.catalog_goal_pools += 1


def locations(world) -> list[str]:
    return [catalog_location_name(item_id) for pool in world.catalog_pools for item_id in pool]


def items(world) -> list[str]:
    return [PROGRESSIVE_CATALOG_POOL] * len(world.catalog_pools)


def classify(world, name: str):
    return ItemClassification.progression if name == PROGRESSIVE_CATALOG_POOL else None


def rules(world) -> None:
    # Pool k (0-based) needs k + 1 Progressive Catalog Pool. Nook's shop is the one source that's always on, so a
    # check needs an open shop.
    shop = timesanity.shop_open_rule(world)
    for k, pool in enumerate(world.catalog_pools):
        for item_id in pool:
            location = world.get_location(catalog_location_name(item_id))
            add_rule(location, lambda state, k=k: state.has(PROGRESSIVE_CATALOG_POOL, world.player, k + 1))
            if shop:
                add_rule(location, shop)


def catalog_goal(world):
    # Enough pools open, and Nook's shop (as for the catalog checks)
    shop = timesanity.shop_open_rule(world)

    def goal(state) -> bool:
        return (state.has(PROGRESSIVE_CATALOG_POOL, world.player, world.catalog_goal_pools)
                and (shop is None or shop(state)))
    return goal


def slot_data(world) -> dict:
    return {
        "catalogsanity": world.options.catalogsanity.value,
        # Game item ids per pool; [] without Catalogsanity
        "catalog_pools": world.catalog_pools,
        "new_item_chance": world.options.new_item_chance.value,
        "catalog_sources": sorted(world.options.catalog_sources.value),
        "catalog_goal_count": world.catalog_goal_count,
    }


# Unused hooks (world.py calls every hook of every feature)
def precollected(world) -> list[str]:
    return []
