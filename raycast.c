/***********************************************************************************************************************
 *                             Simple technology demonstration of raycasting engine mechanics                          *
 *                             - This program uses SDL2 library https://www.libsdl.org/                                *
 *                             - All engine assets (textures, sprites) are pixel defined in their .h files             *
 *                             - Feel free to use as you like                                                          *
 ***********************************************************************************************************************/

// Windows specific macro for proper main() call
#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif

// Platform-specific SDL includes
#ifdef __APPLE__
    #include <SDL.h>                                                    // macOS SDL header location
#else
    #include <SDL2/SDL.h>                                               // Linux/Windows SDL header location
#endif

// Standard library includes
#include <stdio.h>                                                      // Standard input/output functions
#include <stdlib.h>                                                     // Memory allocation and utility functions
#include <stdbool.h>                                                    // Boolean type support
#include <stdint.h>                                                     // Fixed-width integer types
#include <math.h>                                                       // Mathematical functions
#include <string.h>                                                     // String manipulation functions

// Custom assets
#include "asset/assets.h"                                               // Textures and sprites

// Screen and rendering constants
#define SCREEN_WIDTH 1024                                               // Window width in pixels
#define SCREEN_HEIGHT 512                                               // Window height in pixels
#define FOV 60                                                          // Field of view in degrees
#define RAY_COUNT 256                                                   // Number of rays to cast (power of 2 for efficiency)
#define TEXTURE_SIZE 64                                                 // Size of texture arrays (64x64 pixels)
#define PI 3.14159265359f                                               // Pi constant for trigonometric calculations

// Distance-based lighting configuration (higher values = darker at distance)
#define WALL_DISTANCE_DIMMING 15.0f                                     // How quickly walls get dark with distance
#define FLOOR_DISTANCE_DIMMING 15.0f                                    // How quickly floor gets dark with distance  
#define CEILING_DISTANCE_DIMMING 15.0f                                  // How quickly ceiling gets dark with distance
#define SPRITE_DISTANCE_DIMMING 11.0f                                   // How quickly sprites get dark with distance

// Minimum brightness levels (0.0 = black, 1.0 = full brightness)
#define WALL_MIN_BRIGHTNESS 0.7f                                        // Minimum wall brightness at far distances
#define FLOOR_MIN_BRIGHTNESS 0.65f                                      // Minimum floor brightness at far distances
#define CEILING_MIN_BRIGHTNESS 0.65f                                    // Minimum ceiling brightness at far distances  
#define SPRITE_MIN_BRIGHTNESS 0.7f                                      // Minimum sprite brightness at far distances

// Map configuration constants
#define MAPX 8                                                          // Map width in cells
#define MAPY 8                                                          // Map height in cells  
#define MAP_CELL_SIZE 64                                                // Size of each map cell in pixels

// Static map layout (0 = empty space, 1 - stone wall, 2 - mossy stone wall, 3 - color stone wall)
static const char map[] = {
    3,3,3,3,3,1,1,1,                  
    3,0,0,0,0,1,0,1,                  
    3,0,0,0,0,0,0,1,                  
    3,0,0,0,0,0,0,1,                  
    3,3,0,0,0,0,2,1,                  
    1,0,0,0,0,2,2,3,                  
    1,0,0,0,0,0,0,3,                  
    1,1,1,1,1,1,1,3,                  
};

// Static sprite layout (0 = no sprite, 1 - hangman, 2 - barrel, 3 - armor_suit, 4 - bed, 5 - plant, 6 - sink, 7 - dead_plant, 8 - light)
static const char map_sprites[] = {
    0,0,0,0,0,0,0,0,                  
    0,2,0,0,5,0,6,0,                  
    0,0,0,8,0,0,8,0,                  
    0,3,0,0,0,0,7,0,                  
    0,0,0,0,0,1,0,0,                  
    0,0,0,8,0,0,0,0,                  
    0,2,0,0,0,8,4,0,                  
    0,0,0,0,0,0,0,0,                  
};

// Global variables
uint32_t *pixels = NULL;                                                // Framebuffer for pixel data
bool engine_on = true;                                                  // Main game loop control flag

// Player structure definition
struct Player {
    float x;                                                            // Player X position in world coordinates
    float y;                                                            // Player Y position in world coordinates
    float dx;                                                           // X component of direction vector
    float dy;                                                           // Y component of direction vector
    float angle;                                                        // Player facing angle in degrees
    float rays_d[RAY_COUNT];                                            // Array storing distances for each ray
};

// Initialize player with starting values
struct Player player = {
    .x = 200,                                                           // Starting X position
    .y = 195,                                                           // Starting Y position
    .angle = 295.0,                                                     // Starting angle (facing north)
    .dx = 0.423,                                                        // cos(295°) ≈ 0.423
    .dy = 0.906                                                         // -sin(295°) ≈ 0.906
};

// Struct for sprite render data
typedef struct {
    float x, y;                                                         // World position
    float dist;                                                         // Distance from player
    int type;                                                           // Sprite type
} Sprite;

// Function declarations
void rungame(SDL_Renderer *renderer);                                   // Main game loop
void r_clearscreenbuffer(void);                                         // Clear framebuffer
void r_drawpoint(int x, int y, uint32_t color);                         // Draw single pixel
void r_drawline(int x0, int y0, int x1, int y1, uint32_t color);        // Draw line using Bresenham
void r_drawplayer(int x, int y, uint32_t color);                        // Draw player representation
void r_drawrectangle(int x, int y, int size, uint32_t color);           // Draw filled rectangle
void r_drawlevel(void);                                                 // Draw 2D map view
void r_raycast(void);                                                   // Main raycasting function
uint32_t* r_get_wall_texture(int wall_type);                            // Get correct texture for wall rendering
uint32_t* r_get_sprite(int sprite_type);                                // Get correct sprite image for rendering
void r_render_sprites(float *wall_distances, int column_width);         // Draw sprites
void r_draw_hud();                                                      // Draw HUD - only pistol and demo HUD with no function
void process_inputs(void);                                              // Handle user input
bool check_collision(float x, float y);                                 // Collision detection

