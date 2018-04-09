This is a framework for writing bots that play Hanabi.
- [BoardGameGeek page for Hanabi](http://boardgamegeek.com/boardgame/98778/hanabi)
- [Official rules (PDF)](http://web.archive.org/web/20140209111840/http://www.rnrgames.com/images/ProductRules/hanabiRules.PDF)

For an extensive list of human strategies, see [Zamiell/hanabi-conventions](https://github.com/Zamiell/hanabi-conventions).


Running the bots
----------------

- Typing `make` will build all of the bots that the Makefile knows about.
- Typing `make FAST=1` will use faster "release build" options.
- Appending `OPENMP=1` will enable parallel processing with OpenMP.

There are eight bots provided:

- BlindBot (blindly plays a random card each time)
- SimpleBot (hints about playable cards)
- ValueBot (also warns about valuable cards)
- HolmesBot (also performs inferences, and uses mulligans)
- SmartBot (prefers to play cards its partners don't know it knows)
- InfoBot (based on [code from Jeff Wu](https://github.com/WuTheFWasThat/hanabi.rs): a "hat game" strategy)
- CheatBot (secretly radios its partners; that is, cheats)
- NewCheatBot (also cheats)

To run a bot, the command is e.g. "./run_BlindBot".

Current average scores (and percent perfect games) per bot, for 2–5 players:

             2p               3p               4p               5p
    Blind     1.25             1.25             1.25             1.25
    Simple   16.92            18.48            19.32            18.50
    Value    19.79            19.43            19.39            17.76
    Holmes   20.75  (5.41%)   20.86  (0.37%)   20.46  (0.04%)   18.40
    Smart    23.09 (29.52%)   23.30 (19.54%)   22.44  (4.56%)   20.69 (0.08%)
    Info     20.00  (0.38%)   24.26 (51.53%)   24.86 (89.55%)   24.89 (91.36%)
    Cheat    24.90 (93.92%)   24.98 (98.37%)   24.97 (97.94%)   24.95 (96.47%)
    NewCheat 24.72 (85.56%)   24.91 (94.37%)   24.88 (91.20%)   24.85 (88.37%)

The interesting thing about CheatBot is that the perfect strategy is
difficult to compute, even when hints are redundant.

Notice that CheatBot and NewCheatBot must be built with `OPENMP=0`,
since their players use global variables to communicate with each other.
CheatBot is very fast, so this isn't much of a disadvantage.
Unfortunately, NewCheatBot is relatively very slow.


Rules subtleties
----------------

This framework respects all the official rules of Hanabi.
Some rules that are commonly played wrong, either accidentally
or on purpose:

* "Empty hints" — such as "Bob, you have no green cards" — are not allowed.
  (The game's designer is apparently on record as claiming that empty hints
  make the game better; but they also make it super easy to "solve" the game
  via ["hat-guessing"](https://github.com/rjtobin/HanSim) strategies.
  Empty hints are not allowed, according to the rulebook that comes in the box.)
  Empty hints can be enabled via `-DHANABI_ALLOW_EMPTY_HINTS`.

* For a 2- or 3-player game, each player should have 5 cards in hand.
  For 4- or 5-player games, each player should have only 4 cards in hand.

* When no hint stones are available, hinting is forbidden. Less well-known:
  When all 8 hint stones are available, discarding is forbidden.
  Discarding can be enabled via `-DHANABI_ALLOW_DISCARDING_EVEN_WITH_ALL_HINT_STONES`.

* When a card of value 5 is successfully played, a hint stone is regained
  if possible. If all 8 hint stones are available, the play is still allowed
  but does not regain you a hint stone.

* Three "mulligans" (or "strikes", or "misplays") are allowed. When the third
  mulligan is used up, the game ends immediately. Points are tallied in the
  usual fashion.

* When the last card of the deck is drawn, each player gets one more turn,
  including the player who drew the last card. This ensures that the last card
  of the deck may be played (if, say, it is of value 5). This does not ensure
  that all games are necessarily winnable; in fact there do exist arrangements
  of the deck which are not winnable.
