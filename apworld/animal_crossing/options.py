from dataclasses import dataclass

from Options import (
    Choice, DefaultOnToggle, FreeText, NamedRange, OptionSet,
    PerGameCommonOptions, Range, Toggle
)

from .villager_data import PERSONALITIES, VILLAGERS

# Museumsanity check mode bits: the Bug/Fish/Fossil Checks option values below.
# WARN: the client mirrors them (AP_MUSEUM_FIND / AP_MUSEUM_DONATE in ap_slotdata.h)!
CHECK_FIND = 1    # catching a bug/fish, digging up a fossil (pre-appraised)
CHECK_DONATE = 2  # donating it


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
    """
    Your character's name (max 8 characters). Empty uses your slot name
    (truncated), or "-" if it has no plain (ASCII) characters. Only used with
    Skip Intro.
    """
    display_name = "Player Name"
    default = ""


class TownName(FreeText):
    """
    Your town's name (max 8 characters). Empty uses "Archi".
    Only used with Skip Intro.
    """
    display_name = "Town Name"
    default = ""


class Gender(Choice):
    """Your character's gender. Only used with Skip Intro."""
    display_name = "Gender"
    option_boy = 0
    option_girl = 1
    default = "random"


class Face(Choice):
    """Your character's face. Only used with Skip Intro."""
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
    """Which house you get. Only used with Skip Intro."""
    display_name = "House"
    option_top_left = 0
    option_top_right = 1
    option_bottom_left = 2
    option_bottom_right = 3
    default = "random"


class TownFruit(Choice):
    """
    Your town's native fruit (applies to every new town, regardless of intro skip).
    """
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
    default = "Archipelago"


class LoanLetterText(FreeText):
    """
    Text of the letter Archipelago sends when your loan is ready for payoff
    at the post office (max 192 characters, printable ASCII). Use | for a line
    break: the game doesn't wrap lines by itself. Empty uses the default.
    """
    display_name = "Loan Letter Text"
    default = "Your loan is ready for|payoff at the post office!"