// Utility math functions
float m_deg_to_rad(float a) { return a * PI / 180; }                    // Convert degrees to radians
float m_fix_ang(float a) {                                              // Normalize angle to 0-359 range
    if (a > 359) { a -= 360; }                                          // Wrap angles above 359
    if (a < 0) { a += 360; }                                            // Wrap negative angles
    return a;                                                           // Return normalized angle
}

// Main program entry point
int main() {
    // Initialize SDL video subsystem and check for errors
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {                                // Initialize SDL video subsystem
        printf("SDL_Init ERROR: Have you installed SDL library in your system?\n"); // Print error message
        return -1;                                                      // Exit with error code
    }                                         
    
    SDL_Window *window;                                                 // Window handle
    SDL_Renderer *renderer;                                             // Renderer handle
    
    // Create window centered on screen
    window = SDL_CreateWindow("Wolf_demo",                              // Window title
                             SDL_WINDOWPOS_CENTERED,                    // X position (centered)
                             SDL_WINDOWPOS_CENTERED,                    // Y position (centered)
                             SCREEN_WIDTH,                              // Window width
                             SCREEN_HEIGHT,                             // Window height
                             SDL_WINDOW_SHOWN);                         // Window flags
    
    // Create hardware-accelerated renderer
    renderer = SDL_CreateRenderer(window,                               // Window to attach to
                                -1,                                     // Use default graphics device
                                SDL_RENDERER_ACCELERATED);              // Use GPU acceleration for framebuffer drawing
    
    // Here starts game loop   
    rungame(renderer);                                                  // Run main game loop
        
    // Cleanup and shutdown
    SDL_DestroyRenderer(renderer);                                      // Destroy renderer
    SDL_DestroyWindow(window);                                          // Destroy window
    SDL_Quit();                                                         // Shutdown SDL
    return 0;                                                           // Exit program successfully
}

// Main game loop function
void rungame(SDL_Renderer *renderer) {
    // Initialization of framebuffer
    SDL_Texture *texture;                                               // Texture for framebuffer
    
    // Create streaming texture for framebuffer updates
    texture = SDL_CreateTexture(renderer,                               // Renderer to use
                              SDL_PIXELFORMAT_ARGB8888,                 // 32-bit ARGB format
                              SDL_TEXTUREACCESS_STREAMING,              // Allow frequent updates
                              SCREEN_WIDTH,                             // Texture width same as window width
                              SCREEN_HEIGHT);                           // Texture height same as window height
    
    // Allocate memory for framebuffer (4 bytes per pixel for ARGB)
    pixels = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    
    // Main game loop - runs until engine_on becomes false
    while (engine_on) {
        process_inputs();                                               // Handle keyboard input and update player
        r_clearscreenbuffer();                                          // Clear framebuffer to background color
        r_drawlevel();                                                  // Draw 2D map representation
        r_drawplayer(player.x, player.y, 0xffff0090);                   // Draw player as colored square
        r_raycast();                                                    // Perform raycasting draw map view and render 3D view
        r_draw_hud();                                                   // Lastly HUD is drawn over rendered scene
        
        // Update display
        SDL_UpdateTexture(texture,                                      // Texture to update
                         NULL,                                          // Update entire texture
                         pixels,                                        // Source pixel data
                         SCREEN_WIDTH * 4);                             // Bytes per row
        
        SDL_RenderCopy(renderer, texture, NULL, NULL);                  // Copy texture to renderer
        SDL_RenderPresent(renderer);                                    // Present rendered frame to screen
        SDL_Delay(1000 / 100);                                          // Limit to 100 FPS
    }
    
    SDL_DestroyTexture(texture);                                        // Cleanup texture after game loop quits
    free(pixels);                                                       // Free allocated framebuffer memory
}

// Draw a single pixel to the framebuffer
void r_drawpoint(int x, int y, uint32_t color) {
    // Bounds checking to prevent buffer overflow
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;                                                         // Exit if coordinates out of bounds
    }
    
    int index = SCREEN_WIDTH * y + x;                                   // Calculate linear array index
    
    // Additional safety check for buffer bounds
    if (index < 0 || index >= (SCREEN_WIDTH * SCREEN_HEIGHT)) {
        return;                                                         // Exit if index out of range
    }
    
    pixels[index] = color;                                              // Set pixel color in framebuffer
}

// Draw line using Bresenham's line algorithm
void r_drawline(int x0, int y0, int x1, int y1, uint32_t color) {
    // Calculate absolute differences for X and Y
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);                         // Absolute difference in X
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);                         // Absolute difference in Y
    
    int sx = x0 < x1 ? 1 : -1;                                          // X step direction (+1 or -1)
    int sy = y0 < y1 ? 1 : -1;                                          // Y step direction (+1 or -1)
    int err = (dx > dy ? dx : -dy) / 2;                                 // Initial error value
    int e2;                                                             // Error accumulator
    
    // Bresenham's algorithm main loop
    while (1) {
        r_drawpoint(x0, y0, color);                                     // Draw current point
        
        if (x0 == x1 && y0 == y1) break;                                // Stop when we reach end point
        
        e2 = err;                                                       // Store current error
        
        // Check if we need to step in X direction
        if (e2 > -dx) {
            err -= dy;                                                  // Update error for X step
            x0 += sx;                                                   // Step in X direction
        }
        
        // Check if we need to step in Y direction
        if (e2 < dy) {
            err += dx;                                                  // Update error for Y step
            y0 += sy;                                                   // Step in Y direction
        }
    }
}

