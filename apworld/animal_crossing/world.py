# The Archipelago world: calls each feature at each generation step. Everything about a feature (its items,
# locations, rules, slot_data, ids) is in its own file, named after its option; FEATURES below lists them.
# Generated data: game_items, museum_data, catalog_data, villager_data. Options: options.py.
# Old seeds' slot_data: slot_data_upgrade (steps in slot_data_upgrades.json).
#
# Every feature file has all of these (plain functions taking the world; unused ones are stubs at the file's end):
#   setup(world)           generate_early: check options, roll random choices, store decisions on the world
#   locations(world)       its location names this seed
#   items(world)           its non-filler pool item names this seed
#   precollected(world)    its start inventory item names
#   classify(world, name)  ItemClassification for its own items, None for others
#   rules(world)           access rules (add_rule only, so the order of features doesn't matter)
#   slot_data(world)       its slot_data keys
#   ITEMS / LOCATIONS      name -> id, every possible one (static)
# Filler is separate (create_items): fruit_filler / extra_bells filler_items, then bell_credits.
# Features read each other's decisions from the world (e.g. world.museum_modes, set by museumsanity.setup): a file
# says so at the top, and SETUP_ORDER runs the source first. timesanity.py holds the rule helpers for
# "when can this be done" (shop hours, months, spawn times).
import dataclasses
from collections.abc import Mapping
from typing import Any

from BaseClasses import CollectionState, Item, ItemClassification, Location, Region
from Options import OptionError, OptionGroup
from worlds.AutoWorld import WebWorld, World

from . import (bell_credits, bug_availability, catalogsanity, extra_bells, favor_pools, fish_availability,
               fossil_availability, fruit_filler, furniture_sets, goal, intro, loansanity, museumsanity,
               nook_store_upgrades, options, quality_of_life, shellsanity, slot_data_upgrade, timesanity, tools_in_pool,
               town_generation, treesanity)
from .game_items import ITEM_NAME_TO_ID as GAME_ITEMS
from .options import AnimalCrossingOptions

GAME_NAME = "Animal Crossing"
# slot_data format (the client's AP_SLOTDATA_VERSION). Bump on any change the client reads (new key, new ids,
# removed/renamed key, changed meaning; new keys also get an upgrade step: slot_data_upgrades.json); not for
# apworld-only changes. Rules: ignore/docs/reference.md "Versions"
SLOT_DATA_VERSION = 5
# Universal Tracker: slot_data keys named like an option but holding something else (villager indices, the pools,
# a tool name), so they're not restored as options
UT_NOT_OPTIONS = {"villager_blacklist", "starting_villagers", "catalog_pools", "starting_tool"}

FEATURES = (goal, intro, town_generation, loansanity, bell_credits, extra_bells, fruit_filler, timesanity,
            favor_pools, shellsanity, treesanity, museumsanity, fossil_availability, fish_availability,
            bug_availability, tools_in_pool, catalogsanity, nook_store_upgrades, furniture_sets, quality_of_life)


def in_order(first: tuple) -> tuple:
    # `first` in its order, then every other feature (for steps where the order matters)
    return first + tuple(f for f in FEATURES if f not in first)


# Step orders below are legacy: how the single-file world.py called the features before the split, kept so the same
# seed + yaml still generates the same game. Only three are real constraints: museumsanity before the availability
# features and furniture_sets before catalogsanity (setup), treesanity before fruit_filler (classify).

# The order of random rolls, and every feature after the ones it reads (museumsanity before the availability
# features: museum_modes; furniture_sets before catalogsanity: goal_pieces). The rest keeps the order random choices
# were made in before the per-feature split, so seeds didn't change with it.
SETUP_ORDER = in_order((goal, tools_in_pool, museumsanity, fossil_availability, fish_availability, bug_availability,
                        intro, town_generation, loansanity, nook_store_upgrades, furniture_sets, catalogsanity,
                        favor_pools))
# Pool and location order feed the fill: changing them changes what a seed generates
POOL_ORDER = in_order((loansanity, timesanity, museumsanity, tools_in_pool, fossil_availability, fish_availability,
                       bug_availability, nook_store_upgrades, catalogsanity, favor_pools, furniture_sets, treesanity))
