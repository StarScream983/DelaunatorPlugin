# Voronoi Planet — Implementation Status

Living document. Update whenever a subsystem changes. Each subsystem entry
captures: *purpose · key files · algorithm + reference · current status ·
known limitations · next steps*.

> Reference priority: Gainey → squeakyspacebar → Red Blob.
> See `References.md` for URLs and the per‑step table.

---

## Conventions for entries

- Date entries `YYYY‑MM‑DD` when adding/updating significant sections.
- Status tags: `not started` · `in progress` · `working (basic)` ·
  `working (full)` · `needs rework`.
- When a step deviates from Gainey, **state which source it follows and why**.
- File paths should be relative to the repo root.

---

## 1. Sphere point distribution

- **Purpose:** generate the base set of points on the sphere that will feed the
  Delaunay step. **Not** an icosahedron subdivision (this is the explicit
  deviation from Gainey).
- **Reference:** Red Blob *1843‑planet‑generation* — Fibonacci‑sphere /
  jittered point distribution.
- **Key files:** _TBD — fill in as code lands._
- **Status:** not started.
- **Notes:** keep the "south pole" point identity tracked here so step 2 can
  re‑inject it after the planar triangulation.

## 2. Spherical Delaunay — Delaunator + south‑pole stitching

- **Purpose:** triangulate the sphere points into the primal mesh.
- **Approach (deviates from Gainey):**
  1. Stereographic projection of all sphere points onto the plane (the south
     pole is the projection center and is excluded from this step — it would
     map to infinity).
  2. Run **Mapbox Delaunator** (2D) on the projected points → planar Delaunay.
  3. **Fil's south‑pole stitching:** add the south pole back in by connecting
     it to the convex hull of the planar triangulation, producing a closed
     spherical triangulation with no missing cap.
- **Reference:** Red Blob *1843‑planet‑generation*; Philippe Rivière (Fil) for
  the stitching trick — see `References.md`.
- **Key files:** `Plugins/DelaunatorPlugin/Source/DelaunatorPlugin/...`
  (existing 2D Delaunator port is the starting point; the stereographic +
  stitching layer is what wraps it for sphere use).
- **Status:** not started (Delaunator core exists; sphere wrapper TBD).
- **Notes:**
  - This is why the plugin is named `DelaunatorPlugin` — the sphere pipeline
    is built around Mapbox Delaunator, not Gainey's icosahedron subdivision.
  - Pole choice is arbitrary; document which pole is used as the projection
    center once decided (convention: project from the south pole, stitch the
    south pole back as the "infinity" cell).

## 3. Voronoi dual mesh

- **Purpose:** dual of the Delaunay triangulation; the cells of the planet.
- **Reference:** Gainey; Red Blob for half‑edge structure.
- **Key files:** _TBD_
- **Status:** not started.
- **Notes:** —

## 4. Tectonic plates — seeding & assignment

- **Purpose:** partition cells into plates.
- **Reference:** squeakyspacebar (preferred for code clarity).
- **Key files:** _TBD_
- **Status:** not started.
- **Notes:** —

## 5. Plate motion & boundary classification

- **Purpose:** convergent / divergent / transform boundaries → elevation drivers.
- **Reference:** Gainey.
- **Key files:** _TBD_
- **Status:** not started.
- **Notes:** —

## 6. Elevation field

- **Purpose:** per‑cell elevation from boundary stresses + noise.
- **Reference:** Gainey.
- **Key files:** _TBD_
- **Status:** not started.
- **Notes:** —

## 7. Climate (temperature, moisture, wind)

- **Purpose:** climate inputs for biome assignment.
- **Reference:** Gainey; Red Blob for simplifications.
- **Key files:** _TBD_
- **Status:** not started.
- **Notes:** —

## 8. Biome assignment

- **Purpose:** map (elevation, temperature, moisture) → biome class.
- **Reference:** Red Blob (Whittaker‑diagram style).
- **Key files:** _TBD_
- **Status:** not started.
- **Notes:** —

## 9. Rendering — GPU indirect instancing

- **Purpose:** draw the planet cells / vegetation / props efficiently.
- **Key files:**
  - `Plugins/DelaunatorPlugin/Source/IndirectInstancingCore/...`
  - `Plugins/IndirectInstancing_Plugin/...`
- **Status:** _existing scaffolding, document current capability here_.
- **Notes:** the second plugin's `.uplugin` is named
  `ShadeupExamplePlugin.uplugin` (forked from a Shadeup sample).

## 10. Large mesh handling — Concurrent Binary Tree

- **Purpose:** subdivision / LOD for large meshes (planet surface).
- **Key files:** `Plugins/DelaunatorPlugin/Source/Large_CBT/...`
- **Status:** _document current capability here_.
- **Notes:** —

## 11. Debug & visualization

- **Purpose:** ImGui overlays, debug draw, on‑sphere visualizations.
- **Reference:** Red Blob for visualization tricks.
- **Key files:** uses `Plugins/Unreal5-ImGui/`.
- **Status:** _document current capability here_.
- **Notes:** —

---

## Change log

(Append dated bullet points as substantive changes happen. Keep them short —
the per‑subsystem sections above are the source of truth.)

- _YYYY‑MM‑DD — initial skeleton._
