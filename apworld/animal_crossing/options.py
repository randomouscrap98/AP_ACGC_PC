from dataclasses import dataclass

from Options import (
    Choice, DefaultOnToggle, FreeText, NamedRange, OptionSet,
    PerGameCommonOptions, Range, Toggle
)

from .villager_data import PERSONALITIES, VILLAGERS

# Museumsanity check mode bits
# WARN: mirrored in ap_slotdata.h: AP_MUSEUM_FIND AP_MUSEUM_DONATE
CHECK_FIND = 1    # catching a bug/fish, digging up a fossil (pre-appraised)
CHECK_DONATE = 2  # donating it


class SkipIntro(Choice):
    """
    How much of the new game intro to skip.
    - off: full intro
    - skip_all: jump straight to house; train and job are skipped
    """
    display_name = "Skip Intro"
    option_off = 0
    # 1 is reserved for a future "train" (skip the train, keep the job) option
    option_skip_all = 2
    default = option_skip_all


class PlayerName(FreeText):
    """
    In-game name (max 8 characters). Empty uses slot name (truncated).
    If your name has no valid characters, the placeholder "-" is used.
    Only used when train is skipped.
    """
    display_name = "Player Name"
    default = ""


class TownName(FreeText):
    """
    In-game town name (max 8 characters). Empty uses "Archi".
    Only used when train is skipped.
    """
    display_name = "Town Name"
    default = ""


class Gender(Choice):
    """
    In-game gender. Only used when train is skipped.
    """
    display_name = "Gender"
    option_boy = 0
    option_girl = 1
    default = "random"


class Face(Choice):
    """
    In-game face; each gender has 8 possible faces.
    See github repo for list of faces by ID.
    Only used when train is skipped.
    """
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
    The shirt you start with. The game normally picks from 8 shirts per gender:
    flame to speedway for boys, folk to gelato for girls. Random (Gender) keeps
    vanilla behavior. Any shirt can be chosen for either gender.
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
    """
    There are four houses in this game to pick from. Usually
    Tom Nook asks which one you want; skipping his job also
    skips this part, so it pulls from the yaml.
    """
    display_name = "House"
    option_top_left = 0
    option_top_right = 1
    option_bottom_left = 2
    option_bottom_right = 3
    default = "random"


class TownFruit(Choice):
    """
    Pick the starting fruit. This is purely cosmetic.
    """
    display_name = "Town Fruit"
    option_apple = 0
    option_cherry = 1
    option_pear = 2
    option_peach = 3
    option_orange = 4
    default = "random"


class GrassShape(Choice):
    """
    The shape of the pattern in your town's grass texture.
    """
    display_name = "Grass Shape"
    option_triangle = 0
    option_square = 1
    option_circle = 2
    default = "random"


class TrainStation(Choice):
    """
    Your town's train station: wood, stone or brick, with five roof colors each.
    """
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
    """
    Signature at the bottom of letters from Archipelago (max 32 characters).
    Empty uses "Archipelago". The mailbox list still shows "AP"
    """
    display_name = "Letter Sender"
    default = ""


class LoanLetterText(FreeText):
    """
    Text of the letter Archipelago sends when your loan is ready for payoff
    at the post office (max 192 characters). Use | for a line break:
    the game doesn't wrap lines by itself. Empty uses
    "Your loan is ready for|payoff at the post office!"
    """
    display_name = "Loan Letter Text"
    default = ""


class LetterPaper(Choice):
    """
    Stationery for letters sent from Archipelago (items, goals, etc).
    """
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
    default = "random"


class TownDay(Range):
    """
    A day in July is picked to be the "Town Day" holiday. 4 is reserved for
    the Fireworks Festival (if you pick 4 it just rolls a random day).
    """
    display_name = "Town Day"
    range_start = 1
    range_end = 31
    default = "random"


