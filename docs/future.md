# Future ideas

Living list. Not a schedule. Keep new work on the **one-pet tamagotchi** loop
(egg → care → evolve → goodbye → next egg). The Pokédex is a scoreboard, not a
box. Forks already did party/LAN/gym — steal a beat from them, do not paste a
second game on top of this one.

Constraints that every idea has to survive:

- Round 360 / 466, four-button arc, swipe card + dex.
- Square-wave SFX on ES8311. No streamed BGM unless we add files on SD.
- PMD sheets we already pack: Idle, Walk L/R, Sleep, Eat, Hurt, Attack, Pose,
  Hop, Nod, DeepBreath, Sit. Attack / Hop / Sit are packed and barely used.
- SpriteCollab also has Wake, EventSleep, Charge, Cringe, Faint — not packed yet.
- One creature in NVS. A box or party is a new save shape.

---

## Another game

We have the ball (SPEED) and the bag (STRENGTH). DEFENSE is still “be good for
12 hours,” which nobody feels.

| Idea | Trains | Why it fits |
|---|---|---|
| **Walk (runner)** | SPEED + weight + ENE | **v1 shipped** + **birds**. Tap = hop lumps; stay down for overhead birds. Same single-gap as a lone lump (never inside a hop). Berry pickup still v2. |
| **Dodge / timing** | DEF | Tap just before a wild move. Uses packed **Hurt** + **Attack**. Different from the runner (one read, not a scroll). Closest thing to wild battle without a full combat UI. |
| **Berry hunt** | JOY + berry discovery | Optional Walk reward at milestones, not its own game. Hidden favorite flavor becomes a verb. |
| **Rhythm nod** | bond / JOY | Tap on the **Nod** / **DeepBreath** beat. Tiny, cute, uses sheets we already play at idle. |
| **Wild encounter** | ATK/DEF/SPD as designed | README roadmap. Resolve with those three stats + Attack/Hurt. Style still open: auto, timing, or 1-button turn. Trainer rank as endgame, not a team builder. |

Stay off LAN gyms and move lists unless this fork *wants* to become those other
repos.

### Walk runner (Chrome-dino, but ours)

Yes. A passive “go for a walk” is just the idle wander we already have. A
side-scroll hop is the first good reason to play packed **Hop** (we skipped it
in idle because it jumps off the ground line).

Rules that keep it TamaPoke:

- **One button.** Tap = hop. No duck. Birds fly above the standing body: jump
  hits them, walk does not. A second height / swipe-down duck can wait.
- **Round screen.** Play in the middle band: ground at `PET_GROUND`, hazards
  enter from the right, pet stays left-of-center. Sky/biome stay as they are.
- **Short.** Bag is 10 s. This should die on hit or stop around 20–30 s, not
  run forever like the dinosaur. High score on the card, like the ball game.
- **Play is the game tray** (same 4-well strip as food). Live: ball, bag,
  walk. The fourth well stays empty. Ball is chase-and-tap. Walk is
  survive-and-ramp.
- **Hazards from the biome**, not cacti: bush / flower (meadow, forest), rock
  (mountain, volcano), wave or shell (beach), snow lump (snow). Same `fillRoundRect`
  language as the bag. Hit = **Hurt** + over.
- **Speed ramp** is the joke. Start slower than idle walk, end faster than the
  ball. Square-wave tick on hop, thud on hit.

v1 is plants + Hop + Hurt. Birds are the “don’t mash jump” read. v2 is a berry
pickup. Wild ping can jump from a long run into the encounter idea later.

---

## Another state

Mood today is Happy / Sad / Eating / Sleeping. Sad already maps to Hurt. Gaps:

| State | Trigger | Sprite / UI |
|---|---|---|
| **Sick** | HYG under 35 while asleep | **Shipped.** Hurt. Medicine in the food tray. Blocks evolve. Bath does not cure. |
| **Tired** | ENE low, still awake | Sit (we skipped it because it faces away — maybe only from behind at night). Yawn via DeepBreath. |
| **Waking** | Light on after sleep | Pack **Wake**. One-shot, then Idle. |
| **Fainted** | All bars 0, before runaway | EventSleep / Faint. Last warning before the dark button. |
| **Training** | Bag or new game | Attack loop instead of Idle. We already play Attack nowhere on the home screen. |
| **Hot / cold** | biome + hour | Volcano day vs snow night. Just a status line + tint, not a new sheet. |

Sleep already forces night and snore Z’s. A wake beat would make the light
button feel like a scene, not a toggle.

---

## Other actions

Home buttons are Feed / Play / Light / Bath. Card has Train strength. Missing
verbs that still fit four buttons (long-press, card page, or a 5th slot):

- ~~**Medicine** — fifth food well. Only while sick. JOY/ENE cost.~~
- **Walk** — third well on the Play tray (see games). Not an AFK stroll; the
  pet already wanders on the home screen. Session cap ~20–30 s or until a hit.
  ENE down, weight down, JOY/SPE up with score. Berry at milestones.
- **Practice** — play packed **Attack** for a few seconds. Token ATK gain, less
  than the bag, so the bag stays the real trainer.
- **Talk / treat** — extra pet with Pose + Heart. Bond only, daily cap already
  exists.
- **Wake stretch** — if we pack Wake, Light-on is this, not an instant Idle.

Do not add a button that does not change a bar, a sprite, or the next egg.

---

## Context the game still does not say

The dex shows all 151. Unregistered are silhouettes. Nothing on device says
**raise → evolve → goodbye → new egg**, or that you cannot pick a species.
That is the #1 “how do I unlock this?” hole.

Shipped:

- ~~Unregistered detail: one line, “Hatch it from an egg.”~~
- ~~Egg screen: rarity in the lower panel + “goodbye blesses the next egg.”~~
- ~~First-run after starter: a two-line card, then never again.~~

---

## Sound

Square waves are the whole instrument. Recent: punch, soft tap, swipe, back.
Still mute or thin:

- Bath splash (only the UI tap today).
- Eat / chew distinct from the menu tap.
- Wake / evolve already have jingles; sick and faint need a low motif.
- Optional tiny sleep BGM later — only if it lives on SD and can be off.

Keep the queue short. Rapid bag hits taught us that.

---

## Systems already on the README

- **Wild encounters / battle** — stats exist; UI does not.
- **Soak test** — `HEALTH` is there; 24–48 h still to run.

Not in scope unless we change the product: party, box, trading, Gen 2+,
streamed music, runtime board detect.

---

## How to add an item

One line under the right heading: what the player does, which packed action or
stat it touches, and what we refuse (second pet, extra GPIO, new font). Cross
it out when it ships, or move the numbers into the README game manual.
