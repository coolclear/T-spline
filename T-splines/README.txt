General
-------

There are two windows (originally three) for this program:
- Topology + the headquarter,
- Geometry: mesh/surface rendering.

Recently, the geometry window and the surface rendering window were separate.
Now they have been combined into one. Moreover, the surface will be
automatically refreshed after changes are made to the control points.

There is an option to provide only necessary knots values (in the middle) and
the program will duplicate the rest at both ends. Use the flag 1 for this
option (see the file format below).


Controls
--------

For how to manipulate cameras (in the geometry and rendering windows) and how
to manipulate objects (in the geometry window), please refer to

https://www.clear.rice.edu/comp360/lab_raytracer/lab_raytracer.html

Note that the focus must be on that window before you can use it.

Topology window:
- save/load buttons

Mesh/Surface window:
- f: toggle wireframe (on/off)
- g: toggle shading mode (flat/smooth)
- v: toggle coloring mode (6 lights/surface normal)

- z: toggle main axes (RGB)
- x: toggle control points
- c: toggle the T-spline surface


The T-mesh file format
----------------------

Numbers should be separated by at least one whitespace, but they may lie on any
line as long as they follow the correct order ('+' for data, '-' for comments):

+ dimensions: rows R and columns C (2 nonnegative integers)
  - At least one dimension must be positive.
  - There may be a limit to the grid size (max 10^4).

+ degrees: deg_V and deg_H (2 nonnegative integers)
  - For each dimension, if the dimension is 0 (unused), the degree must be 0.
    If the dimension = n, the degree must be between 1 and n.

- grid line information
  + horizontal lines ((R-1) x C boolean values)
  + vertical lines (R x (C-1) boolean values)
  - The lines on the boundary are turned on by default and need not be input.

- knot values
  + duplicate flag: dupH (1 boolean value: 0/1)
  + horizontal values:
    - dupH == 0: C + deg_H doubles
    - dupH == 1: C - deg_H + 2 doubles (0 if empty dimension)
  + duplicate flag: dupV (1 boolean value: 0/1)
  + vertical values:
    - dupV == 0: R + deg_V doubles
    - dupV == 1: R - deg_V + 2 doubles (0 if empty dimension)
  - There will be no knot value for an empty dimension (n + n_deg = 0)
  - Both duplicate flags are required, even if the dimension is empty.
  - Knot values (for each dimension) must be non-decreasing. E.g., duplicate
    knots are allowed.

+ complete control points ((R+1) x (C+1) x 3 doubles)
  - The coordinates are (x, y, z) as usual.
  - Even though some grid points may not exist for a specific T-mesh, we will
    still keep the values as placeholders, for simplicity.

+ the final END tag (string "END")
  - This tag helps ensure that the input terminates properly.

Everything else that comes after the END tag will be ignored by the program, so
we may write any comment for the mesh file at the end.

When saving the file, be aware that the program will discard anything beyond
the END tag.