class LetterPaper(Choice):
    """
    Stationery for letters from Archipelago (e.g. "your loan is ready for payoff").
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
    default = 0


class TownDay(Range):
    """
    The day in July of your town's Town Day. 4 is reserved for the Fireworks
    Festival, so it's replaced with a random day.
    """
    display_name = "Town Day"
    range_start = 1
    range_end = 31
    default = "random"


class Goal(OptionSet):
    """
    What you need to do to finish. With several goals, all of them are required.
    - Statue: pay off every house loan so Tom Nook builds your statue.
    - Museum: receive Museum Goal Percent of the museum donation items (needs Museumsanity).
    - Froggy Chair: get the froggy chair into your catalog, any way you can (never a Catalogsanity item).
    - Catalog: get Catalog Goal Percent of the Catalogsanity items into your catalog (needs Catalogsanity).
    - Furniture Set: every piece of the Furniture Set placed somewhere in your house. Each piece is an item,
      mailed to you, and can't be found any other way before that.
    - Chess Set: a full chess board's pieces somewhere in your house (8 pawns, 2 rooks, 2 knights, 2 bishops, a queen
      and a king per colour). One item per piece kind; order the rest from the catalog.
    Both set goals need the basement built and an open shop (to order lost pieces again).
    Once all goals are done, listen to K.K. Slider on a Saturday night (20:00-23:59) to finish.
    """
    display_name = "Goal"
    valid_keys = {"Statue", "Museum", "Froggy Chair", "Catalog", "Furniture Set", "Chess Set"}
    default = frozenset({"Statue", "Museum"})


class FurnitureSet(Choice):
    """Furniture Set goal only: the furniture series (10 pieces each) to collect."""
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
    """The loan for your starting house. Vanilla: 17,400."""
    display_name = "Starting Loan"
    default = 15000
    special_range_names = {"default": 15000, "vanilla": 17400}


class MediumLoan(LoanAmount):
    """The loan for the first house upgrade (bigger main floor). Vanilla: 148,000."""
    display_name = "Medium House Loan"
    default = 45000
    special_range_names = {"default": 45000, "vanilla": 148000}


class BasementLoan(LoanAmount):
    """The loan for the basement (the second upgrade). Vanilla: 49,800."""
    display_name = "Basement Loan"
    default = 30000
    special_range_names = {"default": 30000, "vanilla": 49800}


class LargeLoan(LoanAmount):
    """The loan for the third house upgrade (biggest main floor). Vanilla: 398,000."""
    display_name = "Large House Loan"
    default = 60000
    special_range_names = {"default": 60000, "vanilla": 398000}


class UpperLoan(LoanAmount):
    """The loan for the upper floor (the last upgrade). Vanilla: 798,000."""
    display_name = "Upper Floor Loan"
    default = 80000
    special_range_names = {"default": 80000, "vanilla": 798000}


class Loansanity(DefaultOnToggle):
    """
    Paying off loans sends checks, next loan gated by Progressive House item.
    Setting this to false means no loan checks, and Tom Nook offers upgrades as usual (the loan amounts still apply).
    Must be on for the Statue goal.
    """
    display_name = "Loansanity"


class FossilLoans(DefaultOnToggle):
    """
    The loans after the first (and the Statue goal) also expect the shovel (with Tools in Pool) and one
    dinosaur fossil item (with Progressive fossil availability), so you can dig up fossils to sell.
    Settings this to false will make it only require the Progressive House items.
    """
    display_name = "Fossil Loans"


class TotalLoanChecks(Range):
    """
    Number of checks spread over the five loans, split by each loan's share of the total debt
    (every loan gets at least one; its last check is paying it off).
    """
    display_name = "Total Loan Checks"
    range_start = 5
    range_end = 200
    default = 15


class LoanCreditPercent(Range):
    """
    Bell Credits are the filler: they pay your loan down directly (to 100 owed), then go to savings. They never
    reach your pocket. All credits together are worth this percentage of the total debt. With few filler items,
    the bigger credits are left out (no credit is worth less than 100 Bells while that helps).
    Extra Bells take filler slots first, so a high Extra Bells can leave no room for Bell Credits at all.
    """
    display_name = "Loan Credit Percent"
    range_start = 1
    range_end = 200
    default = 70


class ExtraBells(Range):
    """
    Extra Bells in the pool, in steps of 10,000 Bells, on top of the Bell Credits. Mailed to you as 10,000 and
    30,000 Bell bags (10,000 three times as likely). Best fit: takes filler slots before Bell Credits (can leave
    none), and when there aren't enough slots you get as close as fits.
    """
    display_name = "Extra Bells (x10,000)"
    range_start = 0
    range_end = 100
    default = 6


class Favorsanity(DefaultOnToggle):
    """
    Villager favors (errands and contests) send checks, in pools of Favors per Pool. Any favor from any
    villager counts. The first pool is open from the start; each Progressive Favor Pool opens the next.
    Favors done while every open pool is used up still count: their checks are sent once the next pool opens.
    """
    display_name = "Favorsanity"


class FavorPools(Range):
    """
    With Favorsanity, the number of favor pools. Favor Pools x Favors per Pool is at most 100.
    """
    display_name = "Favor Pools"
    range_start = 1
    range_end = 100
    default = 5


class FavorsPerPool(Range):
    """
    Favor checks in each Favor Pool.
    """
    display_name = "Favors per Pool"
    range_start = 1
    range_end = 100
    default = 3


class Shellsanity(DefaultOnToggle):
    """
    Archipelago items sometimes wash up on the town beach in place of a shell.
    Picking one up sends a check for the month it washed up in. No more of a month's
    items lie on the beach than it has checks left, and they stay there until picked
    up, like shells.
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


