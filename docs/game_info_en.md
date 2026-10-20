# Animal Crossing

## Where is the options page?

Use the Options Creator in the Archipelago Launcher (see [the setup guide](setup_en.md)).
Every option has a description there.

## What does randomization do to this game?

Animal Crossing has no dungeons or keys, so the randomizer locks parts of everyday life behind items:

- **Time (Timesanity):** the clock stops. Each month and each time slot (Morning 4-8, Day 9-15,
  Evening 16-20, Night 21-3) is an item. You set the date and hour from the pause menu, to any month
  and slot you own. This controls which bugs, fish and events you can get.
- **Tools (Tools in Pool):** the net, fishing rod and shovel are items. You can't get one (shop, lost
  and found, ...) until you receive it, and then it's mailed to you. Fishing and bug-catching contests
  need the rod or net. The axe works as usual.
- **House upgrades (Loansanity):** each upgrade needs a Progressive House item. Paying off loans
  sends checks. Loans are much smaller than in vanilla by default. With Fossil Loans (on by default),
  logic expects the shovel and one dinosaur fossil item before the loans after the first (and the Statue
  goal), so you can dig up fossils to sell.
- **Museum (Museumsanity):** catching bugs and fish, digging up fossils and/or donating them sends
  checks (set per category). With the Museum goal, donation items fill the museum, not your own
  donations, each exhibit shows who sent it, and Blathers only takes a donation while its check is
  unsent. Without the Museum goal the museum works as in vanilla. With
  Instant fossil checks, fossils come out of the ground already appraised. By default bugs, fish and
  fossils are gated by items:
  - Fish: "Small/Medium/Large River Fish" (river fish by shadow size), "Pond Fish" (ponds and the
    river lake's own fish), "Sea Fish".
  - Bugs: "Tree Bugs", "Beetles", "Flower Bugs", "Ground Bugs", "Flying Bugs". No bee trees before
    Tree Bugs.
  - Fossils: one "<Series> Fossils" item per dinosaur, plus "Single Fossils" for the rest. Nothing is
    buried until you have one.

  Locked bugs and fish simply don't spawn; the Bugs / Fish pages group them by these items.
- **Catalog (Catalogsanity):** catalog items are split into pools. Getting an item of an unlocked pool
  into your catalog (buying, picking up, wearing...) sends a check. No pool is open at the start; each
  Progressive Catalog Pool unlocks the next one. Nook's shop stocks items you still need, and Catalog
  Sources picks what else can give them (favors, trades, presents, the dump...). Passwords don't work.
- **Favors (Favorsanity):** villager favors (errands and contests) send checks, in pools (Favor Pools x
  Favors per Pool). The first pool is open; each Progressive Favor Pool opens the next. Favors done while the
  open pools are used up still count and send once the next pool opens.
- **Beach (Shellsanity, 3 per month by default):** Archipelago items sometimes wash up on the town beach in place
  of shells. Picking one up sends a check for the month it washed up in (Beach Checks per Month sets how many).
- **Nook's store:** by default each upgrade (Nook 'n' Go, Nookway, Nookington's) is a Progressive Nook
  Store item. It can also go by Bells spent or catalog size instead. Nookington's never needs a visitor.

Filler items are Bell Credits, which pay off your current loan directly. They stop at 100 Bells owed: you pay
the last 100 yourself at the post office, so you choose when the payoff (and its checks) happens. Credits that
arrive between an upgrade and Nook's next loan are held, and once every loan is paid they go into your savings.
Extra Bells (60,000 by default) adds real Bells on top: 10,000 and 30,000 Bell bags, mailed to you. They take filler
slots before the Bell Credits.

## What is the goal?

Choose one or more (all chosen goals are required):

- **Statue:** pay off every house loan so Tom Nook builds your statue.
- **Museum:** receive a percentage of the museum donation items (needs Museumsanity; the donation
  items only exist with this goal).
