from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, FreeText, OptionSet, PerGameCommonOptions, Range, Toggle


class SkipIntro(Choice):
    """
    How much of the new game intro to skip.
    off: play the train ride and Tom Nook's part-time job normally.
    skip_all: skip the train ride and the job; you start next to your house.
    """
    display_name = "Skip Intro"
    option_off = 0
    # 1 is reserved for a future "train" (skip the train, keep the job) option
    option_skip_all = 2
    default = option_skip_all


class PlayerName(FreeText):
    """Your character's name (max 8 characters). Empty uses your slot name (truncated)."""
    display_name = "Player Name"
    default = ""


class TownName(FreeText):
    """Your town's name (max 8 characters). Empty uses "Archi"."""
    display_name = "Town Name"
    default = ""


class Gender(Choice):
    """Your character's gender."""
    display_name = "Gender"
    option_boy = 0
    option_girl = 1
    default = "random"


class Face(Choice):
    """Your character's face (the game normally picks this from Rover's questions)."""
    display_name = "Face"
    option_face_1 = 0
    option_face_2 = 1
    option_face_3 = 2
    option_face_4 = 3
    option_face_5 = 4
    option_face_6 = 5
    option_face_7 = 6
    option_face_8 = 7
    default = "random"


class StartingShirt(Choice):
    """
    The shirt you start with. The game picks from 8 shirts per gender: flame to speedway for boys,
    folk to gelato for girls. Random (Gender) keeps that; any shirt can be chosen for either gender.
    """
    display_name = "Starting Shirt"
    option_flame_shirt = 0
    option_paw_shirt = 1
    option_wavy_pink_shirt = 2
    option_future_shirt = 3
    option_bold_check_shirt = 4
    option_mint_gingham = 5
    option_bad_plaid_shirt = 6
    option_speedway_shirt = 7
    option_folk_shirt = 8
    option_daisy_shirt = 9
    option_wavy_tan_shirt = 10
    option_optical_shirt = 11
    option_rugby_shirt = 12
    option_sherbet_gingham = 13
    option_yellow_tartan = 14
    option_gelato_shirt = 15
    option_random_gender = 16
    default = option_random_gender

    @classmethod
    def get_option_name(cls, value: int) -> str:
        if value == cls.option_random_gender:
            return "Random (Gender)"
        return super().get_option_name(value)


class House(Choice):
    """Which house you get when the job is skipped."""
    display_name = "House"
    option_top_left = 0
    option_top_right = 1
    option_bottom_left = 2
    option_bottom_right = 3
    default = "random"


class TownFruit(Choice):
    """Your town's native fruit (applies to every new town, with or without the intro skip)."""
    display_name = "Town Fruit"
    option_apple = 0
    option_cherry = 1
    option_pear = 2
    option_peach = 3
    option_orange = 4
    default = "random"


class GrassShape(Choice):
    """The shape of the pattern in your town's grass texture."""
    display_name = "Grass Shape"
    option_triangle = 0
    option_square = 1
    option_circle = 2
    default = "random"


class TrainStation(Choice):
    """Your town's train station: wood, stone or brick, with five roof colors each."""
    display_name = "Train Station"
    option_wood_green_roof = 0
    option_wood_light_red_roof = 1
    option_wood_blue_roof = 2
    option_wood_purple_roof = 3
    option_wood_dark_red_roof = 4
    option_stone_red_roof = 5
    option_stone_orange_roof = 6
    option_stone_green_roof = 7
    option_stone_blue_roof = 8
    option_stone_purple_roof = 9
    option_brick_red_green_roof = 10
    option_brick_blue_roof = 11
    option_brick_tan_green_roof = 12
    option_brick_purple_roof = 13
    option_brick_brown_roof = 14
    default = "random"

    # Shown in the options GUI/WebHost instead of the YAML names
    names = {
        0: "Wood, Green Roof",
        1: "Wood (Light), Red Roof",
        2: "Wood, Blue Roof",
        3: "Wood, Purple Roof",
        4: "Wood (Dark), Red Roof",
        5: "Stone, Red Roof",
        6: "Stone, Orange Roof",
        7: "Stone, Green Roof",
        8: "Stone, Blue Roof",
        9: "Stone, Purple Roof",
        10: "Brick (Red), Green Roof",
        11: "Brick, Blue Roof",
        12: "Brick (Tan), Green Roof",
        13: "Brick, Purple Roof",
        14: "Brick, Brown Roof",
    }

    @classmethod
    def get_option_name(cls, value: int) -> str:
        return cls.names[value]


