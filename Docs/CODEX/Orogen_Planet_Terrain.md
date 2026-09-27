[https://www.orogen.studio/#02s35zwroxys1yymtihpk5](https://www.orogen.studio/#02s35zwroxys1yymtihpk5)
[https://github.com/raguilar011095/planet_heightmap_generation](https://github.com/raguilar011095/planet_heightmap_generation)
i wanna study this voronoi planet generator, while this is my current code
[https://github.com/StarScream983/DelaunatorPlugin](https://github.com/StarScream983/DelaunatorPlugin)
my plan was to make a height map like minecraft i have continentalness, P&V, and erosion.
just to expand my horizons and explore new possibilities i wanted to study the raguilar repo, he has continentalness, what is it in his planet, how does he calculate it, and how does it affect height?

Based on what's documented in the raguilar011095/planet_heightmap_generation ("World Orogen") repo, here's how continentality fits into that pipeline, and how it compares to your own Voronoi/Delaunator approach.

## What continentality is in World Orogen

Continentality isn't a height-generating term there — it's a **climate modifier**, not a terrain-generation input like your P&V/erosion stack. The repo describes it as a derived value: distance from ocean/coast, classified into zones (blue = ocean, green = coast, yellow = moderate interior, orange/red = deep continental interior). It's used purely to scale how much temperature swings seasonally at a given location, not to shape elevation directly. [github](https://github.com/raguilar011095/planet_heightmap_generation)

## How it's calculated

Continentality is computed as a **distance-from-coast field** derived via BFS from ocean/land boundaries — it's one of the "distance fields" the pipeline builds (alongside mountain-distance and ocean-distance fields used for elevation). The commit history shows this was explicitly reworked as "zone-based temperature continentality following geographic guide," with tuned Köppen thresholds, implying it buckets cells into discrete bands rather than a smooth continuous gradient. [github](https://github.com/raguilar011095/planet_heightmap_generation)

## How it affects height (short answer: it doesn't, directly)

This is the key distinction from your Minecraft-style stack: in World Orogen, **continentality only feeds temperature and precipitation**, not elevation. Specifically: [github](https://github.com/raguilar011095/planet_heightmap_generation)

- Coast cells get a low "maritime factor" (~0.50x seasonal swing)
- Deep interior cells get a high factor (~1.20x seasonal swing)
- This modulates the temperature simulation, which uses ITCZ position, latitude, elevation lapse rate, and ocean-current warmth as the main inputs, with continentality just scaling the *seasonal amplitude* on top of that [github](https://github.com/raguilar011095/planet_heightmap_generation)

Elevation itself in World Orogen comes from a completely separate pipeline: three distance fields (mountain/ocean/coastline) combined via a `harmonic-mean formula`, plus stress-driven uplift from plate collisions, hotspot volcanism, island arcs, and erosion post-processing. Continentality plays no role in that elevation formula — it's purely a post-elevation climate variable. [github](https://github.com/raguilar011095/planet_heightmap_generation)

## Why this matters for your Minecraft-style heightmap

Your setup (continentalness + P&V + erosion) treats continentality as a **primary elevation-shaping noise axis**, à la Minecraft's terrain generation — it directly biases height toward land/ocean before erosion runs. World Orogen instead gets its ocean/land shape from tectonic plate simulation and distance-field elevation, and only introduces a "continentality" concept afterward, purely for climate realism (rainfall/temperature swings), not height.

| Aspect | Your system (Minecraft-style) | World Orogen |
|---|---|---|
| Continentalness role | Primary elevation-shaping axis | Climate-only (temperature/precip modifier) |
| Land/ocean origin | Continentalness noise | Tectonic plate + farthest-point continent seeding |
| Height formula input | Continentalness + P&V + erosion | Harmonic-mean of mountain/ocean/coast distance fields + stress uplift |
| Erosion | Separate stage | Separate stage (glacial, hydraulic, thermal, ridge sharpening) |

If you want to "borrow" the World Orogen idea for your own heightmap, the useful takeaway isn't the continentality math itself (since it doesn't touch height) — it's their **distance-field-based elevation approach**: computing separate BFS distance fields for ocean, mountains, and coastline, then blending them with a `harmonic mean`, which gives smoother, more geologically plausible transitions than a raw continentalness spline. That could be a genuinely new tool to add alongside your existing P&V/erosion stack rather than a replacement for continentalness.

Do you want me to dig into the exact harmonic-mean elevation formula they use, since that's the piece most transferable to your Delaunator-based system?

---