class ShellsanityItemChance(Range):
    """
    Shellsanity: chance (percent) that a shell washing up is an Archipelago item
    instead, while the current month has checks left. A full beach walk is about
    20 shells.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Shellsanity Item Chance"
    range_start = 1
    range_end = 100
    default = 20


class ToolsInPool(DefaultOnToggle):
    """
    The net, fishing rod and shovel are items. The tool is unobtainable through
    normal means (store, lost and found, etc) until you receive it.
    Tools and other items are mailed to you. Axe stays vanilla. The mole cricket and pill bug
    need the shovel as well as the net.
    """
    display_name = "Tools in Pool"


class StartingTool(Choice):
    """
    A tool you start with, mailed to you. Without Tools in Pool it's a freebie
    on top of the vanilla tools.
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
    What upgrades Tom Nook's store (Nook's Cranny -> Nook 'n' Go -> Nookway -> Nookington's). 
    Nookington's never needs a visitor from another town. Each upgrade still
    takes two days of renovation, one level at a time.
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
    """Sales Upgrades only: total Bells spent for Nook 'n' Go. Vanilla: 25,000."""
    display_name = "Nook 'n' Go Sales"
    default = 15000
    special_range_names = {"default": 15000, "vanilla": 25000}


class NookwaySales(StoreSales):
    """Sales Upgrades only: total Bells spent for Nookway. Vanilla: 90,000."""
    display_name = "Nookway Sales"
    default = 40000
    special_range_names = {"default": 40000, "vanilla": 90000}


class NookingtonsSales(StoreSales):
    """Sales Upgrades only: total Bells spent for Nookington's. Vanilla: 240,000."""
    display_name = "Nookington's Sales"
    default = 75000
    special_range_names = {"default": 75000, "vanilla": 240000}


class StoreCatalog(Range):
    range_start = 1
    range_end = 500


class NookNGoCatalog(StoreCatalog):
    """Catalog Upgrades only: catalog entries (of the current player) for Nook 'n' Go."""
    display_name = "Nook 'n' Go Catalog"
    default = 30


class NookwayCatalog(StoreCatalog):
    """Catalog Upgrades only: catalog entries (of the current player) for Nookway."""
    display_name = "Nookway Catalog"
    default = 70


class NookingtonsCatalog(StoreCatalog):
    """Catalog Upgrades only: catalog entries (of the current player) for Nookington's."""
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
    Villagers that start in a new town. With 6 or fewer, all of them start and
    the rest is random; with more, the 6 starters are picked from this list at
    random. "Personality: Cranky" (or Normal, Peppy, Lazy, Jock, Snooty) adds
    every villager with it. Blacklisted villagers are skipped.
    Only applies when the town is made, not to an existing save.
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


class ShopsAlwaysOpen(DefaultOnToggle):
    """Shops are open at any hour."""
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
    Turnips never spoil, not when the week ends and not when you go back in time.
    Ants only appear on spoiled turnips or candy, so with Timesanity the ant
    adds logic to need October (candy is sold Oct 16-30).
    """
    display_name = "Turnips Never Spoil"


class NoFishingTrash(DefaultOnToggle):
    """
    Fishing never hooks an empty can, boot or old tire.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "No Fishing Trash"


class HiddenBugHints(DefaultOnToggle):
    """
    Bugs that hide give themselves away now and then: a little dirt pops up where a
    mole cricket is buried or in front of a rock with a pill bug under it, and a tree
    with a bagworm or spider in it wiggles.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Hidden Bug Hints"


class HiddenItemHints(DefaultOnToggle):
    """
    A little sparkle now and then over the rock that gives Bells when you hit it, and in
    trees with furniture, a present or the golden shovel in them (not the ones with 100 Bells).
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
    Things you buy in Tom Nook's shop stay on the shelf, so you can buy them again
    (stationery already works like this). Grab bags still sell out.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Shop Never Sells Out"


