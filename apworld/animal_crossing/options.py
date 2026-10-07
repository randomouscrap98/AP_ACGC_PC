from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, FreeText, OptionSet, PerGameCommonOptions, Range, Toggle

from .museum import CHECK_DONATE, CHECK_FIND
from .villagers import PERSONALITIES, VILLAGERS


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


class LetterSender(FreeText):
    """Signature at the bottom of letters from Archipelago (max 32 characters). Empty uses "Archipelago".
    The mailbox list still shows "AP" (sender names there are max 8 characters)."""
    display_name = "Letter Sender"
    default = "Archipelago"


class LoanLetterText(FreeText):
    """Text of the letter Archipelago sends when your loan is ready for payoff at the post office
    (max 192 characters, printable ASCII). Use | for a line break: the game doesn't wrap lines by itself.
    Empty uses the default."""
    display_name = "Loan Letter Text"
    default = "Your loan is ready for|payoff at the post office!"


class LetterPaper(Choice):
    """Stationery for letters from Archipelago (e.g. "your loan is ready for payoff")."""
    display_name = "Letter Paper"
    option_airmail_paper = 0
    option_sparkly_paper = 1
    option_bamboo_paper = 2
    option_orange_paper = 3
    option_essay_paper = 4
    option_panda_paper = 5
    option_ranch_paper = 6
    option_steel_paper = 7
    option_blossom_paper = 8
    option_vine_paper = 9
    option_cloudy_paper = 10
    option_petal_paper = 11
    option_snowy_paper = 12
    option_rainy_day_paper = 13
    option_watermelon_paper = 14
    option_deep_sea_paper = 15
    option_starry_sky_paper = 16
    option_daisy_paper = 17
    option_bluebell_paper = 18
    option_maple_leaf_paper = 19
    option_woodcut_paper = 20
    option_octopus_paper = 21
    option_festive_paper = 22
    option_skyline_paper = 23
    option_museum_paper = 24
    option_fortune_paper = 25
    option_stageshow_paper = 26
    option_thick_paper = 27
    option_spooky_paper = 28
    option_noodle_paper = 29
    option_neat_paper = 30
    option_horsetail_paper = 31
    option_felt_paper = 32
    option_parchment = 33
    option_cool_paper = 34
    option_elegant_paper = 35
    option_lacy_paper = 36
    option_polka_dot_paper = 37
    option_dizzy_paper = 38
    option_rainbow_paper = 39
    option_hot_neon_paper = 40
    option_cool_neon_paper = 41
    option_aloha_paper = 42
    option_ribbon_paper = 43
    option_fantasy_paper = 44
    option_woodland_paper = 45
    option_gingko_paper = 46
    option_fireworks_paper = 47
    option_winter_paper = 48
    option_gyroid_paper = 49
    option_ivy_paper = 50
    option_wing_paper = 51
    option_dragon_paper = 52
    option_tile_paper = 53
    option_misty_paper = 54
    option_simple_paper = 55
    option_honeybee_paper = 56
    option_mystic_paper = 57
    option_sunset_paper = 58
    option_lattice_paper = 59
    option_dainty_paper = 60
    option_butterfly_paper = 61
    option_new_years_card = 62
    option_inky_paper = 63
    default = 0


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
    Museum: receive Museum Goal Percent of the museum donation items (needs Museumsanity).
    Once all goals are done, listen to K.K. Slider on a Saturday night (20:00-23:59) to finish.
    """
    display_name = "Goal"
    valid_keys = {"Statue", "Museum"}
    default = frozenset({"Statue", "Museum"})


class LoanAmount(Range):
    range_start = 1000
    range_end = 9999999


class StartingLoan(LoanAmount):
    """The loan for your starting house. Vanilla: 17,400."""
    display_name = "Starting Loan"
    default = 5000


class MediumLoan(LoanAmount):
    """The loan for the first house upgrade (bigger main floor). Vanilla: 148,000."""
    display_name = "Medium House Loan"
    default = 10000


class BasementLoan(LoanAmount):
    """The loan for the basement (the second upgrade). Vanilla: 49,800."""
    display_name = "Basement Loan"
    default = 10000


class LargeLoan(LoanAmount):
    """The loan for the third house upgrade (biggest main floor). Vanilla: 398,000."""
    display_name = "Large House Loan"
    default = 20000


class UpperLoan(LoanAmount):
    """The loan for the upper floor (the last upgrade). Vanilla: 798,000."""
    display_name = "Upper Floor Loan"
    default = 30000


class Loansanity(DefaultOnToggle):
    """
    Paying off loans sends checks, and each house upgrade needs a Progressive House item.
    Off: no loan checks, and Tom Nook offers upgrades as usual (the loan amounts still apply).
    Must be on for the Statue goal.
    """
    display_name = "Loansanity"


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
    default = 10


class ToolsInPool(DefaultOnToggle):
    """
    The net, fishing rod and shovel are items. Tom Nook doesn't sell one (and it's not in the lost and found)
    until you receive it; it's mailed to you when you do. The axe stays vanilla.
    """
    display_name = "Tools in Pool"


class StartingTool(Choice):
    """
    A tool you start with, mailed to you. Without Tools in Pool it's a freebie on top of the vanilla tools.
    """
    display_name = "Starting Tool"
    option_none = 0
    option_net = 1
    option_fishing_rod = 2
    option_shovel = 3
    option_random_tool = 4
    default = 0


def villager_key(name: str) -> str:
    # What the player sees and writes in the yaml: "Bob (Lazy)"
    return f"{name} ({PERSONALITIES[VILLAGERS[name][1]]})"


def personality_key(personality: str) -> str:
    # Shorthand for every villager with this personality: "Personality: Cranky"
    return f"Personality: {personality}"


# Option keys for villager lists: every villager plus the personality shorthands
VILLAGER_KEYS = {personality_key(p) for p in PERSONALITIES} | {villager_key(name) for name in VILLAGERS}


class VillagerBlacklist(OptionSet):
    """
    Villagers that never move into your town (as a starting villager, a move-in or a summer camper).
    "Personality: Cranky" (or Normal, Peppy, Lazy, Jock, Snooty) blacklists every villager with it.
    At least 6 villagers must stay allowed. Villagers already living in an existing save stay.
    """
    display_name = "Villager Blacklist"
    valid_keys = VILLAGER_KEYS
    default = frozenset()


class StartingVillagers(OptionSet):
    """
    Villagers that start in a new town. With 6 or fewer, all of them start and the rest is random;
    with more, the 6 starters are picked from this list. "Personality: Cranky" (or Normal, Peppy,
    Lazy, Jock, Snooty) adds every villager with it. Blacklisted villagers are skipped.
    Only applies when the town is made, not to an existing save.
    """
    display_name = "Starting Villagers"
    valid_keys = VILLAGER_KEYS
    default = frozenset()


class NoCockroaches(DefaultOnToggle):
    """Cockroaches never appear in your house, even if you're away for a while."""
    display_name = "No Cockroaches"