class Goal(OptionSet):
    """
    What you need to do to finish. With several goals, all are required.
    Once all goals are done, listen to K.K. Slider on Saturday night (8pm-12am) to finish.
    - Statue: pay off every house loan so Tom Nook builds your statue.
    - Museum: receive "Museum Goal Percent" of the museum donation items (needs Museumsanity).
    - Froggy Chair: get the froggy chair into your catalog, any way you can (never a Catalogsanity item).
    - Catalog: get "Catalog Goal Percent" of the Catalogsanity items into your catalog (needs Catalogsanity).
    - Furniture Set: every piece of the Furniture Set placed somewhere in your house. 
      Each piece is an item mailed to you from AP and is unobtainable through normal means.
    - Chess Set: a full chess board's pieces somewhere in your house.
      Per color: 8 pawns, 2 rooks, 2 knights, 2 bishops, 1 queen, 1 king.
      Only one item of each kind is sent through AP, after which you can order the rest
      from the catalog.
    Chess and furniture goals need the basement built and an open shop time unlocked.
    """
    display_name = "Goal"
    valid_keys = ["Statue", "Museum", "Froggy Chair", "Catalog", "Furniture Set", "Chess Set"]
    default = frozenset({"Statue"})


class FurnitureSet(Choice):
    """
    Furniture Set goal only: the furniture series (10 pieces each) to collect.
    Wallpaper and flooring not included.
    """
    display_name = "Furniture Set"
    option_exotic = 0
    option_lovely = 1
    option_classic = 2
    option_ranch = 3
    option_cabana = 4
    option_blue = 5
    option_modern = 6
    option_regal = 7
    option_green = 8
    option_cabin = 9
    option_kiddie = 10
    default = "random"


class LoanAmount(NamedRange):
    range_start = 1000
    range_end = 999999


class StartingLoan(LoanAmount):
    """
    Loan amount for your starting house. Vanilla: 17,400.
    """
    display_name = "Starting Loan"
    default = 15000
    special_range_names = {"default": 15000, "vanilla": 17400}


class MediumLoan(LoanAmount):
    """
    Loan amount for the first house upgrade (bigger main floor). Vanilla: 148,000.
    """
    display_name = "Medium House Loan"
    default = 45000
    special_range_names = {"default": 45000, "vanilla": 148000}


class BasementLoan(LoanAmount):
    """
    Loan amount for the basement (second upgrade). Vanilla: 49,800.
    """
    display_name = "Basement Loan"
    default = 30000
    special_range_names = {"default": 30000, "vanilla": 49800}


class LargeLoan(LoanAmount):
    """
    Loan amount for the third house upgrade (biggest main floor). Vanilla: 398,000
    ."""
    display_name = "Large House Loan"
    default = 60000
    special_range_names = {"default": 60000, "vanilla": 398000}


class UpperLoan(LoanAmount):
    """
    Loan amount for the upper floor (the last upgrade). Vanilla: 798,000.
    """
    display_name = "Upper Floor Loan"
    default = 80000
    special_range_names = {"default": 80000, "vanilla": 798000}


class Loansanity(DefaultOnToggle):
    """
    Paying of loans sends a configurable amount of checks. Next loan gated by
    Progressive House items in AP pool. Turning this off sets loan behavior
    to vanilla, except for your configured loan amounts.
    Required for "Statue" goal.
    """
    display_name = "Loansanity"


class FossilLoans(Toggle):
    """
    All loans after the first have logic to require the shovel and one dinosaur
    fossil set, IF those are set to be AP items. This ensures you have a consistent
    way to make money if early game is stacked with Progressive Houses. Note that
    this gives less varied seeds, as you'll almost always get early shovel.
    """
    display_name = "Fossil Loans"


class TotalLoanChecks(Range):
    """
    Number of checks spread over the five loans in total, split by each loan's
    share of the total debt. Each loan gets at LEAST one check. Paying a loan
    releases ALL checks associated with that loan at once.
    """
    display_name = "Total Loan Checks"
    range_start = 5
    range_end = 200
    default = 15


class LoanCreditPercent(Range):
    """
    The main filler in this game: credits applied to your house loan, or to your
    savings if your loans are paid off. Setting is the percentage of your total
    loan; setting to 100 means filler will be enough to completely pay your loan.
    Bell credits reduce interruptions and menuing, and are the recommended filler.
    """
    display_name = "Loan Credit Percent"
    range_start = 1
    range_end = 200
    default = 70


class ExtraBells(Range):
    """
    Extra Bells in the pool, sent directly to you in the mail. Value is x10,000,
    so 6 = 60,000. A mix of 10k and 30k bags. If there's not enough filler items,
    you may not get the full amount or the exact amount. In the extreme case, can
    completely remove the credit filler, but this is unusual.
    """
    display_name = "Extra Bells (x10,000)"
    range_start = 0
    range_end = 100
    default = 6