class EasierCritterCatch(DefaultOnToggle):
    """
    Fish give you one more second to press A once the bobber goes under, and
    running near them no longer scares them (tools and the bobber still do). Sea fish
    face the beach instead of out to sea.
    Bugs take twice as long to notice you moving near them. Shaking a tree or
    swinging the net next to a bug still scares it. Bees chase you at half speed.
    Purple butterflies fly as low as the other butterflies instead of high above the trees.
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
    Coconuts sometimes wash up on the town beach in place of a shell (about one
    in every two to three full beach walks). Plant one on the beach row to grow a palm tree.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Coconuts Wash Up"


class NormalizedTimeTravel(DefaultOnToggle):
    """
    A new date always counts as one day passing, however far it jumps, forward
    or back: changing the date (pause menu or the game's own clock setting) or
    coming back after days away.
    Setting this to false makes the game sees the real jump, so a big jump forward is like being away
    that long (weeds, villagers moving out or greeting you like you were gone)
    and going back in time has the usual penalties.
    This can later be changed in-game under Settings > AP.
    """
    display_name = "Normalized Time Travel"


class Timesanity(DefaultOnToggle):
    """
    The clock stops. Each month and each time slot (Morning 4-8, Day 9-15, Evening 16-20, Night 21-3)
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


class Catalogsanity(DefaultOnToggle):
    """
    Catalog items are split into pools. Getting an item of an unlocked pool
    into your catalog (picking it up, wearing it, ...) sends a check. No pool
    is open at the start: each Progressive Catalog Pool unlocks the next one.
    Must be on for the Catalog goal.
    """
    display_name = "Catalogsanity"


class CatalogPools(Range):
    """
    Catalog is split into pools, each one requiring a Progressive Catalog Pool
    unlock (none open at the start). Pools x Pool Size = total catalog checks,
    at most 575 (furniture, clothes, carpets and wallpaper; no stationery or diaries).
    """
    display_name = "Catalog Pools"
    range_start = 1
    range_end = 100
    default = 5


class CatalogPoolSize(Range):
    """
    Catalog checks unlocked per Progressive Catalog Pool. Items are picked at
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
    With Catalogsanity, what can give you catalog checks. Turned off, a source
    only gives items that aren't checks (or are already in your catalog).
    Nook's shop is always a source and will eventually sell everything, so this
    never changes what is reachable.
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
    valid_keys = set(CATALOG_SOURCES)
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
    With the Museum goal, museum donations are also items: your own donations
    never fill the museum, it shows the donation items you received.
    Without it the museum works as usual.
    Must be on for the Museum goal.
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
    - Off: no bug checks.
    - Journal: catching it for the first time. With the Museum goal, Blathers never takes bugs.
    - Museum: donating it. With the Museum goal, Blathers takes it while its check isn't sent.
    - Both: catching and donating are separate checks.
    """
    display_name = "Bug Checks"


class FishChecks(CritterChecks):
    """
    With Museumsanity, what sends a check for each fish.
    - Off: no fish checks.
    - Journal: catching it for the first time. With the Museum goal, Blathers never takes fish.
    - Museum: donating it. With the Museum goal, Blathers takes it while its check isn't sent.
    - Both: catching and donating are separate checks.
    """
    display_name = "Fish Checks"


class FossilChecks(Choice):
    """
    With Museumsanity, what sends a check for each fossil.
    - Off: no fossil checks.
    - Instant: fossils come out of the ground already appraised; digging one up sends its check.
    - Vanilla: mail the fossil to the museum for appraisal, then donating it sends the check.
    - Instant Donate: as Instant, plus a second check for donating it.
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
    - Normalized: every bug and fish that can spawn is equally likely.
    - Dynamic: like Normalized, but bugs and fish whose check isn't sent yet (or, with no checks, that are missing
      from the journal or the museum) are 6 times as likely.
    With anything but vanilla, the fish that only live in river's lake and waterfall acres get 3/4 of the spawns.
    For bugs, they will spawn a bug at least half the time an acre rolls one during sparse bug times (winter, spring nights) (vanilla: as little as 17%).
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
        - "Large River Fish" (river, waterfall and river mouth, by the size of the fish's shadow)
        - "Pond Fish" (ponds and the fish that live only in the river's lake)
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
      With Timesanity, fossil checks need a month of their season.
    - Progressive: each dinosaur series (Tricera, T-rex, Apato, Stego, Ptera, Plesio, Mammoth) only comes out once
      you have its item ("T-rex Fossils", ...); "Single Fossils" opens Amber, Dinosaur Track, Ammonite, Dinosaur Egg
    and Trilobite.
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
    - Vanilla: the game's odds (half the time one of the five single fossils, else a dinosaur part).
    - Normalized: every available fossil equally likely.
    - Dynamic: like Normalized, but fossils whose check isn't sent yet (or, with no checks, that are missing from
      the museum) are 6 times as likely.
    - This can later be changed in-game under Settings > AP.
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
    For the Museum goal: percentage of the museum donation items you need to
    receive.
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