// Clear framebuffer to background color
void r_clearscreenbuffer(void) {
    // Fill entire framebuffer with light gray color (0xFFBBBBBB)
    memset(pixels, 0xFFBBBBBB, 4 * SCREEN_WIDTH * SCREEN_HEIGHT);
}

// Draw player as a 9x9 pixel square
void r_drawplayer(int x, int y, uint32_t color) {
    // Double nested loop to draw 9x9 square
    for (int i = 0; i < 9; i++) {                                       // Loop through X offset
        for (int j = 0; j < 9; j++) {                                   // Loop through Y offset
            r_drawpoint(x + i, y + j, color);                           // Draw pixel at offset position
        }
    }
}

// Draw HUD - only pistol and crosshair and demo HUD at this moment. No animations
void r_draw_hud(){
    // Here we draw crosshair
    r_drawline(763,256,773,256,0xFF45FF17);                             // 10px horizontal line with neon green color
    r_drawline(768,251,768,261,0xFF45FF17);                             // 10px vertical line with neon green color

    // Here we draw pistol sprite
    int x = 0, y = 0, pistol_pixel = 0;                                 // Initialize loop variables and pixel counter
    // For loop to draw image line by line from the top
    for(y = 0; y<131; y++){                                             // Loop through pistol sprite height
        for(x=0; x<122; x++){                                           // Loop through pistol sprite width
            // Only draw non-transparent (pink) pixels to framebuffer at calculated position
            if(pistol[pistol_pixel] != 0xFFFF00FF) pixels[390876 + (y*1024) + x] = pistol[pistol_pixel]; 
            pistol_pixel++;                                             // Move to next pixel in sprite data
        }
    }

    // Here we draw demo hud
    int hud_pixel = 0;                                                  // Initialize HUD pixel counter
    for(y = 0; y<38; y++){                                              // Loop through HUD sprite height
        for(x=0; x<142; x++){                                           // Loop through HUD sprite width
            pixels[486258 + (y*1024) + x] = hud[hud_pixel];             // Draw HUD pixel to framebuffer at calculated position
            hud_pixel++;                                                // Move to next pixel in HUD sprite data
        }
    }
}

