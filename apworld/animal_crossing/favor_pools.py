# Favorsanity: villager favors send checks, in Favor Pools pools of Favors per Pool. The first pool is open from
# the start; each Progressive Favor Pool opens the next. Favors done past the open pools are banked.
# WARN: the client mirrors the ids and the max (pc_ap_favorsanity.h)!
from BaseClasses import ItemClassification
from worlds.generic.Rules import add_rule

PROGRESSIVE_FAVOR_POOL = "Progressive Favor Pool"
ITEMS = {PROGRESSIVE_FAVOR_POOL: 0x10006}

FAVORS_MAX = 100  # Favor Pools x Favors per Pool max
FAVOR_BASE_ID = 0x20000  # favor n (1-based): FAVOR_BASE_ID + n


def favor_location_name(favor: int) -> str:
    return f"Favor {favor}"


LOCATIONS = {favor_location_name(n): FAVOR_BASE_ID + n for n in range(1, FAVORS_MAX + 1)}


def setup(world) -> None:
    o = world.options
    world.favor_pools = o.favor_pools.value if o.favorsanity else 0
    world.favor_count = world.favor_pools * o.favors_per_pool.value
    if world.favor_count > FAVORS_MAX:
        raise world.option_error(f"Favor Pools x Favors per Pool is {world.favor_count}, at most {FAVORS_MAX}.")


def locations(world) -> list[str]:
    return [favor_location_name(n) for n in range(1, world.favor_count + 1)]


def items(world) -> list[str]:
    return [PROGRESSIVE_FAVOR_POOL] * max(world.favor_pools - 1, 0)


def classify(world, name: str):
    return ItemClassification.progression if name == PROGRESSIVE_FAVOR_POOL else None


def rules(world) -> None:
    # Favor n (1-based) is in pool (n - 1) // favors_per_pool; pool k (0-based) needs k Progressive Favor Pool
    per = world.options.favors_per_pool.value
    for n in range(per + 1, world.favor_count + 1):
        add_rule(world.get_location(favor_location_name(n)),
                 lambda state, k=(n - 1) // per: state.has(PROGRESSIVE_FAVOR_POOL, world.player, k))


def slot_data(world) -> dict:
    return {
        "favorsanity": world.options.favorsanity.value,
        "favor_pools": world.options.favor_pools.value,
        "favors_per_pool": world.options.favors_per_pool.value,
    }


# Unused hooks (world.py calls every hook of every feature)
def precollected(world) -> list[str]:
    return []
