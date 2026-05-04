# Voronoi Planet — External References

Sources used to derive the planet generation pipeline, in priority order.

## Priority

**Andy Gainey is the primary reference.** Reuse his pipeline structure
(subdivided icosahedron → Delaunay → Voronoi dual → tectonics → climate →
biomes). Substitute squeakyspacebar or Red Blob Games only when their code is
cleaner / better suited for a specific step. Red Blob is largely a simplified
re‑implementation of Gainey's approach.

When a step is implemented from a non‑primary source, log which one in
`VORONOI_PLANET_Implementation.md`.

---

## 1. Andy Gainey  (primary)

- **Blog write‑up** — full algorithm walkthrough, the source‑of‑truth
  description of every pipeline stage:
  http://experilous.com/1/blog/post/procedural-planet-generation
- **Original JS implementation (Wayback pin)** — the reference implementation
  the blog post describes; Gainey's live site is unreliable, so use this:
  https://web.archive.org/web/20200531054229/http://experilous.com/1/project/planet-generator/2014-09-28/planet-generator.js

## 2. squeakyspacebar  (use when its code beats Gainey's)

- **Blog post** — *Procedural Map Generation With Voronoi Diagrams*. Strong
  treatment of plate generation / assignment and Lloyd relaxation:
  https://squeakyspacebar.github.io/2017/07/12/Procedural-Map-Generation-With-Voronoi-Diagrams.html
- **`novatellus` repo** — Python implementation accompanying the post:
  https://github.com/squeakyspacebar/novatellus/tree/develop

## 3. Red Blob Games  (clarity / dual‑mesh details)

- **Observable notebook** — *1843 Planet Generation*. Simplified, well‑visualised
  port of Gainey's pipeline; useful when the original is hard to follow:
  https://www.redblobgames.com/x/1843-planet-generation/
- **Companion repo**:
  https://github.com/redblobgames/1843-planet-generation/


# BONUS REPO
 - https://namishh.com/blog/devlogs/planet
 - https://github.com/namishh/planet/tree/master

---


# BIOME IMPLEMENTATION:

we will use Pressure based Circulation as base implementation for biomes.
https://docs.google.com/viewerng/viewer?url=https://tenjix.de/content/projects/climate-based-biomes/Thesis+(Dark+Version).pdf
https://github.com/Tenjix/Sethex
https://www.youtube.com/watch?v=dQQqRKGbNKY&t=85s