// Render all sprites in the scene with proper depth testing
void r_render_sprites(float *wall_distances, int column_width) {
    Sprite sprites[MAPX * MAPY];                                        // Array to hold all sprites in scene
    int sprite_count = 0;                                               // Counter for number of sprites found

    // Collect sprite positions from map
    for (int my = 0; my < MAPY; my++) {                                 // Loop through map Y coordinates
        for (int mx = 0; mx < MAPX; mx++) {                             // Loop through map X coordinates
            int spriteType = map_sprites[my * MAPX + mx];               // Get sprite type at this map position
            if (spriteType > 0) {                                       // If there's a sprite here
                float sx = mx * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5f;   // Calculate sprite world X position (center of cell)
                float sy = my * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5f;   // Calculate sprite world Y position (center of cell)
                float dx = sx - player.x;                               // Calculate X distance from player
                float dy = sy - player.y;                               // Calculate Y distance from player

                // Store sprite data in array with calculated distance
                sprites[sprite_count++] = (Sprite){sx, sy, sqrtf(dx*dx + dy*dy), spriteType};
            }
        }
    }

    // Sort sprites by distance (far → near) for proper rendering order
    for (int i = 0; i < sprite_count - 1; i++) {                        // Outer loop for bubble sort
        for (int j = i + 1; j < sprite_count; j++) {                    // Inner loop for bubble sort
            if (sprites[i].dist < sprites[j].dist) {                    // If current sprite is closer
                Sprite tmp = sprites[i];                                // Swap sprites to maintain far-to-near order
                sprites[i] = sprites[j];                              
                sprites[j] = tmp;                                     
            }
        }
    }

    // Define viewport and rendering constants
    const float fov = (float)FOV;                                       // Field of view as float
    const float rays = (float)RAY_COUNT;                                // Number of rays as float
    const float vp_left  = 512.0f;                                      // Left edge of 3D viewport
    const float vp_right = (float)SCREEN_WIDTH;                         // Right edge of 3D viewport
    const float eps = 0.0005f;                                          // Small value (epsilon) to prevent z-fighting

    // Render each sprite
    for (int i = 0; i < sprite_count; i++) {                            // Loop through all sprites
        float dx = sprites[i].x - player.x;                             // X distance from player to sprite
        float dy = sprites[i].y - player.y;                             // Y distance from player to sprite

        // Calculate sprite angle relative to player
        float sprite_angle = m_fix_ang(atan2f(-dy, dx) * 180.0f / PI);  // Convert to degrees and normalize
        float angle_diff = sprite_angle - player.angle;                 // Difference from player's facing direction
        if (angle_diff < -180) angle_diff += 360;                       // Normalize angle difference to -180 to +180
        if (angle_diff >  180) angle_diff -= 360;                    

        // Calculate perpendicular distance (corrected for fisheye effect)
        float perpDist = sprites[i].dist * cosf(m_deg_to_rad(angle_diff));

        // Safety checks to prevent rendering issues
        if (perpDist < 1.0f) continue;                                  // Skip if sprite too close
        int sprite_h = (MAP_CELL_SIZE * SCREEN_HEIGHT) / perpDist;      // Calculate sprite height on screen
        if (sprite_h > SCREEN_HEIGHT * 2) continue;                     // Skip if sprite would be absurdly large
        int sprite_w = sprite_h;                                        // Make sprite square (width = height)

        // Calculate vertical drawing bounds (bottom-aligned to floor)
        int drawEndY = SCREEN_HEIGHT / 2 + sprite_h / 2;                // Bottom edge of sprite
        int drawStartY = drawEndY - sprite_h;                           // Top edge of sprite

        // Vertical clipping and texture Y start calculation
        int texY_start = 0;                                             // Starting Y coordinate in texture
        if (drawStartY < 0) {                                           // If sprite extends above screen
            texY_start = (-drawStartY) * TEXTURE_SIZE / sprite_h;       // Calculate which part of texture to start with
            drawStartY = 0;                                             // Clip to top of screen
        }
        if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;    // Clip to bottom of screen

        // Calculate horizontal screen position
        float r_center_f = (angle_diff + (fov * 0.5f)) / (fov / rays);  // Convert angle to ray index (float)
        float screenX_center = vp_right - (r_center_f * (float)column_width) - (float)column_width * 0.5f; // Screen X position

        // Calculate horizontal drawing bounds
        int drawStartX = (int)floorf(screenX_center - sprite_w * 0.5f); // Left edge of sprite
        int drawEndX   = (int)ceilf (screenX_center + sprite_w * 0.5f); // Right edge of sprite

        // Horizontal clipping and texture X start calculation
        int texX_start = 0;                                             // Starting X coordinate in texture
        if (drawStartX < (int)vp_left) {                                // If sprite extends left of 3D viewport
            texX_start = (int)((vp_left - drawStartX) * (float)TEXTURE_SIZE / (float)sprite_w); // Calculate texture start
            drawStartX = (int)vp_left;                                  // Clip to viewport left edge
        }
        if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;      // Clip to screen right edge
        if (drawEndX < (int)vp_left || drawStartX >= SCREEN_WIDTH) continue; // Skip if completely outside viewport

        uint32_t *tex = r_get_sprite(sprites[i].type);                  // Get texture data for this sprite type

        // Render sprite columns
        for (int x = drawStartX; x <= drawEndX; x++) {                  // Loop through horizontal pixels
            // Calculate texture X coordinate for this screen column
            int texX = texX_start + (int)(((x - drawStartX) * (float)TEXTURE_SIZE) / (float)sprite_w);
            if (texX < 0) texX = 0;                                     // Clamp to texture bounds
            else if (texX >= TEXTURE_SIZE) texX = TEXTURE_SIZE - 1;   

            // Calculate interpolated wall depth at this screen position for depth testing
            float r_f = (vp_right - ((float)x + 0.5f)) / (float)column_width; // Convert screen X to ray index
            int r0 = (int)floorf(r_f);                                  // Lower ray index for interpolation
            float t = r_f - (float)r0;                                  // Interpolation factor
            int r1 = r0 + 1;                                            // Upper ray index for interpolation
            if (r0 < 0) { r0 = 0; t = 0.0f; }                           // Clamp to valid ray indices
            if (r1 >= RAY_COUNT) { r1 = RAY_COUNT - 1; t = 0.0f; }    
            float wall_d = (1.0f - t) * wall_distances[r0] + t * wall_distances[r1]; // Interpolated wall distance

            // Depth test - skip if sprite is behind wall
            if (perpDist > wall_d - eps) continue;                    

            // Draw vertical strip of sprite
            for (int y = drawStartY; y <= drawEndY; y++) {              // Loop through vertical pixels
                // Calculate texture Y coordinate for this screen row
                int texY = texY_start + (int)(((y - drawStartY) * (float)TEXTURE_SIZE) / (float)sprite_h);
                if (texY < 0) texY = 0;                                 // Clamp to texture bounds
                else if (texY >= TEXTURE_SIZE) texY = TEXTURE_SIZE - 1;

                uint32_t color = tex[texY * TEXTURE_SIZE + texX];       // Get pixel color from texture
                if (color == 0xFFFF00FF) continue;                      // Skip transparent pixels (magenta)

                // Apply distance-based darkening
                float dark = 1.0f - (sprites[i].dist / (MAP_CELL_SIZE * SPRITE_DISTANCE_DIMMING)); // Calculate darkening factor
                if (dark < SPRITE_MIN_BRIGHTNESS) dark = SPRITE_MIN_BRIGHTNESS; // Apply minimum brightness
                uint32_t r = ((color >> 16) & 0xFF) * dark;             // Apply darkening to red component
                uint32_t g = ((color >>  8) & 0xFF) * dark;             // Apply darkening to green component
                uint32_t b = (color & 0xFF) * dark;                     // Apply darkening to blue component

                r_drawpoint(x, y, 0xFF000000 | (r << 16) | (g << 8) | b); // Draw darkened pixel
            }
        }
    }
}

