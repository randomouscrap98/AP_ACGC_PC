import datetime
import math
from collections.abc import Mapping
from typing import Any

from BaseClasses import Item, ItemClassification, Location, Region
from Options import OptionError, OptionGroup
from worlds.AutoWorld import WebWorld, World
from worlds.generic.Rules import add_rule, set_rule

from .items import BELL_CREDITS, FISHING_ROD, ITEM_NAME_TO_ID, MONTHS, NET, PROGRESSIVE_HOUSE, SHOVEL, TIME_SLOTS, TOOLS
from .locations import LOANS, LOCATION_NAME_TO_ID, favor_location_name, loan_location_name, split_loan_checks
from .museum import (BUGS, CHECK_DONATE, CHECK_FIND, FISH, FOSSIL_SEASONS, FOSSILS, MUSEUM_BUG, MUSEUM_FISH,
                     MUSEUM_FOSSIL, MUSEUM_ITEM_NAME_TO_ID, MUSEUM_NAMES, MUSEUM_PAINTING, SEASON_MONTHS,
                     museum_donate_name, museum_find_name, museum_item_name)
from . import options
from .options import AnimalCrossingOptions, personality_key, villager_key
from .villagers import PERSONALITIES, VILLAGERS

GAME_NAME = "Animal Crossing"
NAME_MAX = 8  # PLAYER_NAME_LEN / LAND_NAME_SIZE in the game
DEFAULT_TOWN = "Archi"
LETTER_SENDER_MAX = 32  # MAIL_FOOTER_LEN in the game
DEFAULT_LETTER_SENDER = "Archipelago"
LETTER_TEXT_MAX = 192  # MAIL_BODY_LEN in the game
BELLS_ROUND = 100  # Bell credit amounts are rounded to this
MIN_VILLAGERS = 6  # the game starts with 6 villagers
MIN_YEAR, MAX_YEAR = 2001, 2100  # mTM_MIN_YEAR / mTM_MAX_YEAR (PC)


class AnimalCrossingItem(Item):
    game = GAME_NAME


class AnimalCrossingLocation(Location):
    game = GAME_NAME


def clamp_name(text: str, max_len: int = NAME_MAX) -> str:
    # The client does the real charset conversion; keep printable ASCII and the length here.
    text = "".join(c for c in text if " " <= c <= "~")
    return text.strip()[:max_len]


def letter_text(text: str) -> str:
    # | is the line break (FreeText is one line on the web options page)
    text = "".join(c for c in text if " " <= c <= "~")
    return text.strip().replace("|", "\n")[:LETTER_TEXT_MAX]


def villager_indices(chosen: set[str]) -> list[int]:
    # Expand personality shorthands into their villagers; the client gets npc indices
    return sorted(
        index for name, (index, looks) in VILLAGERS.items()
        if villager_key(name) in chosen or personality_key(PERSONALITIES[looks]) in chosen
    )


