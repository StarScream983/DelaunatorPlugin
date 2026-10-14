The script runs itself on the last line. `resize()` calls `build()`, then `draw()`.

```588:589:Plugins/DelaunatorPlugin/Docs/Foliage/neighbors_v2_0.html
window.addEventListener('resize', resize);
resize();
```

That last `resize()` runs as soon as the browser finishes reading the script. It also runs when the window changes size, and when you move any slider. Moving the mouse does not rebuild. It only redraws.

`build()` makes the triangles in this order:

These are the six base triangles on a 1000 by 700 page. The corner numbers move if the window size changes. The links do not.

The seven corners are:

| Point | x | y |
|---|---|---|
| P0 | 137.4 | 350 |
| P1 | 318.7 | 36 |
| P2 | 500 | 350 |
| P3 | 681.3 | 36 |
| P4 | 862.6 | 350 |
| P1b | 318.7 | 664 |
| P3b | 681.3 | 664 |

```javascript
A = {                                    // top-left triangle
  v: [ P0, P2, P1 ],                     // corner 0 is P0 left-middle, corner 1 is P2 center, corner 2 is P1 top-left peak
  base: "A",                             // this triangle's name
  depth: 0,                              // a base triangle, not a split child
  child: null,                           // no children yet
  n:  [ B, null, D ],                    // side P2-P1 points at B, side P1-P0 points nowhere, side P0-P2 points at D
  ne: [ 1, 1, 2 ],                       // B is met on B's side 1, the middle value is unused, D is met on D's side 2
  flip: [ true, false, false ],          // P2-P1 runs backward along B, the other two sides do not
  t0: [ 0, 0, 0 ],                       // each side starts at the beginning of the neighbor side
  t1: [ 1, 1, 1 ],                       // each side ends at the end of the neighbor side, so it covers the whole side
  pier: [ null, null, null ]             // no midpoint link on any corner yet
}
B = {                                    // top-middle triangle
  v: [ P2, P3, P1 ],                     // corner 0 is P2 center, corner 1 is P3 top-right peak, corner 2 is P1 top-left peak
  base: "B",                             // this triangle's name
  depth: 0,                              // a base triangle, not a split child
  child: null,                           // no children yet
  n:  [ null, A, C ],                    // side P3-P1 points nowhere, side P1-P2 points at A, side P2-P3 points at C
  ne: [ 0, 0, 1 ],                       // the first value is unused, A is met on A's side 0, C is met on C's side 1
  flip: [ false, true, true ],           // P1-P2 runs backward along A, P2-P3 runs backward along C
  t0: [ 0, 0, 0 ],                       // each side starts at the beginning of the neighbor side
  t1: [ 1, 1, 1 ],                       // each side covers the whole neighbor side
  pier: [ null, null, null ]             // no midpoint link on any corner yet
}
C = {                                    // top-right triangle
  v: [ P2, P4, P3 ],                     // corner 0 is P2 center, corner 1 is P4 right-middle, corner 2 is P3 top-right peak
  base: "C",                             // this triangle's name
  depth: 0,                              // a base triangle, not a split child
  child: null,                           // no children yet
  n:  [ null, B, F ],                    // side P4-P3 points nowhere, side P3-P2 points at B, side P2-P4 points at F
  ne: [ 0, 2, 2 ],                       // the first value is unused, B is met on B's side 2, F is met on F's side 2
  flip: [ false, true, false ],          // P3-P2 runs backward along B, P2-P4 runs the same way as F
  t0: [ 0, 0, 0 ],                       // each side starts at the beginning of the neighbor side
  t1: [ 1, 1, 1 ],                       // each side covers the whole neighbor side
  pier: [ null, null, null ]             // no midpoint link on any corner yet
}
D = {                                    // bottom-left triangle
  v: [ P0, P2, P1b ],                    // corner 0 is P0 left-middle, corner 1 is P2 center, corner 2 is P1b bottom-left peak
  base: "D",                             // this triangle's name
  depth: 0,                              // a base triangle, not a split child
  child: null,                           // no children yet
  n:  [ E, null, A ],                    // side P2-P1b points at E, side P1b-P0 points nowhere, side P0-P2 points at A
  ne: [ 1, 1, 2 ],                       // E is met on E's side 1, the middle value is unused, A is met on A's side 2
  flip: [ true, false, false ],          // P2-P1b runs backward along E, P0-P2 runs the same way as A
  t0: [ 0, 0, 0 ],                       // each side starts at the beginning of the neighbor side
  t1: [ 1, 1, 1 ],                       // each side covers the whole neighbor side
  pier: [ null, null, null ]             // no midpoint link on any corner yet
}
E = {                                    // bottom-middle triangle
  v: [ P2, P3b, P1b ],                   // corner 0 is P2 center, corner 1 is P3b bottom-right peak, corner 2 is P1b bottom-left peak
  base: "E",                             // this triangle's name
  depth: 0,                              // a base triangle, not a split child
  child: null,                           // no children yet
  n:  [ null, D, F ],                    // side P3b-P1b points nowhere, side P1b-P2 points at D, side P2-P3b points at F
  ne: [ 0, 0, 1 ],                       // the first value is unused, D is met on D's side 0, F is met on F's side 1
  flip: [ false, true, true ],           // P1b-P2 runs backward along D, P2-P3b runs backward along F
  t0: [ 0, 0, 0 ],                       // each side starts at the beginning of the neighbor side
  t1: [ 1, 1, 1 ],                       // each side covers the whole neighbor side
  pier: [ null, null, null ]             // no midpoint link on any corner yet
}
F = {                                    // bottom-right triangle
  v: [ P2, P4, P3b ],                    // corner 0 is P2 center, corner 1 is P4 right-middle, corner 2 is P3b bottom-right peak
  base: "F",                             // this triangle's name
  depth: 0,                              // a base triangle, not a split child
  child: null,                           // no children yet
  n:  [ null, E, C ],                    // side P4-P3b points nowhere, side P3b-P2 points at E, side P2-P4 points at C
  ne: [ 0, 2, 2 ],                       // the first value is unused, E is met on E's side 2, C is met on C's side 2
  flip: [ false, true, false ],          // P3b-P2 runs backward along E, P2-P4 runs the same way as C
  t0: [ 0, 0, 0 ],                       // each side starts at the beginning of the neighbor side
  t1: [ 1, 1, 1 ],                       // each side covers the whole neighbor side
  pier: [ null, null, null ]             // no midpoint link on any corner yet
}
```

Side 0 is the side between corner 1 and corner 2. Side 1 is between corner 2 and corner 0. Side 2 is between corner 0 and corner 1. A null side is an outside edge.