// Main raycasting function - renders 3D view
void r_raycast(void) {
    int r;                                                              // Ray counter variable
    float rangle = player.angle - FOV / 2.0f;                           // Starting ray angle (leftmost ray)
    float angle_step = (float)FOV / (float)RAY_COUNT;                   // Angle increment between rays
    int column_width = (SCREEN_WIDTH - 512) / RAY_COUNT;                // Width of each rendered column
    
    // Array to store wall distances for sprite depth testing
    float wall_distances[RAY_COUNT];                                    // Store distance for each ray
    
    // Cast rays from left to right across field of view
    for (r = 0; r < RAY_COUNT; r++) {                                   // Loop through each ray
        float rayAngleRad = m_deg_to_rad(rangle);                       // Convert ray angle to radians
        float rayDirX = cos(rayAngleRad);                               // X component of ray direction
        float rayDirY = -sin(rayAngleRad);                              // Y component of ray direction (negative for screen coordinates)
        
        // Check for horizontal grid line intersections
        float distanceH = 1000000;                                      // Initialize to large value (no hit)
        float hitX_H = 0, hitY_H = 0;                                   // Hit coordinates for horizontal intersections
        int wallType_H = 0;                                             // Wall type at horizontal intersection
        
        if (rayDirY != 0) {                                             // Only check if ray has Y component
            float deltaY, firstY;                                       // Y step and first Y intersection
            
            if (rayDirY < 0) {                                          // Ray pointing upward
                deltaY = -MAP_CELL_SIZE;                                // Step up by one cell
                firstY = floor(player.y / MAP_CELL_SIZE) * MAP_CELL_SIZE - 0.01f; // First intersection above player
            } else {                                                    // Ray pointing downward
                deltaY = MAP_CELL_SIZE;                                 // Step down by one cell
                firstY = floor(player.y / MAP_CELL_SIZE) * MAP_CELL_SIZE + MAP_CELL_SIZE; // First intersection below player
            }
            
            float deltaX = deltaY * (rayDirX / rayDirY);                // Corresponding X step
            float testY = firstY;                                       // Current test Y coordinate
            float testX = player.x + (testY - player.y) * (rayDirX / rayDirY); // Current test X coordinate
            
            // Step along ray checking for wall hits
            for (int depth = 0; depth < MAPY; depth++) {                // Limit search depth
                int mapX = (int)floor(testX / MAP_CELL_SIZE);           // Convert to map coordinates
                int mapY = (int)floor(testY / MAP_CELL_SIZE);          
                
                // Check map boundaries
                if (mapX < 0 || mapX >= MAPX || mapY < 0 || mapY >= MAPY) {
                    break;                                              // Hit map boundary, stop checking
                }
                
                if (map[mapY * MAPX + mapX] > 0) {                      // Check if cell contains wall
                    hitX_H = testX;                                     // Store hit coordinates
                    hitY_H = testY;                                   
                    wallType_H = map[mapY * MAPX + mapX];               // Store wall type
                    // Calculate distance from player to hit point
                    distanceH = sqrt((testX - player.x) * (testX - player.x) +
                                   (testY - player.y) * (testY - player.y));
                    break;                                              // Found wall, stop checking
                }
                
                testX += deltaX;                                        // Move to next intersection
                testY += deltaY;                                      
            }
        }
        
        // Check for vertical grid line intersections
        float distanceV = 1000000;                                      // Initialize to large value (no hit)
        float hitX_V = 0, hitY_V = 0;                                   // Hit coordinates for vertical intersections
        int wallType_V = 0;                                             // Wall type at vertical intersection
        
        if (rayDirX != 0) {                                             // Only check if ray has X component
            float deltaX, firstX;                                       // X step and first X intersection
            
            if (rayDirX < 0) {                                          // Ray pointing leftward
                deltaX = -MAP_CELL_SIZE;                                // Step left by one cell
                firstX = floor(player.x / MAP_CELL_SIZE) * MAP_CELL_SIZE - 0.01f; // First intersection left of player
            } else {                                                    // Ray pointing rightward
                deltaX = MAP_CELL_SIZE;                                 // Step right by one cell
                firstX = floor(player.x / MAP_CELL_SIZE) * MAP_CELL_SIZE + MAP_CELL_SIZE; // First intersection right of player
            }
            
            float deltaY = deltaX * (rayDirY / rayDirX);                // Corresponding Y step
            float testX = firstX;                                       // Current test X coordinate
            float testY = player.y + (testX - player.x) * (rayDirY / rayDirX); // Current test Y coordinate
            
            // Step along ray checking for wall hits
            for (int depth = 0; depth < MAPX; depth++) {                // Limit search depth
                int mapX = (int)floor(testX / MAP_CELL_SIZE);           // Convert to map coordinates
                int mapY = (int)floor(testY / MAP_CELL_SIZE);          
                
                // Check map boundaries
                if (mapX < 0 || mapX >= MAPX || mapY < 0 || mapY >= MAPY) {
                    break;                                              // Hit map boundary, stop checking
                }
                
                if (map[mapY * MAPX + mapX] > 0) {                      // Check if cell contains wall
                    hitX_V = testX;                                     // Store hit coordinates
                    hitY_V = testY;                                   
                    wallType_V = map[mapY * MAPX + mapX];               // Store wall type
                    // Calculate distance from player to hit point
                    distanceV = sqrt((testX - player.x) * (testX - player.x) +
                                   (testY - player.y) * (testY - player.y));
                    break;                                              // Found wall, stop checking
                }
                
                testX += deltaX;                                        // Move to next intersection
                testY += deltaY;                                      
            }
        }
        
        // Determine which intersection is closer
        float hitX, hitY, distance;                                     // Variables for final hit data
        bool hitVertical = false;                                       // Flag to track if we hit a vertical wall
        int currentWallType;                                            // Type of wall we hit

        if (distanceH < distanceV) {                                    // Horizontal intersection is closer
            hitX = hitX_H;                                              // Use horizontal hit coordinates
            hitY = hitY_H;                                            
            distance = distanceH;                                       // Use horizontal distance
            hitVertical = false;                                        // Mark as horizontal wall hit
            currentWallType = wallType_H;                               // Use horizontal wall type

            // Draw debug ray every 4th ray to reduce visual clutter
            if (r % 4 == 0) {
                r_drawline(player.x + 5, player.y + 5, hitX, hitY, 0xFF00BBBB); // Draw cyan debug ray
            }
        } else {                                                        // Vertical intersection is closer
            hitX = hitX_V;                                              // Use vertical hit coordinates
            hitY = hitY_V;                                            
            distance = distanceV;                                       // Use vertical distance
            hitVertical = true;                                         // Mark as vertical wall hit
            currentWallType = wallType_V;                               // Use vertical wall type
            
            // Draw debug ray every 4th ray to reduce visual clutter
            if (r % 4 == 0) {
                r_drawline(player.x + 5, player.y + 5, hitX, hitY, 0xFF00BBBB); // Draw cyan debug ray
            }
        }
        
        // Apply fisheye correction to prevent distortion
        float correctedDistance = distance * cos(m_deg_to_rad(rangle - player.angle));
        player.rays_d[r] = correctedDistance;                           // Store corrected distance in player data
        wall_distances[r] = correctedDistance;                          // Store for sprite depth testing
                
        // Calculate wall height based on corrected distance
        float wallHeight = (MAP_CELL_SIZE * SCREEN_HEIGHT) / correctedDistance;
        
        // Calculate wall rendering bounds and texture mapping
        int wallTop, wallBottom;                                        // Top and bottom pixel coordinates for wall
        float textureStep;                                              // Step size for texture sampling
        float textureStart = 0;                                         // Starting texture coordinate
        
        if (wallHeight > SCREEN_HEIGHT) {                               // Wall extends beyond screen height
            wallTop = 0;                                                // Start at top of screen
            wallBottom = SCREEN_HEIGHT;                                 // End at bottom of screen
            
            // Calculate texture offset for walls that extend beyond screen
            float textureOffset = (wallHeight - SCREEN_HEIGHT) / 2.0f;  // Amount of texture to skip
            textureStart = textureOffset * TEXTURE_SIZE / wallHeight;   // Convert to texture coordinates
            textureStep = (float)TEXTURE_SIZE / wallHeight;             // Texture step per pixel
        } else {                                                        // Wall fits within screen height
            wallTop = (SCREEN_HEIGHT - wallHeight) / 2;                 // Center wall vertically
            wallBottom = wallTop + wallHeight;                          // Calculate bottom position
            textureStart = 0;                                           // Start from top of texture
            textureStep = (float)TEXTURE_SIZE / wallHeight;             // Texture step per pixel
        }
        
        // Calculate texture X coordinate based on hit position
        float wallHitOffset;                                            // Offset within the wall cell
        if (hitVertical) {                                              // Hit vertical wall
            wallHitOffset = fmod(hitY, MAP_CELL_SIZE);                  // Use Y coordinate for texture X
        } else {                                                        // Hit horizontal wall
            wallHitOffset = fmod(hitX, MAP_CELL_SIZE);                  // Use X coordinate for texture X
        }
        
        // Convert wall offset to texture coordinate
        int textureX = (int)(wallHitOffset * TEXTURE_SIZE / MAP_CELL_SIZE);
        if (textureX >= TEXTURE_SIZE) textureX = TEXTURE_SIZE - 1;      // Clamp to texture bounds
        if (textureX < 0) textureX = 0;                               
        
        // Render floor texture below wall
        for (int y = wallBottom; y < SCREEN_HEIGHT; y++) {              // Loop from wall bottom to screen bottom
            // Calculate distance to floor point using screen geometry
            float floorDistance = (MAP_CELL_SIZE * SCREEN_HEIGHT / 2.0f) / (y - SCREEN_HEIGHT / 2.0f);
            floorDistance = floorDistance / cos(m_deg_to_rad(rangle - player.angle)); // Apply fisheye correction
            
            // Calculate world coordinates of floor point
            float floorX = player.x + rayDirX * floorDistance;          // World X coordinate
            float floorY = player.y + rayDirY * floorDistance;          // World Y coordinate
            
            // Convert to texture coordinates
            int floorTexX = (int)(fmod(floorX, MAP_CELL_SIZE) * TEXTURE_SIZE / MAP_CELL_SIZE) & (TEXTURE_SIZE - 1);
            int floorTexY = (int)(fmod(floorY, MAP_CELL_SIZE) * TEXTURE_SIZE / MAP_CELL_SIZE) & (TEXTURE_SIZE - 1);
            
            uint32_t floorColor = ground[floorTexY * TEXTURE_SIZE + floorTexX]; // Get floor texture color
            
            // Apply distance-based darkening to floor
            float darkening = 1.0f - (floorDistance / (MAP_CELL_SIZE * FLOOR_DISTANCE_DIMMING));
            if (darkening < FLOOR_MIN_BRIGHTNESS) darkening = FLOOR_MIN_BRIGHTNESS; // Apply minimum brightness
            
            // Apply darkening to color components
            uint32_t r_comp = ((floorColor >> 16) & 0xFF) * darkening;  // Red component
            uint32_t g = ((floorColor >> 8) & 0xFF) * darkening;        // Green component
            uint32_t b = (floorColor & 0xFF) * darkening;               // Blue component
            floorColor = 0xFF000000 | (r_comp << 16) | (g << 8) | b;    // Recombine color
            
            // Draw floor pixels across column width
            for (int i = 0; i < column_width; i++) {
                r_drawpoint(SCREEN_WIDTH - r * column_width - i, y, floorColor);
            }
        }
        
        // Render ceiling texture above wall
        for (int y = 0; y < wallTop; y++) {                             // Loop from screen top to wall top
            // Use mirrored Y coordinate for ceiling calculation
            int mirrorY = SCREEN_HEIGHT - 1 - y;                        // Mirror Y coordinate for ceiling
            // Calculate distance to ceiling point using screen geometry
            float ceilDistance = (MAP_CELL_SIZE * SCREEN_HEIGHT / 2.0f) / (mirrorY - SCREEN_HEIGHT / 2.0f);
            ceilDistance = ceilDistance / cos(m_deg_to_rad(rangle - player.angle)); // Apply fisheye correction
            
            // Calculate world coordinates of ceiling point
            float ceilX = player.x + rayDirX * ceilDistance;            // World X coordinate
            float ceilY = player.y + rayDirY * ceilDistance;            // World Y coordinate
            
            // Convert to texture coordinates
            int ceilTexX = (int)(fmod(ceilX, MAP_CELL_SIZE) * TEXTURE_SIZE / MAP_CELL_SIZE) & (TEXTURE_SIZE - 1);
            int ceilTexY = (int)(fmod(ceilY, MAP_CELL_SIZE) * TEXTURE_SIZE / MAP_CELL_SIZE) & (TEXTURE_SIZE - 1);
            
            uint32_t ceilColor = ceiling[ceilTexY * TEXTURE_SIZE + ceilTexX]; // Get ceiling texture color
            
            // Apply distance-based darkening to ceiling (darker than floor)
            float darkening = (1.0f - (ceilDistance / (MAP_CELL_SIZE * CEILING_DISTANCE_DIMMING))) * 0.85f;
            if (darkening < CEILING_MIN_BRIGHTNESS) darkening = CEILING_MIN_BRIGHTNESS; // Apply minimum brightness
            
            // Apply darkening to color components
            uint32_t r_comp = ((ceilColor >> 16) & 0xFF) * darkening;   // Red component
            uint32_t g = ((ceilColor >> 8) & 0xFF) * darkening;         // Green component
            uint32_t b = (ceilColor & 0xFF) * darkening;                // Blue component
            ceilColor = 0xFF000000 | (r_comp << 16) | (g << 8) | b;     // Recombine color
            
            // Draw ceiling pixels across column width
            for (int i = 0; i < column_width; i++) {
                r_drawpoint(SCREEN_WIDTH - r * column_width - i, y, ceilColor);
            }
        }
        
        // Render textured wall slice
        uint32_t* wallTexture = r_get_wall_texture(currentWallType);    // Get appropriate wall texture

        // Draw wall pixels from top to bottom
        for (int y = wallTop; y < wallBottom; y++) {                    // Loop through wall height
            // Calculate texture Y coordinate for this pixel
            float textureYFloat = textureStart + (y - wallTop) * textureStep;
            int textureY = (int)textureYFloat;                          // Convert to integer
            
            // Clamp texture Y to valid range
            if (textureY >= TEXTURE_SIZE) textureY = TEXTURE_SIZE - 1;
            if (textureY < 0) textureY = 0;                           
            
            // Get texture color at calculated coordinates
            uint32_t textureColor = wallTexture[textureY * TEXTURE_SIZE + textureX];
            
            // Apply distance-based darkening to wall
            float wallDarkening = 1.0f - (correctedDistance / (MAP_CELL_SIZE * WALL_DISTANCE_DIMMING));
            if (wallDarkening < WALL_MIN_BRIGHTNESS) wallDarkening = WALL_MIN_BRIGHTNESS; // Apply minimum brightness
            
            // Make vertical walls slightly darker for depth perception
            if (hitVertical) {
                wallDarkening *= 0.8f;                                  // Darken vertical walls
            }
            
            // Apply darkening to color components
            uint32_t r_comp = ((textureColor >> 16) & 0xFF) * wallDarkening; // Red component
            uint32_t g = ((textureColor >> 8) & 0xFF) * wallDarkening;  // Green component
            uint32_t b = (textureColor & 0xFF) * wallDarkening;         // Blue component
            textureColor = 0xFF000000 | (r_comp << 16) | (g << 8) | b;  // Recombine color
            
            // Draw wall pixels across column width
            for (int i = 0; i < column_width; i++) {
                r_drawpoint(SCREEN_WIDTH - r * column_width - i, y, textureColor);
            }
        }
        
        rangle = rangle + angle_step;                                   // Move to next ray angle
    }
    r_render_sprites(wall_distances, column_width);                     // Render sprites after walls are drawn
}

