Raycasting engine tech demo
----------------------------

This is a simple raycast engine implementation in C using SDL2 library.
The engine is just a proof of concept, created by my curiosity for raycasting technology and it is not intended to be anything more.
The code is well commented, so maybe it will help someone else with similar interests.

All the assets like sprites and textures are stored in static arrays in header files.
To create those header files a simple converter was created in texture_converter directory. This converter converts a GIMP exported .ppm file to .h header file with static uint32 pixel array
Most of the textures and sprites were extracted from shareware version of Wolfenstein 3D. All credit goes to ID software.

Key features:
----------------------
1. Textured walls, floors and ceilings.
2. Added texture dimming according to distance for better visual depth perception.
3. Wall and not walk-thru sprites collision detection.
4. Sprites with transparent pixels (magenta color).
5. Simple hud (not functional) with weapon and crosshair.