class Favorsanity(DefaultOnToggle):
    """
    Villager favors send checks. This is the "give me work!" dialog option when
    talking to villagers. Favors are gated by pools to deepen progression/spheres.
    First pool always open from start. ANY favor from any Villager counts.
    """
    display_name = "Favorsanity"


class FavorPools(Range):
    """
    With Favorsanity, the number of pools to gate favors. Each pool has "Favors Per Pool"
    amount of favors in it. "Favor Pools" x "Favors per Pool" is at most 100.
    """
    display_name = "Favor Pools"
    range_start = 1
    range_end = 100
    default = 3


class FavorsPerPool(Range):
    """
    Favor checks in each Favor Pool.
    """
    display_name = "Favors per Pool"
    range_start = 1
    range_end = 100
    default = 5


class Shellsanity(Toggle):
    """
    Archipelago items sometimes wash up on the beach in place of a shell.
    Like shells, they stay forever until picked up. To get more shells to
    spawn, pick up existing ones (including non-AP shells) and move to a new day.
    """
    display_name = "Shellsanity"


class BeachChecksPerMonth(Range):
    """
    With Shellsanity, the beach checks per month (3 makes 36).
    """
    display_name = "Beach Checks per Month"
    range_start = 1
    range_end = 20
    default = 3


class Treesanity(Toggle):
    """
    Fruit trees drop AP items, up to the given amount per fruit tree type.
    Fruit is now progressive and in the pool, and is unobtainable through
    normal means in-game. Note that once fruit is received, Tom Nook will
    sell it in his store, should you lose it.
    """
    display_name = "Treesanity"


class TreeChecksPerFruit(Range):
    """
    With Treesanity, the amount of tree checks per fruit 
    (3 makes 15, or one tree per type). A tree drops 3 at most per harvest;
    it bears again 3 days after shaking.
    """
    display_name = "Tree Checks per Fruit"
    range_start = 1
    range_end = 20
    default = 3


class ShellsanityItemChance(Range):
    """
    With Shellsanity, percent chance that a generated shell is an Archipelago item.
    The beach can hold at most 20 of any shell, including AP items.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Shellsanity Item Chance"
    range_start = 1
    range_end = 100
    default = 20


class ToolsInPool(Toggle):
    """
    The net, fishing rod and shovel are AP items. Axe stays vanilla.
    The tool can't be bought or found until you receive it in the mail from
    the multiworld.
    """
    display_name = "Tools in Pool"


class StartingTool(Choice):
    """
    A tool you start with, mailed to you. Without Tools in Pool it's a freebie
    on top of the vanilla tools. This can greatly change logic.
    """
    display_name = "Starting Tool"
    option_none = 0
    option_net = 1
    option_fishing_rod = 2
    option_shovel = 3
    option_random_tool = 4
    default = 0


class NookStoreUpgrades(Choice):
    """
    How Tom Nook upgrades his store. Regardless of setting, Nookington's never
    needs a visitor to upgrade, unlike vanilla.
    - Progressive: three Progressive Nook Store items in the pool, one upgrade each.
    - Sales: total Bells spent at the store reaches the Nook Store Sales amounts (Vanilla behavior).
    - Catalog: the number of entries in your catalog reaches the Nook Store Catalog counts (any, not just Catalogsanity).
    """
    display_name = "Nook Store Upgrades"
    option_progressive = 1
    option_sales = 2
    option_catalog = 3
    default = option_progressive


class StoreSales(NamedRange):
    range_start = 1000
    range_end = 999999


class NookNGoSales(StoreSales):
    """
    Sales Upgrades only: total Bells spent for Nook 'n' Go. Vanilla: 25,000.
    """
    display_name = "Nook 'n' Go Sales"
    default = 15000
    special_range_names = {"default": 15000, "vanilla": 25000}


class NookwaySales(StoreSales):
    """
    Sales Upgrades only: total Bells spent for Nookway. Vanilla: 90,000.
    """
    display_name = "Nookway Sales"
    default = 40000
    special_range_names = {"default": 40000, "vanilla": 90000}


class NookingtonsSales(StoreSales):
    """
    Sales Upgrades only: total Bells spent for Nookington's. Vanilla: 240,000.
    """
    display_name = "Nookington's Sales"
    default = 75000
    special_range_names = {"default": 75000, "vanilla": 240000}


class StoreCatalog(Range):
    range_start = 1
    range_end = 500


class NookNGoCatalog(StoreCatalog):
    """
    Catalog Upgrades only: catalog entries (of the current player) for Nook 'n' Go.
    """
    display_name = "Nook 'n' Go Catalog"
    default = 30


class NookwayCatalog(StoreCatalog):
    """
    Catalog Upgrades only: catalog entries (of the current player) for Nookway.
    """
    display_name = "Nookway Catalog"
    default = 70


class NookingtonsCatalog(StoreCatalog):
    """
    Catalog Upgrades only: catalog entries (of the current player) for Nookington's.
    """
    display_name = "Nookington's Catalog"
    default = 120


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
    Villagers in this list will never move in, show up at the start, be a summer
    camper, etc. "Personality: Cranky" (or Normal, Peppy, Lazy, Jock, Snooty)
    blacklists all villagers with that personality. At least 6 villagers must stay allowed.
    """
    display_name = "Villager Blacklist"
    valid_keys = VILLAGER_KEYS
    default = frozenset()