// Get the appropriate texture based on wall type
uint32_t* r_get_wall_texture(int wall_type) {
    switch(wall_type) {                                                 // Switch on wall type value
        case 1: return greystone;                                       // Stone wall texture
        case 2: return mossy;                                           // Mossy stone wall texture
        case 3: return colorstone;                                      // Colored stone wall texture
        default: return greystone;                                      // Default to stone if unknown type
    }
}

// Get the appropriate sprite based on sprite type
uint32_t* r_get_sprite(int sprite_type){
    switch(sprite_type) {                                               // Switch on sprite type value
        case 1: return hangman;                                         // Hangman sprite
        case 2: return barrel;                                          // Barrel sprite
        case 3: return armor_suit;                                      // Armor suit sprite
        case 4: return bed;                                             // Bed sprite
        case 5: return plant;                                           // Plant sprite
        case 6: return sink;                                            // Sink sprite
        case 7: return dead_plant;                                      // Dead plant sprite
        case 8: return light;                                           // Ceiling light sprite
        default: return light;                                          // Default to ceiling light if unknown type
    }
}

// Draw filled rectangle
void r_drawrectangle(int x, int y, int size, uint32_t color) {
    // Draw horizontal lines to fill rectangle
    for (int i = 0; i <= size; i++) {                                   // Loop through rectangle height
        r_drawline(x, y + i, x + size, y + i, color);                   // Draw each horizontal line
    }
}

