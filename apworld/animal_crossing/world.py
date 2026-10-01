from collections.abc import Mapping
from typing import Any

from BaseClasses import Item, ItemClassification, Location, Region
from Options import OptionError, OptionGroup
from worlds.AutoWorld import WebWorld, World
from worlds.generic.Rules import set_rule

from .items import BELL_BAGS, ITEM_NAME_TO_ID, PROGRESSIVE_HOUSE
from .locations import LOANS, LOCATION_NAME_TO_ID, favor_location_name, loan_location_name, split_loan_checks
from . import options
from .options import AnimalCrossingOptions

GAME_NAME = "Animal Crossing"
NAME_MAX = 8  # PLAYER_NAME_LEN / LAND_NAME_SIZE in the game
DEFAULT_TOWN = "Archi"
BELLS_ROUND = 100  # bell bag amounts are rounded to this


class AnimalCrossingItem(Item):
    game = GAME_NAME


class AnimalCrossingLocation(Location):
    game = GAME_NAME


def clamp_name(text: str) -> str:
    # The client does the real charset conversion; keep printable ASCII and the length here.
    text = "".join(c for c in text if " " <= c <= "~")
    return text.strip()[:NAME_MAX]


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
        ]),
        OptionGroup("Loan Goal", [
            options.StartingLoan,
            options.MediumLoan,
            options.LargeLoan,
            options.BasementLoan,
            options.UpperLoan,
            options.TotalLoanChecks,
            options.FillerBellsPercent,
        ]),
        OptionGroup("Quality of Life", [
            options.NoCockroaches,
            options.ShopsAlwaysOpen,
            options.NoWeeds,
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
        self.ac_player_name = clamp_name(self.options.player_name.value) or clamp_name(self.player_name)
        self.ac_town_name = clamp_name(self.options.town_name.value) or DEFAULT_TOWN
        # The 4th of July is the Fireworks Festival; the game never picks it either
        if self.options.town_day.value == 4:
            self.options.town_day.value = self.random.choice([d for d in range(1, 32) if d != 4])

        # Same order as LOANS
        self.loans = [
            self.options.starting_loan.value,
            self.options.medium_loan.value,
            self.options.basement_loan.value,
            self.options.large_loan.value,
            self.options.upper_loan.value,
        ]
        self.loan_checks = split_loan_checks(self.loans, self.options.total_loan_checks.value)

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        names = [loan_location_name(k, j) for k in range(len(LOANS)) for j in range(1, self.loan_checks[k] + 1)]
        names += [favor_location_name(n) for n in range(1, self.options.favorsanity.value + 1)]
        menu.add_locations({name: LOCATION_NAME_TO_ID[name] for name in names}, AnimalCrossingLocation)
        self.multiworld.regions.append(menu)

    def create_items(self) -> None:
        pool = [self.create_item(PROGRESSIVE_HOUSE) for _ in range(len(LOANS) - 1)]
        # Everything else is filler. Bell bag amounts are set in fill_slot_data from what was actually placed,
        # so adding other items to the pool later doesn't break the total.
        free = len(self.multiworld.get_unfilled_locations(self.player)) - len(pool)
        pool += [self.create_filler() for _ in range(free)]
        self.multiworld.itempool += pool

    def create_item(self, name: str) -> AnimalCrossingItem:
        if name == PROGRESSIVE_HOUSE:
            classification = ItemClassification.progression
        else:
            classification = ItemClassification.filler
        return AnimalCrossingItem(name, classification, ITEM_NAME_TO_ID[name], self.player)

    def get_filler_item_name(self) -> str:
        names = list(BELL_BAGS)
        return self.random.choices(names, weights=[BELL_BAGS[n][1] for n in names])[0]

    def set_rules(self) -> None:
        # Loan k only exists after k house upgrades (Progressive House x k)
        for k in range(1, len(LOANS)):
            for j in range(1, self.loan_checks[k] + 1):
                location = self.multiworld.get_location(loan_location_name(k, j), self.player)
                set_rule(location, lambda state, k=k: state.has(PROGRESSIVE_HOUSE, self.player, k))

        # All chosen goals are required
        goals = {
            # The statue comes after the last loan, which needs every house upgrade
            "Statue": lambda state: state.has(PROGRESSIVE_HOUSE, self.player, len(LOANS) - 1),
        }
        chosen = [goals[g] for g in sorted(self.options.goal.value)]
        self.multiworld.completion_condition[self.player] = lambda state: all(goal(state) for goal in chosen)

    def bell_bag_amounts(self) -> dict[str, int]:
        # Count the bell bags this player will actually receive (placed anywhere, plus start inventory),
        # then size them so they add up to filler_bells_percent of the total debt.
        counts = {name: 0 for name in BELL_BAGS}
        items = [loc.item for loc in self.multiworld.get_locations() if loc.item]
        items += self.multiworld.precollected_items[self.player]
        for item in items:
            if item.player == self.player and item.name in counts:
                counts[item.name] += 1

        total_bells = sum(self.loans) * self.options.filler_bells_percent.value / 100
        units = sum(counts[name] * BELL_BAGS[name][0] for name in BELL_BAGS)
        unit = total_bells / units if units else 0
        return {name: round(unit * BELL_BAGS[name][0] / BELLS_ROUND) * BELLS_ROUND for name in BELL_BAGS}

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
            "no_cockroaches": self.options.no_cockroaches.value,
            "shops_always_open": self.options.shops_always_open.value,
            "no_weeds": self.options.no_weeds.value,
            "goal": sorted(self.options.goal.value),
            "loans": self.loans,
            "loan_checks": self.loan_checks,
            "favorsanity": self.options.favorsanity.value,
            "bell_bags": self.bell_bag_amounts(),
        }