class StartingVillagers(OptionSet):
    """
    Can force starting villagers. If more than 6, will be chosen randomly.
    "Personality: Cranky" (or Normal, Peppy, Lazy, Jock, Snooty) adds
    every villager with it. Blacklisted villagers are skipped.
    """
    display_name = "Starting Villagers"
    valid_keys = VILLAGER_KEYS
    default = frozenset()


class NoCockroaches(DefaultOnToggle):
    """
    Cockroaches never appear in your house, even if you're away for a while.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "No Cockroaches"


class ShopsAlwaysOpen(Toggle):
    """
    Shops are open at any hour. This impacts logic, as some time slots have
    no store available, so this setting cannot be changed later in-game.
    """
    display_name = "Shops Always Open"


class NoWeeds(DefaultOnToggle):
    """
    Weeds never grow in your town.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "No Weeds"


class MoreFavors(DefaultOnToggle):
    """
    Villagers will almost always give you a job when you talk to them.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "More Favors"


class VillagersDontLeave(Toggle):
    """
    Villagers never move out, and nobody talks about moving. Once your town has
    15 villagers, nobody new moves in either.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Villagers Don't Leave"


class VillagersNeverSleep(Toggle):
    """
    Villagers never go to sleep: when they would, they stay awake at home,
    so you can always visit them (handy for favors when you only have
    early-morning or night hours).
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Villagers Never Sleep"


class TurnipsNeverSpoil(DefaultOnToggle):
    """
    Turnips never spoil under any circumstances. This impacts ant logic,
    as the only way to get ants without spoiled turnips is candy, which
    is sold in October. This setting can't be changed later in-game.
    """
    display_name = "Turnips Never Spoil"


class FruitFiller(DefaultOnToggle):
    """
    Add non-town fruit to pool as filler. With Treesanity they're always in
    the pool (as progression).
    """
    display_name = "Fruit Filler"


class NoFishingTrash(DefaultOnToggle):
    """
    Fishing never hooks an empty can, boot or old tire.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "No Fishing Trash"


class HiddenBugHints(DefaultOnToggle):
    """
    Normally hidden bugs now show a visible sign for where they are.
    Dirt pops up near rocks with pill bugs or mole crickets underground,
    and trees with spiders or bagworms shake.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Hidden Bug Hints"


class HiddenItemHints(DefaultOnToggle):
    """
    Money rocks and trees with furniture, a present, or the golden shovel now sparkle.
    With Catalogsanity, goods in Nook's shop that would send a catalog check sparkle too.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Hidden Item Hints"


class SkipHRATalk(DefaultOnToggle):
    """
    Tom Nook never stops you in his shop to talk about the Happy Room Academy.
    The Academy starts judging your house the day after you move in.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Skip HRA Talk"


class ShortShopTalk(Toggle):
    """
    Tom Nook keeps it short when you buy something in his shop: just the item, the
    price and "would you like it?". He still gives you raffle tickets, he just
    doesn't talk about them. Catalog orders skip the small talk too.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Short Shop Talk"


