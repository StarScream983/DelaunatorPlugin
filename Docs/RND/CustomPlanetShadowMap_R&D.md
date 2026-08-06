# PLANETARY SHADOWS:

## PIPELINE
3 passes before UE5's GBuffer N.L shadows or CSM:
- planet terrain shadow map pass.
- clouds coverage map pass.
- far objects shadows, either moons of km away ships.

## PLANET TERRAIN SHADOW MAP PASS
take base triangles, of the planet, which have one of their vertices on the hemisphere facing the sun, and which face normal is away from the sun, project them on a hypothetical quad plane centered on planet center, plane covers the whole planet diameter an more(because terrain height), the plane is not a texture, it word as spatial hashing but each quad stores the projected triangles as polygons in a winding order which helps figure out the lit and dark part of the quad; after all triangles are projected, each terrain pixel inside triangles is projected on the plane finds its quad, fetches the polygon 
line (float2: vector, float: offset, float intensity)
corner (float2: )


## POSSIBLE UPDATES:
- project triangles that are on the lit hemisphes and player direction hemisphere
- repeat pipeline for many suns
- would this be realistic for night moon shadows?