class ShopsAlwaysOpen(Toggle):
    """Shops are open at any hour."""
    display_name = "Shops Always Open"


class NoWeeds(DefaultOnToggle):
    """Weeds never grow in your town."""
    display_name = "No Weeds"


class MoreFavors(DefaultOnToggle):
    """Villagers will almost always give you a job when you talk to them."""
    display_name = "More Favors"


class NormalizedTimeTravel(DefaultOnToggle):
    """
    A new date always counts as one day passing, however far it jumps, forward or back: changing
    the date (pause menu or the game's own clock setting) or coming back after days away.
    Off: the game sees the real jump, so a big jump forward is like being away that long
    (weeds, villagers moving out) and going back in time has the usual penalties.
    """
    display_name = "Normalized Time Travel"


class Timesanity(DefaultOnToggle):
    """
    The clock stops. Each month and each time slot (Morning 4-8, Day 9-15, Evening 16-20, Night 21-3)
    is an item. You set the date and hour from the pause menu, to any month and slot you own.
    """
    display_name = "Timesanity"


class StartingMonth(Choice):
    """With timesanity, the month you start with. The game starts on its 1st."""
    display_name = "Starting Month"
    option_january = 0
    option_february = 1
    option_march = 2
    option_april = 3
    option_may = 4
    option_june = 5
    option_july = 6
    option_august = 7
    option_september = 8
    option_october = 9
    option_november = 10
    option_december = 11
    default = "random"


class StartingTime(Choice):
    """With timesanity, the time slot you start with. The game starts at its first hour."""
    display_name = "Starting Time"
    option_morning = 0
    option_day = 1
    option_evening = 2
    option_night = 3
    default = "random"


class Museumsanity(DefaultOnToggle):
    """
    Museum donations are items: your own donations never fill the museum, it shows the donation items
    you received. Catching, digging up or donating things sends checks (see the options below).
    Must be on for the Museum goal.
    """
    display_name = "Museumsanity"