class ShopNeverSellsOut(Toggle):
    """
    Things you buy in Tom Nook's shop stay on the shelf, so you can buy them again.
    Grab bags still sell out.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Shop Never Sells Out"


class EasierCritterCatch(DefaultOnToggle):
    """
    Fish can't get scared from running, and give you one whole additional second
    when biting the bobber. Tools can still be used to manually scare fish. Sea
    fish (attempt to) face the beach like in newer games. Bugs take longer to
    flee with walking or running (but still can). Bees chase at half speed,
    and Purple butterflies fly low to the ground.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Easier Critter Catch"


class NoFallingStalks(Toggle):
    """
    The stalk market never rolls the falling pattern (prices only go down all week).
    This can later be changed in-game under Settings > AP.
    """
    display_name = "No Falling Stalks"


class CoconutsWashUp(DefaultOnToggle):
    """
    Coconuts sometimes wash up on the town beach in place of a shell.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Coconuts Wash Up"


class NormalizedTimeTravel(DefaultOnToggle):
    """
    Time travel always counts as one day forward, no matter how far forward
    or back you actually travel. Setting this to false uses vanilla logic:
    backwards travel has penalties, and forward travel brings weeds and
    villager move-outs, etc.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Normalized Time Travel"


class Timesanity(Toggle):
    """
    The clock stops fully. Each month and each time slot (Morning 4-8, Day 9-15, Evening 16-20, Night 21-3)
    is an item. You set the date and hour from the pause menu, to any month and slot you own.
    """
    display_name = "Timesanity"


class StartingMonth(Choice):
    """
    With timesanity, the month you start with. The game starts on the 1st of
    the month.
    """
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
    """
    With timesanity, the time slot you start with. The game starts on the
    first hour of the time slot.
    """
    display_name = "Starting Time"
    option_morning = 0
    option_day = 1
    option_evening = 2
    option_night = 3
    default = "random"


class Catalogsanity(Toggle):
    """
    Reserve a section of the catalog for AP. Sections are gated by Progressive
    Catalog Pool. Registering an item sends a check. Items in the Catalogsanity
    pool only come from registered sources (see "Catalog Sources").
    Must be on for the Catalog goal.
    """
    display_name = "Catalogsanity"


class CatalogPools(Range):
    """
    Catalog is split into pools, each one requiring a Progressive Catalog Pool
    unlock (none open at the start). Pools x Pool Size = total catalog checks,
    at most 575 (furniture, clothes, carpets and wallpaper only)
    """
    display_name = "Catalog Pools"
    range_start = 1
    range_end = 100
    default = 5


class CatalogPoolSize(Range):
    """
    Catalog items unlocked per Progressive Catalog Pool. Items are picked at
    random, with every pool getting a similar mix of furniture, clothes,
    carpets and wallpaper.
    """
    display_name = "Catalog Pool Size"
    range_start = 1
    range_end = 100
    default = 15


class NewItemChance(Range):
    """
    How often a random item you get from Catalogsanity sources is new
    (Nook's shop, favors, balloon presents, ...)
    This can later be changed in-game under Settings > AP.
    """
    display_name = "New Item Chance"
    range_start = 5
    range_end = 100
    default = 50


CATALOG_SOURCES = [
    "Favors",
    "Trades",
    "Lost and Found",
    "Dump",
    "Letters",
    "Trees",
    "Presents",
    "Grab Bags",
    "Wisp"
]


class CatalogSources(OptionSet):
    """
    What can give you items from the reserved Catalogsanity catalog.
    Nook's shop is always a source and will eventually sell everything, so this
    never changes what is reachable, only how easy it is to get items outside Nook.
    - Favors: villager favor rewards.
    - Trades: villagers trading or selling in a chat.
    - Lost and Found: the daily item.
    - Dump: the daily items.
    - Letters: villager reply presents and letter-quality rewards.
    - Trees: furniture from shaking trees.
    - Presents: from trees and balloons.
    - Grab Bags: Nook's sale day bags.
    - Wisp: his reward.
    """
    display_name = "Catalog Sources"
    valid_keys = CATALOG_SOURCES
    default = frozenset(CATALOG_SOURCES)


class CatalogGoalPercent(Range):
    """
    For the Catalog goal: percentage of the Catalogsanity items you need in
    your catalog.
    """
    display_name = "Catalog Goal Percent"
    range_start = 1
    range_end = 100
    default = 85


