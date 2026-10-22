# Animal Crossing

## Where is the options page?

Use the Options Creator in the Archipelago Launcher (see [the setup guide](setup_en.md)).
Every option has a description there.

## What does randomization do to this game?

Animal Crossing has no dungeons or keys, so the randomizer locks parts of everyday life behind items:

- **House upgrades (Loansanity):** each upgrade needs a Progressive House item. Paying off loans
  sends checks. Loans are much smaller than in vanilla by default. With the Statue goal, you
  must pay off every loan.
- **Museum (Museumsanity):** catching bugs and fish, digging up fossils, and donating art sends checks. 
  You can turn each category on or off; art is currently not implemented and is forced off.
  Checks by default are sent when the critter is caught, or immediately upon digging up the fossil
  (which is pre-appraised), however the vanilla donation experience (including fossil appraisal)
  can be re-enabled by setting the check type in your yaml. You can also 
  double the checks by having them sent both on catch and on donate. With the Museum Goal
  (off by default), players collect your donations in the multiworld and fill up your museum. 
  You must complete a certain percentage of the museum (using the multiworld items, not your own 
  checks, which are separate) to complete the goal. To make progression more interesting, fish,
  bugs, and fossils are locked behind progressive items:
  - Fish (5): "Small/Medium/Large River Fish", "Pond Fish" (including the lake), "Sea Fish".
  - Bugs (5): "Tree Bugs" (including bees), "Beetles", "Flower Bugs", "Ground Bugs", "Flying Bugs"
  - Fossils (8): one "<Series> Fossils" item per dinosaur, plus "Single Fossils" for the rest. Nothing is
    buried until you have one.
- **Time (Timesanity):** the clock stops. Off by default. Each month and each time slot 
  (Morning 4-8, Day 9-15, Evening 16-20, Night 21-3) is an item. You set the date and 
  hour from the pause menu, to any month and slot you own. This controls which bugs, 
  fish and events you can get.
- **Tools (Tools in Pool):** the net, fishing rod and shovel are items. Off by default.
  These tools are unobtainable until receiving them from the multiworld, where it is mailed to
  you. This adds additional logic to bugs, fish, and fossils.
- **Catalog (Catalogsanity):** A configurable section of the whole catalog is reserved for the multiworld,
  and collecting items in this section releases checks. Off by default. The section is split into pools, which
  must be unlocked with progression items. No pool is open at the start. You can get all items
  from Nook's shop, however you can also enable additional sources to obtain these items. Without
  the pool unlocked, the items in that pool are unobtainable through any means.
- **Favors (Favorsanity):** villager favors (errands and contests) send checks, in pools (Favor Pools x
  Favors per Pool). The first pool is open; each Progressive Favor Pool opens the next. Favors done while the
  open pools are used up still count and send once the next pool opens.
- **Beach (Shellsanity, 3 per month by default):** Archipelago items sometimes wash up on the beach in 
  place of shells. Off by default. Picking one up sends a check for the month it washed up in 
  (Beach Checks per Month sets how many). Not good to use without Timesanity (all shells available at start).
- **Nook's store:** by default each upgrade (Nook 'n' Go, Nookway, Nookington's) is a Progressive Nook
  Store item. It can also go by Bells spent or catalog size instead. Nookington's never needs a visitor.

Filler items are Bell Credits, which pay off your current loan directly. They stop at 100 Bells owed: you pay
the last 100 yourself at the post office, so you choose when the payoff (and its checks) happens. Credits that
arrive between an upgrade and Nook's next loan are held, and once every loan is paid they go into your savings.
Extra Bells (60,000 by default) adds real Bells on top: 10,000 and 30,000 Bell bags, mailed to you. They take filler
slots before the Bell Credits. Finally, non-town fruit can be added to the pool (on by default).

## What is the goal?

Choose one or more (all chosen goals are required):

- **Statue:** pay off every house loan so Tom Nook builds your statue.
- **Museum:** receive a percentage of the museum donation items (needs Museumsanity; the donation
  items only exist with this goal).
- **Froggy Chair:** get the froggy chair into your catalog, any way you can.
- **Catalog:** get a percentage of the Catalogsanity items into your catalog.
- **Furniture Set:** every piece of one furniture series (Classic, Cabin, Kiddie, ... 10 pieces) or the
  Instruments set (15: guitars, pianos, strings, harp, lute, vibraphone, drums; random by default) placed somewhere
  in your house. Each piece is an item, mailed to you; nothing else gives one before
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
  piece: received, and in your house). A full in-game Archipelago client is also in there.
- Checks made while disconnected are sent when you reconnect. If you quit before that, catches, digs
  and donations are lost; loans, favors and catalog checks are sent again next time.

Quality-of-life options, mostly on by default:

- Skip the intro and start next to your house, with your name, town, face, house and so on set in
  your options. Faces and starting shirts:

  ![Faces and starting shirts with their option keys](../pc/res/start_reference.png)
- Normalized time travel: changing the date always counts as one day passing, so no weeds or
  move-outs from jumping around.
- Bugs, fish and fossils can spawn evenly, or favor ones you still need.
- No cockroaches, no weeds, more favors from villagers, shops always open (off by default), turnips never spoil.
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

- [expert.yaml](example_yamls/expert.yaml): a longer game (the defaults before 0.5.1): Statue and Museum goals,
  with time locks, catalog and beach checks, and the tools as items.
- [museum_short.yaml](example_yamls/museum_short.yaml): a short game focused on the museum.
- [house_only.yaml](example_yamls/house_only.yaml): a house-only game with no time locks.

