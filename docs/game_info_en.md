# Animal Crossing

## Where is the options page?

Use the Options Creator in the Archipelago Launcher (see [the setup guide](setup_en.md)).
Every option has a description there.

## What does randomization do to this game?

Animal Crossing has no dungeons or keys, so the randomizer locks parts of everyday life behind items:

- **Time (Timesanity):** the clock stops. Each month and each time slot (Morning, Day, Evening,
  Night) is an item. You set the date and hour from the pause menu, to any month and slot you own.
  This controls which bugs, fish and events you can get.
- **Tools (Tools in Pool):** the net, fishing rod and shovel are items. Tom Nook won't sell one until
  you receive it, and then it's mailed to you.
- **House upgrades (Loansanity):** each upgrade needs a Progressive House item. Paying off loans
  sends checks. Loans are much smaller than in vanilla by default.
- **Museum (Museumsanity):** donation items fill the museum, not your own donations. Catching
  bugs and fish and digging up fossils sends checks.
- **Favors (Favorsanity):** a number of villager favors (errands and contests) send checks.

Filler items are Bell Credits, which pay off your current loan directly.

## What is the goal?

Choose one or both:

- **Statue:** pay off every house loan so Tom Nook builds your statue.
- **Museum:** receive a percentage of the museum donation items.

Once your goals are done, listen to K.K. Slider on a Saturday night (20:00-23:59) to finish.

## What other changes are there?

Quality-of-life options, mostly on by default:

- Skip the intro and start next to your house, with your name, town, face, house and so on set in
  your options.
- Normalized time travel: changing the date always counts as one day passing, so no weeds or
  move-outs from jumping around.
- Bugs, fish and fossils can spawn evenly, or favor ones you still need.
- Fossils can come out of the ground already appraised.
- No cockroaches, more favors from villagers.
- Each date always has the same weather (vanilla odds), and the Date & Time page of the pause
  menu shows it for the date you pick. Event days and Nook's job can still override it. It always
  rains on the 13th from February to November, for the Coelacanth and the Snail.
- Optional: shops always open, no weeds, villagers never leave, villagers never sleep, turnips
  never spoil, no falling stalk market.

Cosmetic options: town fruit, grass shape, train station, Town Day, starting shirt, starting
villagers and a villager blacklist, and the stationery used for letters from Archipelago.

## Fossil seasons

With Fossil Spawns set to Season Locked, each fossil only comes out of the ground in its season,
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

A short game focused on the museum:

```yaml
Animal Crossing:
  goal: ["Museum"]
  museum_goal_percent: 50
  loansanity: false
  favorsanity: 0
  fossil_checks: instant
  critter_spawns: dynamic
```

A house-only game with no time locks:

```yaml
Animal Crossing:
  goal: ["Statue"]
  timesanity: false
  museumsanity: false
  total_loan_checks: 30
```