class Museumsanity(DefaultOnToggle):
    """
    Catching, digging up or donating things sends checks (see the options below).
    Required for the Museum Goal, which replaces museum donations with AP items.
    """
    display_name = "Museumsanity"


# Values are the check mode bits (CHECK_* at the top, the client's AP_MUSEUM_*)
class CritterChecks(Choice):
    option_off = 0
    option_journal = CHECK_FIND
    option_museum = CHECK_DONATE
    option_both = CHECK_FIND | CHECK_DONATE
    default = CHECK_FIND


class BugChecks(CritterChecks):
    """
    With Museumsanity, what sends a check for each bug.
    - Off: no bug checks or AP items
    - Journal: catching it for the first time. With the Museum goal, Blathers never takes bugs.
    - Museum: donating it. With the Museum goal, Blathers takes it while its check isn't sent.
    - Both: catching and donating are separate checks. Doubles bug checks
    """
    display_name = "Bug Checks"


class FishChecks(CritterChecks):
    """
    With Museumsanity, what sends a check for each fish.
    - Off: no fish checks.
    - Journal: catching it for the first time. With the Museum goal, Blathers never takes fish.
    - Museum: donating it. With the Museum goal, Blathers takes it while its check isn't sent.
    - Both: catching and donating are separate checks. Doubles fish checks
    """
    display_name = "Fish Checks"


class FossilChecks(Choice):
    """
    With Museumsanity, what sends a check for each fossil.
    - Off: no fossil checks.
    - Instant: fossils come out of the ground already appraised; digging one up sends its check.
    - Vanilla: mail the fossil to the museum for appraisal, then donating it sends the check.
    - Instant Donate: dig up as Instant, plus a second check for donating it. Doubles fossil checks
    """
    display_name = "Fossil Checks"
    # Values are the check mode bits (CHECK_* at the top): "Vanilla" is donate only
    option_off = 0
    option_instant = CHECK_FIND
    option_vanilla = CHECK_DONATE
    option_instant_donate = CHECK_FIND | CHECK_DONATE
    default = CHECK_FIND


class CritterOdds(Choice):
    """
    How likely each bug and fish is. Works without Museumsanity too.
    - Vanilla: the game's odds.
    - Normalized: every bug and fish spawn is equally likely.
    - Dynamic: like Normalized, but bugs and fish whose check isn't sent yet (or have not been found)
      are 6 times as likely.
    Normalized and Dynamic force unique region fish (waterfall, lake) to take up a greater
    share of the fish in that region over river fish. Bugs also spawn more frequently in
    acres, which helps with winter sparseness.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Critter Odds"
    option_vanilla = 0
    option_normalized = 1
    option_dynamic = 2
    default = 2


class BugAvailability(Choice):
    """
    Which bugs can spawn.
    - All: every bug.
    - Progressive: a bug only spawns once you have its item:
      - "Tree Bugs" (cicadas, Bagworm, Spider, Purple Butterfly, Bee)
      - "Beetles"
      - "Flower Bugs" (butterflies, ladybugs, Mantis, Snail)
      - "Ground Bugs" (crickets, Grasshopper, locusts, Mole Cricket, Pill Bug, Ant, Cockroach)
      - "Flying Bugs" (dragonflies, Mosquito, Firefly, Pond Skater)
    Needs Bug Checks (with Museumsanity); without them this is All.
    """
    display_name = "Bug Availability"
    option_all = 0
    option_progressive = 1
    default = 1


class FishAvailability(Choice):
    """
    Which fish can spawn.
    - All: every fish.
    - Progressive: a fish only spawns once you have one of its items:
      - "Small River Fish"
      - "Medium River Fish"
      - "Large River Fish"
      - "Pond Fish" (includes the lake)
      - "Sea Fish"
    The fishing tourney needs the river items for the basses. Needs Fish Checks (with Museumsanity); without them this is All.
    """
    display_name = "Fish Availability"
    option_all = 0
    option_progressive = 1
    default = 1


class FossilAvailability(Choice):
    """
    Which fossils can come out of the ground. Works without Museumsanity too.
    - All: every fossil.
    - Season Locked: each fossil only comes out in one season:
      - Spring: March-May
      - Summer: June-August
      - Autumn: September-November
      - Winter: December-February
    - Progressive: each dinosaur series requires an item before it spawns, adds 8 progressive items to pool.
    No fossils are buried until you have one of these items. Needs Fossil Checks (with
    Museumsanity); without them this is All.
    """
    display_name = "Fossil Availability"
    option_all = 0
    option_season_locked = 1
    option_progressive = 2
    default = 2


class FossilOdds(Choice):
    """
    How likely each available fossil is. Works without Museumsanity too.
    - Vanilla: the game's original odds (not in your favor)
    - Normalized: every available fossil equally likely.
    - Dynamic: like Normalized, but fossils whose check isn't sent yet (or which have not been found) are 6 times as likely.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Fossil Odds"
    option_vanilla = 0
    option_normalized = 1
    option_dynamic = 2
    default = 2