# Values are the check mode bits (museum.py CHECK_*, the client's AP_MUSEUM_*)
class CritterChecks(Choice):
    option_off = 0
    option_journal = CHECK_FIND
    option_museum = CHECK_DONATE
    option_both = CHECK_FIND | CHECK_DONATE
    default = CHECK_FIND


class BugChecks(CritterChecks):
    """
    With Museumsanity, what sends a check for each bug.
    Off: no bug checks.
    Journal: catching it for the first time. Blathers never takes bugs.
    Museum: donating it. Blathers takes it while its check isn't sent.
    Both: catching and donating are separate checks.
    """
    display_name = "Bug Checks"


class FishChecks(CritterChecks):
    """
    With Museumsanity, what sends a check for each fish.
    Off: no fish checks.
    Journal: catching it for the first time. Blathers never takes fish.
    Museum: donating it. Blathers takes it while its check isn't sent.
    Both: catching and donating are separate checks.
    """
    display_name = "Fish Checks"


class FossilChecks(Choice):
    """
    With Museumsanity, what sends a check for each fossil.
    Off: no fossil checks.
    Instant: fossils come out of the ground already appraised; digging one up sends its check.
    Vanilla: mail the fossil to the museum for appraisal, then donating it sends the check.
    Instant Donate: as Instant, plus a second check for donating it.
    """
    display_name = "Fossil Checks"
    # Values are the check mode bits (museum.py CHECK_*): "Vanilla" is donate only
    option_off = 0
    option_instant = CHECK_FIND
    option_vanilla = CHECK_DONATE
    option_instant_donate = CHECK_FIND | CHECK_DONATE
    default = CHECK_FIND


class CritterSpawns(Choice):
    """
    Which bugs and fish spawn. Works without Museumsanity too.
    Vanilla: the game's odds.
    Normalized: every bug and fish that can spawn is equally likely.
    Dynamic: bugs and fish whose check isn't sent yet (or, with no checks, that are missing from the journal or
    the museum) spawn more often.
    """
    display_name = "Critter Spawns"
    option_vanilla = 0
    option_normalized = 1
    option_dynamic = 2
    default = 1


class FossilSpawns(Choice):
    """
    Which fossil a dug-up fossil turns out to be. Works without Museumsanity too.
    Vanilla: the game's odds.
    Dynamic: fossils whose check isn't sent yet (or, with no checks, that are missing from the museum) are more
    likely.
    Season Locked: each fossil only comes out in one season (Spring: March-May, Summer: June-August,
    Autumn: September-November, Winter: December-February). With Timesanity, fossil checks need a month of
    their season.
    """
    display_name = "Fossil Spawns"
    option_vanilla = 0
    option_dynamic = 1
    option_season_locked = 2
    default = 1


class PaintingChecks(Choice):
    """With Museumsanity, what sends a check for each painting. Not available yet."""
    display_name = "Painting Checks"
    option_disabled = 0
    default = 0


class MuseumGoalPercent(Range):
    """For the Museum goal: percentage of the museum donation items you need to receive."""
    display_name = "Museum Goal Percent"
    range_start = 1
    range_end = 100
    default = 85


@dataclass
class AnimalCrossingOptions(PerGameCommonOptions):
    goal: Goal
    starting_loan: StartingLoan
    medium_loan: MediumLoan
    basement_loan: BasementLoan
    large_loan: LargeLoan
    upper_loan: UpperLoan
    loansanity: Loansanity
    total_loan_checks: TotalLoanChecks
    filler_bells_percent: FillerBellsPercent
    favorsanity: Favorsanity
    tools_in_pool: ToolsInPool
    starting_tool: StartingTool
    timesanity: Timesanity
    starting_month: StartingMonth
    starting_time: StartingTime
    museumsanity: Museumsanity
    bug_checks: BugChecks
    fish_checks: FishChecks
    fossil_checks: FossilChecks
    painting_checks: PaintingChecks
    critter_spawns: CritterSpawns
    fossil_spawns: FossilSpawns
    museum_goal_percent: MuseumGoalPercent
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
    villager_blacklist: VillagerBlacklist
    starting_villagers: StartingVillagers
    letter_paper: LetterPaper
    letter_sender: LetterSender
    loan_letter_text: LoanLetterText
    no_cockroaches: NoCockroaches
    shops_always_open: ShopsAlwaysOpen
    no_weeds: NoWeeds
    more_favors: MoreFavors
    normalized_time_travel: NormalizedTimeTravel
