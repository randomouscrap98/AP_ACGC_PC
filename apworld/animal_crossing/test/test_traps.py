# Traps / DeathLink: the pool math (Trap Percent of the locations, capped so the Bell Credits keep a quarter of the
# filler slots after Fruit Filler and the Extra Bells bags), the weights, the ids and the slot_data keys.
# Run from the Archipelago clone: python -m pytest worlds/animal_crossing/test -q
import math
from collections import Counter

from BaseClasses import ItemClassification
from test.general import setup_multiworld

from .. import bell_credits, extra_bells, fruit_filler, traps
from ..world import SLOT_DATA_VERSION, AnimalCrossingWorld
from . import AnimalCrossingTestBase


def generate(options: dict, seed: int = 1) -> AnimalCrossingWorld:
    # Itempool without the fill: create_items already made every filler item (the counts are what these test)
    return setup_multiworld(AnimalCrossingWorld, seed=seed, options=options).worlds[1]


def slots(world) -> tuple[Counter, int, int]:
    # The pool's item names, the filler slots after Fruit Filler and the Extra Bells bags (traps + Bell Credits),
    # and the slots the credits get (what the traps left)
    names = Counter(item.name for item in world.multiworld.itempool)
    left = len(world.multiworld.itempool) - len(world.seed_pool_items())
    left -= sum(names[name] for name in fruit_filler.FRUITS)
    left -= names[extra_bells.TEN_K] + names[extra_bells.THIRTY_K]
    return names, left, left - len(trap_names(world))


def trap_names(world) -> list[str]:
    return [item.name for item in world.multiworld.itempool if item.name in traps.TRAP_NAMES]


class TrapWeightsPoolTest(AnimalCrossingTestBase):
    """30% of the items, one kind at 0 and Resetti at 5, with DeathLink on"""

    run_default_tests = False  # the pool is what these test
    options = {
        "trap_percent": 30,
        "trap_weights": {"Bee Trap": 0, "Pitfall Trap": 1, "Weed Trap": 1, "Resetti Trap": 5},
        "death_link": True,
        "death_link_trap_weights": {"Bee Trap": 0, "Pitfall Trap": 1, "Weed Trap": 1, "Resetti Trap": 5},
    }

    def test_trap_count(self):
        # Trap Percent of the locations, capped at 3/4 of the slots left after the fruit and the bags
        made = len(trap_names(self.world))
        _, left, _ = slots(self.world)
        self.assertGreater(made, 0)
        self.assertEqual(made, min(round(len(self.world.seed_locations()) * 30 / 100), left * 3 // 4))

    def test_credits_keep_a_quarter(self):
        _, left, credits = slots(self.world)
        self.assertGreaterEqual(credits, math.ceil(left / 4))

    def test_every_slot_a_filler(self):
        names, left, credits = slots(self.world)
        fillers = credits + len(trap_names(self.world))
        fillers += sum(names[name] for name in fruit_filler.FRUITS)
        fillers += names[extra_bells.TEN_K] + names[extra_bells.THIRTY_K]
        self.assertEqual(fillers, len(self.world.multiworld.itempool) - len(self.world.seed_pool_items()))
        self.assertEqual(left, credits + len(trap_names(self.world)))
        self.assertEqual(len(self.world.multiworld.itempool), len(self.world.get_locations()))

    def test_weights(self):
        # Weights only pick between their own kinds: Bee Trap is 0, so it never appears
        made = trap_names(self.world)
        self.assertTrue(made)
        self.assertLessEqual(set(made), {"Pitfall Trap", "Weed Trap", "Resetti Trap"})
        for item in self.world.multiworld.itempool:
            if item.name in traps.TRAP_NAMES:
                self.assertEqual(item.classification, ItemClassification.trap)

    def test_slot_data(self):
        slot_data = self.world.fill_slot_data()
        self.assertEqual(slot_data["death_link"], 1)
        self.assertEqual(slot_data["accumulate_resetti"], 1)
        # A list in TRAP_NAMES order: Bee 0, Gyroid Face 0, Resetti 5, Pitfall 1, Weed 1, Cockroach 0, Color 0
        self.assertEqual(slot_data["death_link_trap_weights"], [0, 0, 5, 1, 1, 0, 0])
        self.assertEqual(slot_data["slot_data_version"], SLOT_DATA_VERSION)


class TrapWeightsOffTest(AnimalCrossingTestBase):
    """Every weight 0: Trap Percent puts nothing in the pool"""

    run_default_tests = False
    options = {"trap_percent": 80, "trap_weights": dict.fromkeys(traps.TRAP_NAMES, 0)}

    def test_no_traps(self):
        names, left, credits = slots(self.world)
        self.assertEqual(trap_names(self.world), [])
        # The slots stay with the Bell Credits
        self.assertEqual(credits, sum(names[name] for name in bell_credits.BELL_CREDITS))
        self.assertEqual(credits, left)


class TrapIdsTest(AnimalCrossingTestBase):
    run_default_tests = False
    options = {"trap_percent": 0}

    def test_ids(self):
        # Id order is TRAP_NAMES order, from 0x10380, 32 ids reserved (0x10380-0x1039F)
        self.assertEqual(list(traps.ITEMS), list(traps.TRAP_NAMES))
        self.assertEqual(list(traps.ITEMS.values()), [0x10380 + t for t in range(len(traps.TRAP_NAMES))])
        self.assertEqual(len(traps.TRAP_NAMES), 7)
        self.assertLessEqual(traps.ITEMS[traps.TRAP_NAMES[-1]], 0x10380 + 32 - 1)

    def test_off(self):
        # Trap Percent 0: no trap in the pool, but the keys are still sent (DeathLink off, the default weights)
        self.assertEqual(trap_names(self.world), [])
        slot_data = self.world.fill_slot_data()
        self.assertEqual(slot_data["death_link"], 0)
        self.assertEqual(slot_data["death_link_trap_weights"], [1] * 7)
        self.assertEqual(slot_data["accumulate_resetti"], 1)  # on by default, it needs no trap to matter

    # A tiny seed: Froggy Chair goal, no loans / museum / catalog checks, no Extra Bells, no Fruit Filler
    TINY = {"trap_percent": 80, "goal": ["Froggy Chair"], "loansanity": False, "museumsanity": False,
            "catalogsanity": False, "nook_store_upgrades": "sales", "favorsanity": True, "favor_pools": 1,
            "extra_bells": 0, "fruit_filler": False}

    def test_one_slot(self):
        # 80% of a single location: the slot stays a Bell Credit (the credits keep 25%, rounded up)
        world = generate(dict(self.TINY, favors_per_pool=1))
        _, _, credits = slots(world)
        self.assertEqual(len(world.get_locations()), 1)
        self.assertEqual(trap_names(world), [])
        self.assertEqual(credits, 1)

    def test_tiny(self):
        # 3 locations: 2 traps at most, so the last slot is a Bell Credit
        world = generate(dict(self.TINY, favors_per_pool=3))
        _, _, credits = slots(world)
        self.assertEqual(len(world.get_locations()), 3)
        self.assertEqual(len(trap_names(world)), 2)
        self.assertEqual(credits, 1)