class PaintingChecks(Choice):
    """
    With Museumsanity, what sends a check for each painting. Not available yet.
    """
    display_name = "Painting Checks"
    option_disabled = 0
    default = 0


class MuseumGoalPercent(Range):
    """
    For the Museum goal: percentage of the museum donation items you need to receive.
    """
    display_name = "Museum Goal Percent"
    range_start = 1
    range_end = 100
    default = 65


@dataclass
class AnimalCrossingOptions(PerGameCommonOptions):
    # Same order as the option groups in world.py: the Options Creator lists a group's options in this order
    goal: Goal
    museum_goal_percent: MuseumGoalPercent
    catalog_goal_percent: CatalogGoalPercent
    furniture_set: FurnitureSet
    tools_in_pool: ToolsInPool
    starting_tool: StartingTool
    shops_always_open: ShopsAlwaysOpen
    turnips_never_spoil: TurnipsNeverSpoil
    fruit_filler: FruitFiller
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
    letter_paper: LetterPaper
    letter_sender: LetterSender
    loan_letter_text: LoanLetterText
    villager_blacklist: VillagerBlacklist
    starting_villagers: StartingVillagers
    starting_loan: StartingLoan
    medium_loan: MediumLoan
    basement_loan: BasementLoan
    large_loan: LargeLoan
    upper_loan: UpperLoan
    loan_credit_percent: LoanCreditPercent
    extra_bells: ExtraBells
    favorsanity: Favorsanity
    favor_pools: FavorPools
    favors_per_pool: FavorsPerPool
    shellsanity: Shellsanity
    beach_checks_per_month: BeachChecksPerMonth
    treesanity: Treesanity
    tree_checks_per_fruit: TreeChecksPerFruit
    loansanity: Loansanity
    fossil_loans: FossilLoans
    total_loan_checks: TotalLoanChecks
    timesanity: Timesanity
    starting_month: StartingMonth
    starting_time: StartingTime
    catalogsanity: Catalogsanity
    catalog_pools: CatalogPools
    catalog_pool_size: CatalogPoolSize
    catalog_sources: CatalogSources
    museumsanity: Museumsanity
    bug_checks: BugChecks
    fish_checks: FishChecks
    fossil_checks: FossilChecks
    painting_checks: PaintingChecks
    bug_availability: BugAvailability
    fish_availability: FishAvailability
    fossil_availability: FossilAvailability
    nook_store_upgrades: NookStoreUpgrades
    nook_n_go_sales: NookNGoSales
    nookway_sales: NookwaySales
    nookingtons_sales: NookingtonsSales
    nook_n_go_catalog: NookNGoCatalog
    nookway_catalog: NookwayCatalog
    nookingtons_catalog: NookingtonsCatalog
    no_cockroaches: NoCockroaches
    no_weeds: NoWeeds
    more_favors: MoreFavors
    villagers_dont_leave: VillagersDontLeave
    villagers_never_sleep: VillagersNeverSleep
    no_falling_stalks: NoFallingStalks
    no_fishing_trash: NoFishingTrash
    easier_critter_catch: EasierCritterCatch
    hidden_bug_hints: HiddenBugHints
    hidden_item_hints: HiddenItemHints
    skip_hra_talk: SkipHRATalk
    short_shop_talk: ShortShopTalk
    shop_never_sells_out: ShopNeverSellsOut
    coconuts_wash_up: CoconutsWashUp
    normalized_time_travel: NormalizedTimeTravel
    critter_odds: CritterOdds
    fossil_odds: FossilOdds
    new_item_chance: NewItemChance
    shellsanity_item_chance: ShellsanityItemChance
