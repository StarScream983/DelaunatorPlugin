# in minecraft what does continentalness, P&V and Erosion mean? how are they used to determine final height?

In Minecraft, continentalness is a large-scale Perlin noise field (one of several "climate parameters") that essentially answers "how far inland or out to sea is this location?" — and it's one of the three main noise inputs (alongside erosion and peaks & valleys) that determine terrain height. [github](https://github.com/mebusy/notes/blob/master/dev_notes/minecraft_world_gen.md)

## What continentalness represents

Continentalness is a single scalar value per XZ column, typically ranging from -1 to 1, sampled from a large-frequency Perlin noise field. Low values (near -1) mean the location is deep ocean; values near 0 sit around the coastline; high values (approaching 1) mean deep inland terrain. Mojang defines named bands across this range — deep ocean, ocean, coast, near-inland, mid-inland, far-inland — each corresponding to a specific numeric interval (e.g. deep ocean spans roughly -1.05 to -0.45). [youtube](https://www.youtube.com/watch?v=2QehypczePA)

## How it drives height

Continentalness feeds into a spline function — not a linear formula — that maps the noise value to a target terrain height. Splines let developers hand-place control points (e.g. "at continentalness -1, height is 50" and "at continentalness 1, height is 100") and the game interpolates a curve between them, so the height-vs-continentalness relationship can bend sharply rather than scale linearly. Critically, this spline isn't 1-dimensional: the final spline is actually built using continentalness, erosion, and weirdness/peaks-and-valleys together, so the same continentalness value produces different heights depending on the accompanying erosion and PV values. [dawnosaur.substack](https://dawnosaur.substack.com/p/how-minecraft-generates-worlds-you)

## Division of labor with erosion and peaks & valleys

The three noises don't overlap in role — continentalness mainly decides whether an area floods below sea level (oceans vs. land) and how "inland" it feels, while erosion controls flatness versus mountainousness of that base shape, and peaks & valleys adds the fine ridge/valley/plateau detail on top. [youtube](https://www.youtube.com/watch?v=sgOkNiU7FUU)

| Noise | Primary job | Effect on height |
|---|---|---|
| Continentalness | Ocean vs. land, "how far inland" | Base height/offset — higher continentalness generally means higher average terrain and keeps land above sea level  [minecraft](https://minecraft.wiki/w/World_generation) |
| Erosion | Terrain ruggedness | High erosion flattens land; low erosion makes it hilly/cliffy, and it governs underwater height too  [youtube](https://www.youtube.com/watch?v=sgOkNiU7FUU) |
| Peaks & Valleys (weirdness-derived) | Ridge/valley/plateau shaping | Adds local variation like mountain ridges and valley carving on top of the continentalness/erosion base  [youtube](https://www.youtube.com/watch?v=CSa5O6knuwI) |

Specifically, continentalness and erosion together (plus PV) feed into an "offset" value that shifts the terrain height up or down, and a separate "factor" value that scales how strongly 3D noise perturbs that base shape — both computed via splines keyed on all three noise values simultaneously. So in practice, continentalness sets the coarse land/ocean skeleton and general elevation tier, erosion smooths or roughens it, and PV carves the final ridge-and-valley detail — the combination is what produces Minecraft's varied biome-appropriate terrain (flat plains, jagged mountains, deep oceans) from just three noise fields. [minecraft](https://minecraft.wiki/w/Tutorial:Custom_world_generation)

This is architecturally similar to what you're already doing with your continentalness + P&V + erosion trio — Minecraft's implementation is essentially the reference design pattern you're following, just using spline-based multi-dimensional lookup tables instead of arithmetic blending.

---

in my repo i have the thought of using desired elevation as continentalness, is it ok, is it similar or close to mine craft continentalness?

Yes, using desired elevation as your continentalness value is a valid and closely-aligned simplification of Minecraft's approach — you're just skipping one layer of indirection that Minecraft keeps for flexibility.

## How Minecraft actually treats continentalness

It's crucial to note that in Minecraft, continentalness itself is **not** an elevation value — it's a raw Perlin noise scalar (roughly -1 to 1, extended to about -3.8 to 3.8 internally). That raw noise gets passed through a spline function which converts it into a height "offset," and that spline is also cross-referenced against erosion and peaks-and-valleys values before producing the final target elevation. So technically, continentalness in vanilla Minecraft is one axis of a lookup table, not the elevation itself. [reddit](https://www.reddit.com/r/VoxelGameDev/comments/th9t2i/how_does_minecraft_combine_these_3_spline_graph/)

## Why "desired elevation" as continentalness still works

Conceptually, though, continentalness functions as exactly what you're describing — a coarse target/base elevation signal that gets refined afterward. The spline's entire job is to translate "how far inland this is" into "what height this should roughly be" before erosion and PV add detail. If you collapse that indirection and directly author continentalness as your intended base elevation, you're not breaking the model — you're just pre-baking the spline's output into your input, which is a reasonable shortcut as long as you still let erosion and peaks-and-valleys modulate it afterward rather than treating your continentalness value as final. [dawnosaur.substack](https://dawnosaur.substack.com/p/how-minecraft-generates-worlds-you)

## Where it could diverge from Minecraft's design intent

| Minecraft's approach | Your "desired elevation" approach |
|---|---|
| Continentalness is abstract noise; height comes from a 3-noise spline lookup  [reddit](https://www.reddit.com/r/VoxelGameDev/comments/th9t2i/how_does_minecraft_combine_these_3_spline_graph/) | Continentalness directly encodes the target height, skipping the noise-to-height translation step |
| Same continentalness value can yield different heights depending on erosion/PV combo  [minecraft](https://minecraft.wiki/w/Tutorial:Custom_world_generation) | Risk: if erosion/PV are applied as simple additive/multiplicative offsets on top of a literal elevation, extreme erosion or PV values could push heights in ways that no longer look "desired" |
| Land/ocean threshold emerges from spline shape, tunable independent of the raw value  [minecraft.fandom](https://minecraft.fandom.com/wiki/Biome) | You'd need to bake your ocean/land threshold logic into how you interpret your elevation value directly |

The main thing to watch for: make sure erosion and P&V still act as *modifiers* on your desired-elevation base (flattening, roughening, adding ridges) rather than being ignored or fighting against a value you already consider "final." As long as that hierarchy holds — continentalness sets the target, erosion smooths/sharpens it, P&V adds ridge detail — your system stays functionally equivalent to Minecraft's design, just with a more literal, human-readable base layer instead of an abstract noise field passed through splines.

---

# yes, my DesiredElevation is not final elevation it's just a factor for final height, that will be affected by P&V and Erosion.
in its current state, i still wanna have a spline for continentalness (like minecraft), is it still relevant for my DesiredElevation?
because you said im prebaking the spline's output into my input.

Yes — a continentalness spline is still worth keeping even with DesiredElevation pre-baked as a proxy for target height. It's not redundant; it just does a different job than raw noise-to-height translation, and it's actually where a lot of Minecraft's terrain character comes from.

## Why the spline still matters

The reason Minecraft bothers with a spline at all, rather than mapping continentalness linearly to height, is that the spline encodes **non-monotonic, deliberately weird relationships** that a straight scale factor can't. The clearest example: mushroom fields spawn at the *lowest* continentalness values (below -1.05), but the terrain there rises up into land instead of continuing to sink into deeper ocean — a local minimum-then-spike shape in the curve, not a smooth line. If continentalness only scaled DesiredElevation linearly, you couldn't produce that kind of localized exception without a separate override system. [minecraft.fandom](https://minecraft.fandom.com/wiki/Mushroom_Fields)

## What the spline gives you that a plain factor doesn't

- Non-linear response curves — flat plateaus at certain continentalness ranges, sharp cliffs at threshold crossings, and local bumps like the mushroom-field spike [reddit](https://www.reddit.com/r/Minecraft/comments/1lwzkc1/weird_mushroom_fields_terrain_generation/)
- Multi-parameter blending — Minecraft's actual spline function takes continentalness alongside erosion and ridge/PV values simultaneously, not continentalness in isolation, so the same continentalness input yields different curve shapes depending on erosion state [maven.fabricmc](https://maven.fabricmc.net/docs/yarn-1.20.4+build.3/net/minecraft/util/math/class-use/Spline.html)
- Designer-authored control points — you can hand-place "at this continentalness, force this height regardless of the raw value" rules, which is much harder to fake with pure multiplication

## How this applies to your pre-baked DesiredElevation

Since your DesiredElevation is already a target-height-like value rather than abstract noise, your spline's x-axis input changes, but its purpose doesn't disappear — you'd spline DesiredElevation itself (rather than raw noise) into a refined offset, letting you inject those same non-linear touches (plateaus, dips, special-case spikes) on top of your prebaked value before erosion and P&V modulate it further. Practically: [mcasset](https://mcasset.cloud/1.21.11/data/minecraft/worldgen/density_function/overworld_large_biomes/factor.json)

| Without spline | With spline on DesiredElevation |
|---|---|
| DesiredElevation maps 1:1 (or via simple scale) to base height | DesiredElevation gets reshaped — e.g. compress mid-range values into flatter shelves, or punch in a coastal cliff right at your ocean/land threshold |
| No way to create localized exceptions (e.g. Minecraft's ocean-mushroom-spike) | Can define control points for special terrain behaviors at specific DesiredElevation bands |
| Threshold behavior (land vs ocean cutoff) is implicit in your raw formula | Threshold and transition sharpness become explicit, tunable control points |

So keep the spline — just think of it as reshaping your already-meaningful DesiredElevation curve rather than translating meaningless noise into meaning, which is a strictly easier and more intuitive authoring problem than what Minecraft's devs solved from scratch.

---

# i have a dilemmna about erosion:
1- i watched [Henrik Kniberg](https://www.youtube.com/@henrikkniberg)'s youtube video about minecraft terrain
[https://www.youtube.com/watch?v=CSa5O6knuwI&t=795s](https://www.youtube.com/watch?v=CSa5O6knuwI&t=795s)
he says that erosion in minecraft (is kinda abstracted as i understand) is used to flatten terrain, high erosion is flatter+lower terrain while low erosion will allow for higher terrain or lower and irregular (from contentalness and P&V), which brings me to understand that in my terrain is already obsolete, unless i had micro noise, so i don't really understand how to go from here.
2- another alternative is to use splines and interpolations
[https://codeplea.com/introduction-to-splines](https://codeplea.com/introduction-to-splines)
[https://interpolationcalculator.com/types-of-interpolation/](https://interpolationcalculator.com/types-of-interpolation/)
that cardinal trick can be considered erosion, higher erosion is low cardinal making the splines smoother less rugged, thinning mountains, flattening valeys; while low erosion is high cardinal giveing deeper valleys and more rugged terrain shape
but then i am confused as to what P&V would be for me.

Your read on erosion is actually spot-on for the "why it feels obsolete" part, and the spline-tension idea for erosion is a genuinely valid reinterpretation — but P&V is where the confusion is understandable, because it's doing a job that's easy to overlook: it's not a modifier, it's a *shape generator* in its own right.

## Why erosion feels obsolete in your system

Kniberg's explanation is specifically about Minecraft's implementation: erosion controls a "squashing factor" applied to 3D density noise, where high erosion suppresses the 3D noise almost entirely (flat terrain) and low erosion lets the raw 3D noise show through as jagged, chaotic shapes. Since your terrain height comes from continuous spline/interpolation across Voronoi vertices rather than a 3D density field being carved away, that specific mechanism genuinely doesn't map onto your pipeline — you're right that without an equivalent "noise field to suppress or expose," erosion-as-squashing-factor has nothing to act on. [youtube](https://www.youtube.com/watch?v=CSa5O6knuwI)

## Your spline-tension idea for erosion is reasonable

Reinterpreting erosion as a cardinal spline tension parameter — high erosion = low tension = smoother, more relaxed curves between vertices; low erosion = high tension = sharper, more overshoot-prone curves — is a legitimate translation of the *intent* (erosion = flatness control) into a mechanism that actually exists in your architecture (spline-based vertex interpolation). This preserves the causal relationship Minecraft has (high erosion → flat, low erosion → dramatic) even though the underlying math is completely different from squashing 3D noise. [youtube](https://www.youtube.com/watch?v=CSa5O6knuwI)

# What Peaks & Valleys actually is (and why it's separate from continentalness)

This is the key piece that clarifies your confusion: P&V isn't a modifier of continentalness at all — it's an **independent noise field**, run through its own folding formula: PV = 1 − abs((3×abs(weirdness)) − 2), which takes a single "weirdness" noise and folds it into repeating valley→plain→ridge→plain→valley bands. This creates a self-repeating ridge-and-valley pattern across the whole world, completely decoupled from whether you're in an ocean or deep inland — it's what makes mountain ranges and river-valley troughs happen in specific *bands*, not just wherever continentalness happens to be high. [reddit](https://www.reddit.com/r/VoxelGameDev/comments/1f01v6o/minecraft_noise_maps_and_how_do_they_generate/)

Crucially, P&V, continentalness, and erosion are combined together into three separate outputs — offset (base height), factor (3D noise strength, tied to erosion), and jaggedness (extra height added specifically at peaks) — and jaggedness only kicks in when erosion is low and PV is high, meaning **P&V's ridge effect is gated by erosion**, not independent of it. [reddit](https://www.reddit.com/r/minecraft_configs/comments/1mxyfrt/help_making_island_world_generation/)

## How to translate P&V into your system

Since you don't have 3D density carving, think of P&V as a second, independent elevation-contribution noise assigned per Voronoi site (separately from DesiredElevation), which represents local ridge/valley folding rather than macro land distribution:

| Concept | Minecraft mechanism | Your equivalent |
|---|---|---|
| Continentalness | Spline → base offset height | DesiredElevation → spline → base height (already have this) |
| Erosion | Squashes/exposes 3D noise (flat vs. jagged) | Spline tension/cardinal parameter controlling interpolation smoothness |
| Peaks & Valleys | Folded noise creating repeating ridge-valley bands, added as "jaggedness" bonus gated by erosion | A second per-site noise value (ridged/folded) added as an extra height term, whose magnitude is scaled by your erosion/tension parameter |

Concretely: assign each Voronoi site a P&V value from a folded/ridged noise (same fold formula works fine: 1 − abs(3×abs(x) − 2)), then compute final height roughly as spline(DesiredElevation) + jaggedness(P&V) × gate(erosion), where gate(erosion) is small when erosion/tension is high (flat, ridges suppressed) and large when erosion/tension is low (ridges pop out dramatically). This keeps P&V doing what it's actually supposed to do — generating the *ridge/valley shape itself* — rather than trying to fold it into your continentalness or erosion logic, which is likely the source of your confusion since those two already have clear jobs in your pipeline.

---

## What P&V actually is

Peaks & Valleys (PV) is a single derived value computed from a separate raw noise field called **weirdness**. It's not its own independent Perlin noise — it's a deterministic fold/transform applied to weirdness. [minecraft.fandom](https://minecraft.fandom.com/wiki/Debug_screen)

## The exact formula

Minecraft's actual formula, straight from the noise router source:

\[
PV = \left(-\left|\,|W| - \frac{2}{3}\,\right| + \frac{1}{3}\right) \times 3
\]

where \(W\) is the raw weirdness noise value, sampled independently (its own frequency/octave config, range roughly -1 to 1). [minecraft.fandom](https://minecraft.fandom.com/wiki/Debug_screen)

This produces a folded, repeating M/W-shaped curve as weirdness sweeps from -1 to 1: PV is lowest (valley) at \(W=0\), rises to a peak around \(|W|=2/3\), then folds back down again toward \(|W|=1\)  [youtube](https://www.youtube.com/watch?v=sLRX7CgkFpM). That's why it's called "folded" — the same PV output value can come from two different weirdness inputs, symmetric around zero.

## What each PV band means

| PV value range | Named zone | Terrain result |
|---|---|---|
| < 0.05 | Valley | Lowest terrain, river placement  [minecraft.fandom](https://minecraft.fandom.com/wiki/Debug_screen) |
| 0.05 – 0.267 | Low | Gently low terrain |
| 0.267 – 0.4 | Mid | Moderate elevation |
| 0.4 – 0.567 | High | Elevated terrain |
| > 0.567 | Peak | Highest terrain, mountain tops  [minecraft.fandom](https://minecraft.fandom.com/wiki/Debug_screen) |

## How to calculate it for your system

1. Generate a **weirdness noise field** — a standalone Perlin/simplex noise, sampled per Voronoi site, completely independent of continentalness and erosion (different seed offset, different frequency)
2. Apply the fold formula above to get PV per site
3. Feed PV into your height formula as an additive/multiplicative contribution — this is the part that's independent of your tectonic plate boundaries, unlike Gainey's boundary-driven mountains

So concretely: yes, you need to generate a new noise field for weirdness, then run it through that fold equation to get PV — this is not something Gainey's plate-boundary-stress system gives you for free, since his ridges only exist at plate edges, while PV/weirdness can produce ridges and valleys anywhere on the sphere regardless of plate structure.

---

# MINECRAFT - TENJINX PIPELINE

The actual pipeline order
Your full stack should run strictly in this sequence, with each stage only reading outputs from the stage before it:

Continentalness spline → base target elevation per site

Erosion (as spline tension) → smooths or sharpens the interpolation between vertices

Peaks & Valleys (folded ridge noise) → adds ridge/valley detail gated by erosion, producing final elevation

Temperature → derived from equator distance (latitude) plus elevation-based lapse rate on your finished heightmap

Circulation/wind → pressure-difference flow field deflected by Coriolis effect, computed independently of your terrain generator, just using elevation and temperature as inputs

Evapotranspiration & humidity → driven by moisture, temperature, and wind speed; humidity gets carried by the circulation flow-field, accumulating over water and dispersing over land

Precipitation → function of temperature, humidity, and the orographic effect (rain increases where air is forced upward by rising terrain, i.e. mountains you already generated in step 3)

Rivers → not part of Tenjix's original thesis pipeline explicitly, but naturally layered on top: once precipitation is known per cell, standard downhill-flow/steepest-descent river tracing runs on your finished elevation map, accumulating water from high-precipitation, high-elevation sources down to the ocean