class AnimalCrossingWeb(WebWorld):
    # Options not listed here stay under "Game Options"
    option_groups = [
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
        OptionGroup("Loansanity", [
            options.Loansanity,
            options.TotalLoanChecks,
            options.StartingLoan,
            options.MediumLoan,
            options.BasementLoan,
            options.LargeLoan,
            options.UpperLoan,
            options.FillerBellsPercent,
        ]),
        OptionGroup("Timesanity", [
            options.Timesanity,
            options.StartingMonth,
            options.StartingTime,
        ]),
        OptionGroup("Museumsanity", [
            options.Museumsanity,
            options.BugChecks,
            options.FishChecks,
            options.FossilChecks,
            options.PaintingChecks,
            options.CritterSpawns,
            options.FossilSpawns,
            options.MuseumGoalPercent,
        ]),
        OptionGroup("Quality of Life", [
            options.NoCockroaches,
            options.ShopsAlwaysOpen,
            options.NoWeeds,
            options.MoreFavors,
            options.VillagersDontLeave,
            options.TurnipsNeverSpoil,
            options.NoFallingStalks,
            options.NormalizedTimeTravel,
        ]),
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

    def generate_early(self) -> None:
        if not self.options.goal.value:
            raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] Goal needs at least one goal.")
        if "Statue" in self.options.goal.value and not self.options.loansanity:
            raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] The Statue goal needs Loansanity on.")
        if "Museum" in self.options.goal.value and not self.options.museumsanity:
            raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] The Museum goal needs Museumsanity on.")
        # Check mode per museum category (CHECK_* bits), 0 = vanilla, no items. All 0 without Museumsanity.
        self.museum_modes = {MUSEUM_FOSSIL: 0, MUSEUM_PAINTING: 0, MUSEUM_BUG: 0, MUSEUM_FISH: 0}
        if self.options.museumsanity:
            self.museum_modes = {
                MUSEUM_FOSSIL: self.options.fossil_checks.value,
                MUSEUM_PAINTING: self.options.painting_checks.value,
                MUSEUM_BUG: self.options.bug_checks.value,
                MUSEUM_FISH: self.options.fish_checks.value,
            }
            if not any(self.museum_modes.values()):
                raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] Museumsanity needs at least one of Bug, "
                                  f"Fish or Fossil Checks on.")
        # One donation item per thing in each category with checks; locations per mode bit
        self.museum_item_names = []
        self.museum_locations = []
        for c, names in enumerate(MUSEUM_NAMES):
            for name in names if self.museum_modes[c] else []:
                self.museum_item_names.append(museum_item_name(name))
                if self.museum_modes[c] & CHECK_FIND:
                    self.museum_locations.append(museum_find_name(c, name))
                if self.museum_modes[c] & CHECK_DONATE:
                    self.museum_locations.append(museum_donate_name(name))
        # Tools in Pool: the tool each museum category needs, and the tools that go in the pool
        self.category_tools = {MUSEUM_BUG: NET, MUSEUM_FISH: FISHING_ROD, MUSEUM_FOSSIL: SHOVEL}
        # Starting tool: also without Tools in Pool (a freebie, mailed at the start)
        self.starting_tool = None
        choice = self.options.starting_tool.value
        if choice == options.StartingTool.option_random_tool:
            self.starting_tool = self.random.choice(TOOLS)
        elif choice != options.StartingTool.option_none:
            self.starting_tool = TOOLS[choice - 1]
        self.pool_tools = []
        if self.options.tools_in_pool:
            self.pool_tools = [tool for tool in TOOLS if tool != self.starting_tool]
        self.museum_goal_count = math.ceil(len(self.museum_item_names) * self.options.museum_goal_percent.value / 100)
        self.ac_player_name = clamp_name(self.options.player_name.value) or clamp_name(self.player_name)
        self.ac_town_name = clamp_name(self.options.town_name.value) or DEFAULT_TOWN
        self.ac_letter_sender = (clamp_name(self.options.letter_sender.value, LETTER_SENDER_MAX)
                                 or DEFAULT_LETTER_SENDER)
        self.ac_loan_letter_text = (letter_text(self.options.loan_letter_text.value)
                                    or letter_text(options.LoanLetterText.default))
        # The 4th of July is the Fireworks Festival; the game never picks it either
        if self.options.town_day.value == 4:
            self.options.town_day.value = self.random.choice([d for d in range(1, 32) if d != 4])

        self.villager_blacklist = villager_indices(self.options.villager_blacklist.value)
        # The client skips blacklisted ones too; this keeps slot_data readable
        self.starting_villagers = [index for index in villager_indices(self.options.starting_villagers.value)
                                   if index not in self.villager_blacklist]
        if len(VILLAGERS) - len(self.villager_blacklist) < MIN_VILLAGERS:
            raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] Villager Blacklist must leave at least "
                              f"{MIN_VILLAGERS} villagers allowed.")

        # Same order as LOANS
        self.loans = [
            self.options.starting_loan.value,
            self.options.medium_loan.value,
            self.options.basement_loan.value,
            self.options.large_loan.value,
            self.options.upper_loan.value,
        ]
        if self.options.loansanity:
            self.loan_checks = split_loan_checks(self.loans, self.options.total_loan_checks.value)
        else:
            self.loan_checks = [0] * len(LOANS)

        locations = sum(self.loan_checks) + self.options.favorsanity.value + len(self.museum_locations)
        if locations == 0:
            raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] Without Loansanity, Favorsanity must be "
                              f"at least 1.")
        # Every progression item needs a location (the starting month and slot are given at the start)
        items = len(LOANS) - 1 if self.options.loansanity else 0
        if self.options.timesanity:
            items += len(MONTHS) + len(TIME_SLOTS) - 2
        items += len(self.museum_item_names) + len(self.pool_tools)
        if locations < items:
            raise OptionError(f"[{GAME_NAME} - '{self.player_name}'] Total checks at {locations}, it must be at "
                              f"least {items} with these options. Raise optional checks such as Favorsanity.")

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        names = [loan_location_name(k, j) for k in range(len(LOANS)) for j in range(1, self.loan_checks[k] + 1)]
        names += [favor_location_name(n) for n in range(1, self.options.favorsanity.value + 1)]
        names += self.museum_locations
        menu.add_locations({name: LOCATION_NAME_TO_ID[name] for name in names}, AnimalCrossingLocation)
        self.multiworld.regions.append(menu)

    def create_items(self) -> None:
        pool = []
        if self.options.loansanity:
            pool += [self.create_item(PROGRESSIVE_HOUSE) for _ in range(len(LOANS) - 1)]
        if self.options.timesanity:
            # The starting month and slot are given at the start, the rest go in the pool
            start_month = MONTHS[self.options.starting_month.value]
            start_slot = TIME_SLOTS[self.options.starting_time.value]
            for name in MONTHS + TIME_SLOTS:
                if name in (start_month, start_slot):
                    self.multiworld.push_precollected(self.create_item(name))
                else:
                    pool.append(self.create_item(name))
        pool += [self.create_item(name) for name in self.museum_item_names]
        pool += [self.create_item(name) for name in self.pool_tools]
        if self.starting_tool:
            self.multiworld.push_precollected(self.create_item(self.starting_tool))
        # Everything else is filler. Bell credit amounts are set in fill_slot_data from what was actually placed,
        # so adding other items to the pool later doesn't break the total.
        free = len(self.multiworld.get_unfilled_locations(self.player)) - len(pool)
        pool += [self.create_filler() for _ in range(free)]
        self.multiworld.itempool += pool

    def create_item(self, name: str) -> AnimalCrossingItem:
        if name == PROGRESSIVE_HOUSE or name in MONTHS or name in TIME_SLOTS:
            classification = ItemClassification.progression
        elif name in MUSEUM_ITEM_NAME_TO_ID and "Museum" in self.options.goal.value:
            classification = ItemClassification.progression
        elif name in TOOLS:
            # Progression when a museum category with checks needs it (only with Tools in Pool)
            needed = self.options.tools_in_pool and any(self.museum_modes[c] for c, tool in self.category_tools.items() if tool == name)
            classification = ItemClassification.progression if needed else ItemClassification.useful
        else:
            classification = ItemClassification.filler
        return AnimalCrossingItem(name, classification, ITEM_NAME_TO_ID[name], self.player)

    def get_filler_item_name(self) -> str:
        names = list(BELL_CREDITS)
        return self.random.choices(names, weights=[BELL_CREDITS[n][1] for n in names])[0]

    def set_rules(self) -> None:
        # Loan k only exists after k house upgrades (Progressive House x k). Without loansanity
        # there are no loan checks (loan_checks is all 0) and no Statue goal.
        for k in range(len(LOANS)):
            for j in range(1, self.loan_checks[k] + 1):
                location = self.multiworld.get_location(loan_location_name(k, j), self.player)
                if k > 0:
                    set_rule(location, lambda state, k=k: state.has(PROGRESSIVE_HOUSE, self.player, k))
                # Paying needs bells, and selling needs an open shop: no shop is surely open in Morning
                # (only Nook 'n' Go, from 7), every other slot has at least one open hour (Night: 21)
                if self.options.timesanity and not self.options.shops_always_open:
                    add_rule(location, lambda state: state.has_any(("Day Hours", "Evening Hours", "Night Hours"),
                                                                   self.player))

        # Bugs and fish (catch and donate): a month and a time slot it spawns in
        if self.options.timesanity:
            for c, critters in ((MUSEUM_BUG, BUGS), (MUSEUM_FISH, FISH)):
                for name, when in critters:
                    self.set_museum_rule(c, name, self.spawn_rule(when))
            # Ants spawn on spoiled turnips or candy. With turnips never spoiling only candy is left, sold by
            # Nook Oct 16-30: needs October and a slot with an open shop (as for loans above)
            if self.options.turnips_never_spoil:
                self.add_museum_rule(MUSEUM_BUG, "Ant", lambda state: state.has("October", self.player))
                if not self.options.shops_always_open:
                    self.add_museum_rule(MUSEUM_BUG, "Ant", lambda state: state.has_any(
                        ("Day Hours", "Evening Hours", "Night Hours"), self.player))
        # Season locked fossils (dig up and donate): a month of its season
        if self.options.timesanity and self.options.fossil_spawns == options.FossilSpawns.option_season_locked:
            for name, season in zip(FOSSILS, FOSSIL_SEASONS):
                months = [MONTHS[m] for m in SEASON_MONTHS[season]]
                self.set_museum_rule(MUSEUM_FOSSIL, name, lambda state, months=months: state.has_any(months, self.player))

        # Tools in Pool: catching, digging up and donating need the tool (on top of the rules above)
        if self.options.tools_in_pool:
            for c, names in enumerate(MUSEUM_NAMES):
                if c in self.category_tools:
                    for name in names:
                        self.add_museum_rule(c, name, lambda state, tool=self.category_tools[c]:
                                             state.has(tool, self.player))

        # All chosen goals are required
        goals = {
            # The statue comes after the last loan, which needs every house upgrade
            "Statue": lambda state: state.has(PROGRESSIVE_HOUSE, self.player, len(LOANS) - 1),
            "Museum": lambda state: state.has_from_list(self.museum_item_names, self.player, self.museum_goal_count),
        }
        chosen = [goals[g] for g in sorted(self.options.goal.value)]
        # Goals are only sent when K.K. Slider plays (Saturday 20:00-23:59, any month): needs a slot with those hours
        if self.options.timesanity:
            chosen.append(lambda state: state.has_any(("Evening Hours", "Night Hours"), self.player))
        self.multiworld.completion_condition[self.player] = lambda state: all(goal(state) for goal in chosen)

    def set_museum_rule(self, category: int, name: str, rule) -> None:
        # Find and donate locations of one museum thing, the ones that exist
        for location in (museum_find_name(category, name), museum_donate_name(name)):
            if location in self.museum_locations:
                set_rule(self.multiworld.get_location(location, self.player), rule)

    def add_museum_rule(self, category: int, name: str, rule) -> None:
        for location in (museum_find_name(category, name), museum_donate_name(name)):
            if location in self.museum_locations:
                add_rule(self.multiworld.get_location(location, self.player), rule)

    def spawn_rule(self, when: dict[int, tuple[int, ...]]):
        # when: {month: time slots} from museum.py; any owned month with an owned slot of it
        pairs = [(MONTHS[m], [TIME_SLOTS[s] for s in slots]) for m, slots in when.items()]
        return lambda state: any(state.has(month, self.player) and state.has_any(slots, self.player)
                                 for month, slots in pairs)

    def bell_credit_amounts(self) -> dict[str, int]:
        # Count the bell credits this player will actually receive (placed anywhere, plus start inventory),
        # then size them so they add up to filler_bells_percent of the total debt.
        counts = {name: 0 for name in BELL_CREDITS}
        items = [loc.item for loc in self.multiworld.get_locations() if loc.item]
        items += self.multiworld.precollected_items[self.player]
        for item in items:
            if item.player == self.player and item.name in counts:
                counts[item.name] += 1

        total_bells = sum(self.loans) * self.options.filler_bells_percent.value / 100
        units = sum(counts[name] * BELL_CREDITS[name][0] for name in BELL_CREDITS)
        unit = total_bells / units if units else 0
        return {name: round(unit * BELL_CREDITS[name][0] / BELLS_ROUND) * BELLS_ROUND for name in BELL_CREDITS}

    def fill_slot_data(self) -> Mapping[str, Any]:
        # Random choices are already resolved here, so the client gets plain numbers.
        return {
            "skip_intro": self.options.skip_intro.value,
            "player_name": self.ac_player_name,
            "town_name": self.ac_town_name,
            "gender": self.options.gender.value,
            "face": self.options.face.value,
            "starting_shirt": self.options.starting_shirt.value,
            "house": self.options.house.value,
            "town_fruit": self.options.town_fruit.value,
            "grass_shape": self.options.grass_shape.value,
            "train_station": self.options.train_station.value,
            "town_day": self.options.town_day.value,
            "villager_blacklist": self.villager_blacklist,
            "starting_villagers": self.starting_villagers,
            "letter_paper": self.options.letter_paper.value,
            "letter_sender": self.ac_letter_sender,
            "loan_letter_text": self.ac_loan_letter_text,
            "no_cockroaches": self.options.no_cockroaches.value,
            "shops_always_open": self.options.shops_always_open.value,
            "no_weeds": self.options.no_weeds.value,
            "more_favors": self.options.more_favors.value,
            "villagers_dont_leave": self.options.villagers_dont_leave.value,
            "turnips_never_spoil": self.options.turnips_never_spoil.value,
            "no_falling_stalks": self.options.no_falling_stalks.value,
            "normalized_time_travel": self.options.normalized_time_travel.value,
            "goal": sorted(self.options.goal.value),
            "loans": self.loans,
            "loansanity": self.options.loansanity.value,
            "loan_checks": self.loan_checks,
            "favorsanity": self.options.favorsanity.value,
            "tools_in_pool": self.options.tools_in_pool.value,
            "timesanity": self.options.timesanity.value,
            "starting_month": self.options.starting_month.value,
            "starting_time": self.options.starting_time.value,
            "museumsanity": self.options.museumsanity.value,
            # Effective modes: 0 without Museumsanity
            "bug_checks": self.museum_modes[MUSEUM_BUG],
            "fish_checks": self.museum_modes[MUSEUM_FISH],
            "fossil_checks": self.museum_modes[MUSEUM_FOSSIL],
            "painting_checks": self.museum_modes[MUSEUM_PAINTING],
            "museum_goal_count": self.museum_goal_count,
            "critter_spawns": self.options.critter_spawns.value,
            "fossil_spawns": self.options.fossil_spawns.value,
            "fossil_seasons": FOSSIL_SEASONS,
            # Timesanity clock year: the year the seed was made, fixed for the whole save
            "start_year": min(max(datetime.date.today().year, MIN_YEAR), MAX_YEAR),
            "bell_credits": self.bell_credit_amounts(),
            # From archipelago.json; the client compares it with its own build
            "world_version": ".".join(str(n) for n in self.world_version),
        }
