# Roadmap

Mesh Weaver stays on the main line. `bUseMeshWeaver` switches Weaver vs the old indirect-instancing VF. II is not deleted.

Order below is the work sequence. Do not skip ahead to GPU CBT or screen-density tessellation until CPU CBT + collision + gravity character are walkable.

---

## 0. Keep (done)

- Mesh Weaver on main, switchable (`bUseMeshWeaver`, default on).
- Lit / unlit (`bMeshWeaverUnlit`) and ColorViewMode (ImGui *Planet color debug*).
- Coarse Chaos collision on the Delaunay shell (`Collision.md`). Not the Voronoi GPU mesh.

---

## 1. CBT on CPU

Study the existing Large_CBT / bisector code, then **run subdivision on the CPU**.

Goal: a CPU leaf triangle list (subdivided sites), not just the coarse Fibonacci / Delaunay shell.

This is the source of truth for collision. GPU CBT comes later and must not be the only copy of the mesh.

---

## 2. Update collision from CBT sites

Recook Chaos from the **CPU CBT leaves** (same radial elevation formula as today: `PlanetRadius * (1 + Elev * 0.03)`).

Still `GetPhysicsTriMeshData` → `CreatePhysicsMeshes` → `RecreatePhysicsState`. Finer mesh = walkable terraces closer to the render mesh.

Do not recook the whole planet every frame. Neighborhood / async BodySetup swap is a later optimization (`Collision.md`).

---

## 3. Gravity character

Planet-centered gravity (attract toward planet origin) and a character that can stand on the CPU CBT collision.

Do this **after** collision matches the subdivided shell. Gravity without a walkable floor is wasted.

No camera / near-clip hacks. Collision is the floor.

---

## 4. CBT on GPU

Move the same subdivision to GPU (compute / existing Large_CBT GPU path) so the **render** mesh can go denser than the CPU collision mesh.

Collision stays on the CPU CBT (or a coarser CPU subset). Do not read back GPU leaves to Chaos every frame.

---

## 5. Visualize Minecraft height

High-frequency height (`ApplyMinecraftDetailToElevation` / erosion axis) only reads once triangles are small enough.

GPU CBT leaves + Weaver ColorViewMode (elevation / erosion / land distance) + unlit for heatmaps.

---

## 6. Experimental screen-density tessellation (last)

Drive CBT depth from **screen error / pixel density**, not a second tessellator.

Orbit: coarse. Approach: subdiv ramps. Ground: dense enough to drop extra tricks.

This is last because it needs GPU CBT working and a character on the surface to judge density.

---

## Out of scope until later

- Shadow-depth pass for Weaver (planet does not cast yet).
- `EncodeGBuffer` polish / proper deferred packing edge cases.
- Async neighborhood collision recook.
- Merging Weaver and II into one proxy.