LOCATION_ORDER = in_order((loansanity, favor_pools, museumsanity, catalogsanity, shellsanity, treesanity))
PRECOLLECTED_ORDER = in_order((timesanity, tools_in_pool))
# create_item's classification: first answer wins (treesanity before fruit_filler)
CLASSIFY_ORDER = in_order((loansanity, timesanity, favor_pools, catalogsanity, fossil_availability,
                           fish_availability, bug_availability, museumsanity, tools_in_pool, nook_store_upgrades,
                           furniture_sets, treesanity, fruit_filler))

# Every game item is an AP item name (AP item id == the game's item number); the AP-only items are 0x10000+, above
# every game item number.
ITEM_NAME_TO_ID = {**GAME_ITEMS, **{name: item_id for f in FEATURES for name, item_id in f.ITEMS.items()}}
LOCATION_NAME_TO_ID = {name: location_id for f in FEATURES for name, location_id in f.LOCATIONS.items()}
# A name in two features would silently keep only one id
assert len(ITEM_NAME_TO_ID) == len(GAME_ITEMS) + sum(len(f.ITEMS) for f in FEATURES), "item name in two features"
assert len(LOCATION_NAME_TO_ID) == sum(len(f.LOCATIONS) for f in FEATURES), "location name in two features"


class AnimalCrossingItem(Item):
    game = GAME_NAME


class AnimalCrossingLocation(Location):
    game = GAME_NAME


class AnimalCrossingWeb(WebWorld):
    # Options not listed here stay under "Game Options" (Shops Always Open and Turnips Never Spoil change logic,
    # so they're there, not under Quality of Life; Global Storage can't be changed in game)
    option_groups = [
        # Goal settings are named <goal>_goal_<thing> (museum_goal_percent), listed after Goal
        OptionGroup("Goal", [
            options.Goal,
            options.MuseumGoalPercent,
            options.CatalogGoalPercent,
            options.FurnitureSet,
        ]),
        OptionGroup("Intro", [
            options.SkipIntro,
            options.PlayerName,
            options.TownName,
            options.Gender,
            options.Face,
            options.StartingShirt,
            options.House,
        ]),
        OptionGroup("Town Generation", [
            options.TownFruit,
            options.GrassShape,
            options.TrainStation,
            options.TownDay,
            options.LetterPaper,
            options.LetterSender,
            options.LoanLetterText,
            options.VillagerBlacklist,
            options.StartingVillagers,
        ]),
        # Apply with Loansanity off too
        OptionGroup("Matters of Money", [
            options.StartingLoan,
            options.MediumLoan,
            options.BasementLoan,
            options.LargeLoan,
            options.UpperLoan,
            options.LoanCreditPercent,
            options.ExtraBells,
        ]),
        OptionGroup("Favorsanity", [
            options.Favorsanity,
            options.FavorPools,
            options.FavorsPerPool,
        ]),
        OptionGroup("Shellsanity", [
            options.Shellsanity,
            options.BeachChecksPerMonth,
        ]),
        OptionGroup("Treesanity", [
            options.Treesanity,
            options.TreeChecksPerFruit,
        ]),
        OptionGroup("Loansanity", [
            options.Loansanity,
            options.FossilLoans,
            options.TotalLoanChecks,
        ]),
        OptionGroup("Timesanity", [
            options.Timesanity,
            options.StartingMonth,
            options.StartingTime,
        ]),
        OptionGroup("Catalogsanity", [
            options.Catalogsanity,
            options.CatalogPools,
            options.CatalogPoolSize,
            options.CatalogSources,
        ]),
        OptionGroup("Museumsanity", [
            options.Museumsanity,
            options.BugChecks,
            options.FishChecks,
            options.FossilChecks,
            options.PaintingChecks,
            options.BugAvailability,
            options.FishAvailability,
            options.FossilAvailability,
        ]),
        OptionGroup("Nook's Cranny", [
            options.NookStoreUpgrades,
            options.NookNGoSales,
            options.NookwaySales,
            options.NookingtonsSales,
            options.NookNGoCatalog,
            options.NookwayCatalog,
            options.NookingtonsCatalog,
        ]),
        # Only starting values: the game keeps them per save and they can be changed there (Settings > AP)
        OptionGroup("Quality of Life", [
            options.NoWeeds,
            options.MoreFavors,
            options.VillagersDontLeave,
            options.VillagersNeverSleep,
            options.NoFallingStalks,
            options.SkipHRATalk,
            options.ShopNeverSellsOut,
            options.CoconutsWashUp,
            options.NormalizedTimeTravel,
            options.CritterOdds,
            options.FossilOdds,
            options.NewItemChance,
            options.ShellsanityItemChance,
        ]),
        # No longer used (game settings since 0.5.3, Settings > Gameplay): still accepted so older yamls load
        OptionGroup("Deprecated", [
            options.NoCockroaches,
            options.NoFishingTrash,
            options.EasierCritterCatch,
            options.HiddenBugHints,
            options.HiddenItemHints,
            options.ShortShopTalk,
        ], start_collapsed=True),
    ]