- **Froggy Chair:** get the froggy chair into your catalog, any way you can.
- **Catalog:** get a percentage of the Catalogsanity items into your catalog.
- **Furniture Set:** every piece of one furniture series (Classic, Cabin, Kiddie, ... 10 pieces; random by
  default) placed somewhere in your house. Each piece is an item, mailed to you; nothing else gives one before
  that. Needs the basement.
- **Chess Set:** a full chess board somewhere in your house: 8 pawns, 2 rooks, 2 knights, 2 bishops, a queen and a
  king per colour. One item per piece kind; order the rest from the catalog. Needs the basement.

Once your goals are done, a letter arrives telling you to listen to K.K. Slider on a Saturday night
(20:00-23:59) to finish. With Timesanity you need the Evening or Night slot for that.

## What other changes are there?

- Each seed gets its own save folder; existing towns can't be imported. You can't make new players:
  the game loads your player straight away.
- The island is closed (Kapp'n won't take you) for now.
- Pause menu: Date & Time and Save, plus an "AP" page (right under Resume) with Catalog (your unlocked
  pools), Bugs / Fish (what's around each month, which time slots, weather, what you already have, and your
  town's spawn rate), Shells (beach checks per month) and, with their goal, Furniture Set / Chess Set (every
  piece: received, and in your house).
- Checks made while disconnected are sent when you reconnect. If you quit before that, catches, digs
  and donations are lost; loans, favors and catalog checks are sent again next time.

Quality-of-life options, mostly on by default:

- Skip the intro and start next to your house, with your name, town, face, house and so on set in
  your options. Faces and starting shirts:

  ![Faces and starting shirts with their option keys](../pc/res/start_reference.png)
- Normalized time travel: changing the date always counts as one day passing, so no weeds or
  move-outs from jumping around.
- Bugs, fish and fossils can spawn evenly, or favor ones you still need.
- No cockroaches, no weeds, more favors from villagers, shops always open, turnips never spoil.
- Easier critter catch (longer bite window, fish don't flee from running, sea fish face the beach, bugs
  notice you half as fast, slower bees, purple butterflies fly low).
- Hidden bug hints: buried mole crickets, pill bug rocks and bagworm/spider trees give small visual hints
  when you're near.
- Hidden item hints: a sparkle now and then over the money rock and over trees with furniture, a present or
  the golden shovel in them.
- Coconuts sometimes wash up on the town beach.
- Each date always has the same weather (vanilla odds), and the Date & Time page of the pause
  menu shows it for the date you pick. Event days and Nook's job can still override it. It always
  rains on the 13th from February to November, for the Coelacanth and the Snail.
- Optional: villagers never leave, villagers never sleep, no falling stalk market, short shop talk,
  shop never sells out (bought items stay on the shelf; not grab bags).
- Most of these can be changed in game later, under Settings > AP.

Cosmetic options: town fruit, grass shape, train station, Town Day, starting shirt, starting
villagers and a villager blacklist, and the stationery, signature and text of letters from Archipelago.

## Updating

Seeds, options files and saves from 0.5.0 on keep working with newer versions of the game and apworld. A seed
made with a newer apworld than your game is refused on the title screen ("Seed needs a newer client").

## Fossil seasons

With Fossil Availability set to Season Locked, each fossil only comes out of the ground in its season,
to even out progression:

| Season | Months | Fossils |
|--------|--------|---------|
| Spring | March-May | Trilobite, Ammonite, Dinosaur Egg, Ptera (skull, both wings) |
| Summer | June-August | Apato (skull, tail, torso), Stego (skull, tail, torso) |
| Autumn | September-November | T-rex (skull, tail, torso), Tricera (skull, tail, torso) |
| Winter | December-February | Mammoth (skull, torso), Plesio (skull, neck, torso), Amber, Dinosaur Track |

## What does another world's item look like in Animal Crossing?

Checks are sent in the background; a message shows on screen when you send or receive an item.

## Example options

- [simple.yaml](example_yamls/simple.yaml): a simpler game for new players, with no time locks and fewer kinds of
  checks.
- [museum_short.yaml](example_yamls/museum_short.yaml): a short game focused on the museum.
- [house_only.yaml](example_yamls/house_only.yaml): a house-only game with no time locks.