class TownDay(Range):
    """
    The day in July of your town's Town Day. 4 is the Fireworks Festival, so it's
    replaced with a random day.
    """
    display_name = "Town Day"
    range_start = 1
    range_end = 31
    default = "random"


class Goal(OptionSet):
    """
    What you need to do to finish. With several goals, all of them are required.
    Statue: pay off every house loan so Tom Nook builds your statue.
    """
    display_name = "Goal"
    valid_keys = {"Statue"}
    default = frozenset({"Statue"})


class LoanAmount(Range):
    range_start = 1000
    range_end = 9999999


class StartingLoan(LoanAmount):
    """The loan for your starting house. Vanilla: 17,400."""
    display_name = "Starting Loan"
    default = 17400


class MediumLoan(LoanAmount):
    """The loan for the first house upgrade (bigger main floor). Vanilla: 148,000."""
    display_name = "Medium House Loan"
    default = 98000


class LargeLoan(LoanAmount):
    """The loan for the second house upgrade (biggest main floor). Vanilla: 398,000."""
    display_name = "Large House Loan"
    default = 198000


class BasementLoan(LoanAmount):
    """The loan for the basement. Vanilla: 49,800."""
    display_name = "Basement Loan"
    default = 49800


class UpperLoan(LoanAmount):
    """The loan for the upper floor (the last upgrade). Vanilla: 798,000."""
    display_name = "Upper Floor Loan"
    default = 298000


class TotalLoanChecks(Range):
    """
    Number of checks spread over the five loans, split by each loan's share of the total debt
    (every loan gets at least one; its last check is paying it off).
    """
    display_name = "Total Loan Checks"
    range_start = 5
    range_end = 200
    default = 15


class FillerBellsPercent(Range):
    """
    Total Bells sent as filler items, as a percentage of the total debt of all loans.
    AP Bells pay your current loan; 100 or more can pay off everything by itself.
    """
    display_name = "Filler Bells Percent"
    range_start = 0
    range_end = 200
    default = 50


class Favorsanity(Range):
    """Number of villager favors (errands and contests) that send a check, in the order you complete them."""
    display_name = "Favorsanity"
    range_start = 0
    range_end = 100
    default = 20


class NoCockroaches(DefaultOnToggle):
    """Cockroaches never appear in your house, even if you're away for a while."""
    display_name = "No Cockroaches"


class ShopsAlwaysOpen(Toggle):
    """Shops are open at any hour."""
    display_name = "Shops Always Open"


class NoWeeds(DefaultOnToggle):
    """Weeds never grow in your town."""
    display_name = "No Weeds"


@dataclass
class AnimalCrossingOptions(PerGameCommonOptions):
    goal: Goal
    starting_loan: StartingLoan
    medium_loan: MediumLoan
    large_loan: LargeLoan
    basement_loan: BasementLoan
    upper_loan: UpperLoan
    total_loan_checks: TotalLoanChecks
    filler_bells_percent: FillerBellsPercent
    favorsanity: Favorsanity
    skip_intro: SkipIntro
    player_name: PlayerName
    town_name: TownName
    gender: Gender
    face: Face
    starting_shirt: StartingShirt
    house: House
    town_fruit: TownFruit
    grass_shape: GrassShape
    train_station: TrainStation
    town_day: TownDay
    no_cockroaches: NoCockroaches
    shops_always_open: ShopsAlwaysOpen
    no_weeds: NoWeeds