// Draw 2D map representation
void r_drawlevel(void) {
    // Draw filled rectangles for wall cells
    for (int i = 0; i < MAPX; i++) {                                    // Loop through map X coordinates
        for (int j = 0; j < MAPY; j++) {                                // Loop through map Y coordinates
            if (map[i + j * MAPX] > 0) {                                // Check if cell contains wall
                // Draw gray rectangle for wall
                r_drawrectangle(i * MAP_CELL_SIZE, j * MAP_CELL_SIZE, MAP_CELL_SIZE, 0xff888888);
            }
        }
    }
    
    // Draw horizontal grid lines
    for (int i = 0; i <= MAPY * MAP_CELL_SIZE; i += MAP_CELL_SIZE) {    // Loop through horizontal grid positions
        r_drawline(0, i, MAPX * MAP_CELL_SIZE, i, 0xFF000000);          // Draw black horizontal line
    }
    
    // Draw vertical grid lines
    for (int i = 0; i <= MAPX * MAP_CELL_SIZE; i += MAP_CELL_SIZE) {    // Loop through vertical grid positions
        r_drawline(i, 0, i, MAPY * MAP_CELL_SIZE, 0xFF000000);          // Draw black vertical line
    }
}

// Process all user inputs
void process_inputs(void) {
    SDL_Event event;                                                    // Event structure for discrete events
    
    // Handle discrete events (key presses, window close)
    while (SDL_PollEvent(&event)) {                                     // Poll for events in queue
        switch (event.type) {                                           // Check event type
            case SDL_QUIT:                                              // User clicks window X button
                engine_on = false;                                      // Set flag to exit main loop
                break;                                                  // Exit switch statement
                
            case SDL_KEYDOWN:                                           // Key pressed down
                if (event.key.keysym.sym == SDLK_ESCAPE) {              // Check if ESC key
                    engine_on = false;                                  // Set flag to exit main loop
                    break;                                              // Exit switch statement
                }
        }
    }
    
    // Handle continuous keyboard input
    const Uint8 *keystate = SDL_GetKeyboardState(NULL);                 // Get current keyboard state array
    
    // Handle rotation input
    if (keystate[SDL_SCANCODE_LEFT] || keystate[SDL_SCANCODE_A]) {      // Left arrow or A key pressed
        player.angle += 1.8;                                            // Rotate counterclockwise
        player.angle = m_fix_ang(player.angle);                         // Normalize angle to 0-359 range
        player.dx = cos(m_deg_to_rad(player.angle));                    // Update direction X component
        player.dy = -sin(m_deg_to_rad(player.angle));                   // Update direction Y component (negative for screen coords)
    }
    
    if (keystate[SDL_SCANCODE_RIGHT] || keystate[SDL_SCANCODE_D]) {     // Right arrow or D key pressed
        player.angle -= 1.8;                                            // Rotate clockwise
        player.angle = m_fix_ang(player.angle);                         // Normalize angle to 0-359 range
        player.dx = cos(m_deg_to_rad(player.angle));                    // Update direction X component
        player.dy = -sin(m_deg_to_rad(player.angle));                   // Update direction Y component (negative for screen coords)
    }
    
    // Handle forward/backward movement with collision detection
    if (keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_W]) {        // Up arrow or W key pressed
        float new_x = player.x + player.dx * 2.5;                       // Calculate new X position (forward)
        float new_y = player.y + player.dy * 2.5;                       // Calculate new Y position (forward)
        
        // Check collision before moving (separate X and Y for wall sliding)
        if (!check_collision(new_x, player.y)) {                        // Check X movement collision
            player.x = new_x;                                           // Move in X direction if no collision
        }
        if (!check_collision(player.x, new_y)) {                        // Check Y movement collision
            player.y = new_y;                                           // Move in Y direction if no collision
        }
    }
    
    if (keystate[SDL_SCANCODE_DOWN] || keystate[SDL_SCANCODE_S]) {      // Down arrow or S key pressed
        float new_x = player.x - player.dx * 2.5;                       // Calculate new X position (backward)
        float new_y = player.y - player.dy * 2.5;                       // Calculate new Y position (backward)
        
        // Check collision before moving (separate X and Y for wall sliding)
        if (!check_collision(new_x, player.y)) {                        // Check X movement collision
            player.x = new_x;                                           // Move in X direction if no collision
        }
        if (!check_collision(player.x, new_y)) {                        // Check Y movement collision
            player.y = new_y;                                           // Move in Y direction if no collision
        }
    }
}

// Collision detection function - checks if position contains a wall
bool check_collision(float x, float y) {
    // Convert world coordinates to map grid coordinates
    int mapX = (int)floor(x / MAP_CELL_SIZE);                           // Get map X coordinate
    int mapY = (int)floor(y / MAP_CELL_SIZE);                           // Get map Y coordinate
    
    // Check if coordinates are outside map boundaries
    if (mapX < 0 || mapX >= MAPX || mapY < 0 || mapY >= MAPY) {
        return true;                                                    // Collision with map boundary
    }
    
    // Check if the map cell contains a wall (non-zero value)
    if (map[mapY * MAPX + mapX] > 0) {
        return true;                                                    // Collision with wall
    }
    // Check if the map cell contains a non walkable sprite (value 1-7)
    if (map_sprites[mapY * MAPX + mapX] > 0 && map_sprites[mapY * MAPX + mapX] < 8){
        return true;
    }
    
    return false;                                                       // No collision detected
}