class AnimalCrossingWorld(World):
    """
    Animal Crossing (GameCube), via the PC port.
    """

    game = GAME_NAME
    options_dataclass = AnimalCrossingOptions
    options: AnimalCrossingOptions
    web = AnimalCrossingWeb()

    item_name_to_id = ITEM_NAME_TO_ID
    location_name_to_id = LOCATION_NAME_TO_ID

    # Universal Tracker: the real seed's slot_data, else None (generate_early)
    ut: dict[str, Any] | None

    # Decisions shared between features, set by each feature's setup() in generate_early, grouped by that feature.
    # A feature reading another's runs after it (order in generate_early). Only the ones with a default are read
    # for item link groups (create_group), which never run generate_early but still call create_item.

    # intro
    ac_player_name: str
    ac_town_name: str
    # town_generation
    ac_letter_sender: str
    ac_loan_letter_text: str
    villager_blacklist: list[int]
    starting_villagers: list[int]
    # tools_in_pool
    starting_tool: str | None
    # loansanity
    loans: list[int]
    loan_checks: list[int]
    # museumsanity. Item link groups: "nothing chosen" is right for them, as link_items ORs in the linked players'
    # own classifications.
    museum_modes: dict[int, int] = {}
    museum_progression: frozenset[str] | set[str] = frozenset()
    museum_item_names: list[str]
    museum_locations: list[str]
    museum_goal_count: int
    # fossil_availability, fish_availability, bug_availability (read museum_modes)
    fossil_seasons: list[int]
    fossil_series: list[int]
    fossil_series_items: list[str]
    fish_pass_table: list[list[list[int | str]]]
    fish_passes: list[list[int]]
    fish_pass_items: list[str]
    bug_passes: list[int]
    bug_pass_items: list[str]
    # nook_store_upgrades
    store_items: int
    store_thresholds: list[int]
    # furniture_sets. Goal pieces: empty for item link groups
    furniture_set: str | None
    goal_pieces: list[str] | tuple[str, ...] = ()
    # catalogsanity (reads goal_pieces)
    catalog_pools: list[list[int]]
    catalog_goal_count: int
    catalog_goal_pools: int
    # favor_pools
    favor_pools: int
    favor_count: int
    # Bell Credit tiers get_filler_item_name draws from (create_items may drop the big ones)
    filler_tiers: tuple[str, ...] = tuple(bell_credits.BELL_CREDITS)

    # Universal Tracker regenerates the world from slot_data (no yaml needed): see ut_restore_options
    ut_can_gen_without_yaml = True

    @staticmethod
    def interpret_slot_data(slot_data: dict[str, Any]) -> dict[str, Any]:
        # Old seeds -> today's format (slot_data_upgrades.json), so restored options keep their old meaning
        return slot_data_upgrade.upgrade(slot_data, SLOT_DATA_VERSION)

    def ut_slot_data(self) -> dict[str, Any] | None:
        # The real seed's slot_data while Universal Tracker regenerates, else None
        return getattr(self.multiworld, "re_gen_passthrough", {}).get(self.game)

    def ut_restore_options(self, slot_data: dict[str, Any]) -> None:
        # Every option with a slot_data key of its name. Missing keys (older seeds) keep the default.
        for field in dataclasses.fields(self.options):
            if field.name in slot_data and field.name not in UT_NOT_OPTIONS:
                option = getattr(self.options, field.name)
                setattr(self.options, field.name, type(option).from_any(slot_data[field.name]))

    def option_error(self, message: str) -> OptionError:
        return OptionError(f"[{GAME_NAME} - '{self.player_name}'] {message}")

    def generate_early(self) -> None:
        # Universal Tracker: options and random rolls come from the real seed. Rolls below may still happen, but
        # self.random does NOT follow the real seed (e.g. random_tool, a restored town_day): every feature replaces
        # its rolled results from self.ut.
        self.ut = self.ut_slot_data()
        if self.ut is not None:
            self.ut_restore_options(self.ut)
        for feature in SETUP_ORDER:
            feature.setup(self)

        locations, items = len(self.seed_locations()), len(self.seed_pool_items())
        if not locations:
            raise self.option_error("Turn on Loansanity, Favorsanity, Shellsanity or Treesanity.")
        # Every non-filler item needs a location
        if locations < items:
            raise self.option_error(f"Total checks at {locations}, it must be at least {items} with these options. "
                                    f"Raise optional checks such as Favor Pools.")

    def seed_locations(self) -> list[str]:
        return [name for feature in LOCATION_ORDER for name in feature.locations(self)]

    def seed_pool_items(self) -> list[str]:
        # Non-filler items, in pool order
        return [name for feature in POOL_ORDER for name in feature.items(self)]

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        menu.add_locations({name: LOCATION_NAME_TO_ID[name] for name in self.seed_locations()},
                           AnimalCrossingLocation)
        self.multiworld.regions.append(menu)

    def create_items(self) -> None:
        for name in [name for feature in PRECOLLECTED_ORDER for name in feature.precollected(self)]:
            self.multiworld.push_precollected(self.create_item(name))
        pool = [self.create_item(name) for name in self.seed_pool_items()]
        # Everything else is filler. Bell credit amounts are set in fill_slot_data from what was actually placed,
        # so adding other items to the pool later doesn't break the total.
        free = len(self.multiworld.get_unfilled_locations(self.player)) - len(pool)
        fruit = [self.create_item(name) for name in fruit_filler.filler_items(self, free)]
        pool += fruit
        free -= len(fruit)
        money = [self.create_item(name) for name in extra_bells.filler_items(self, free)]
        pool += money
        free -= len(money)
        self.filler_tiers = tuple(bell_credits.tiers(self, free))
        pool += [self.create_filler() for _ in range(free)]
        self.multiworld.itempool += pool

    def create_item(self, name: str) -> AnimalCrossingItem:
        classification = ItemClassification.filler
        for feature in CLASSIFY_ORDER:
            found = feature.classify(self, name)
            if found is not None:
                classification = found
                break
        return AnimalCrossingItem(name, classification, ITEM_NAME_TO_ID[name], self.player)

    def get_filler_item_name(self) -> str:
        return bell_credits.filler_name(self)

    def set_rules(self) -> None:
        for feature in FEATURES:
            feature.rules(self)

        # Something must be reachable with only the start inventory, else the fill fails with a FillError
        # (e.g. only catalog checks, only museum checks behind a Progressive availability or a tool not given, only
        # shop checks at Morning)
        start = CollectionState(self.multiworld)  # every player's precollected items
        if not any(location.can_reach(start) for location in self.get_locations()):
            raise self.option_error("No check can be done at the start with these options. Turn on Loansanity, "
                                    "Favorsanity, Shellsanity or Treesanity. Museum checks only count at the "
                                    "start with their category's Availability set to All (and, with Tools in Pool, "
                                    "its tool as Starting Tool); loan checks at a Morning start need Shops Always "
                                    "Open.")

    def fill_slot_data(self) -> Mapping[str, Any]:
        # Random choices are already resolved here, so the client gets plain numbers.
        slot_data = {}
        for feature in FEATURES:
            slot_data.update(feature.slot_data(self))
        return {
            **slot_data,
            # From archipelago.json; the client compares it with its own build
            "world_version": ".".join(str(n) for n in self.world_version),
            "slot_data_version": SLOT_DATA_VERSION,
        }
