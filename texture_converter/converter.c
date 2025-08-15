/***********************************************************************************************************************
 *                             Simple program to convert GIMP .ppm ASCII files to ARGB uint32 arrays                   *
 *                             - This program has no special library dependencies                                      *
 *                             - Feel free to use as you like                                                          *
 ***********************************************************************************************************************/

#include <stdio.h>      // Standard I/O functions (printf, fopen, etc.)
#include <stdlib.h>     // Standard library functions (malloc, exit, etc.)
#include <string.h>     // String manipulation functions (strcpy, strlen, etc.)
#include <ctype.h>      // Character classification functions (isalnum, isdigit, etc.)
#include <libgen.h>     // Path manipulation functions (basename)
#include <sys/stat.h>   // File status functions (stat)

#ifdef _WIN32
    #include <io.h>
    #define ftruncate _chsize
#else
    #include <unistd.h>
#endif

// Function to display usage instructions to the user
void usage(const char* prog_name) {
    printf("This is simple converter from gimp exported .ppm files to ARGB uint32 array header file\n\n");
    printf("Usage: %s <input_rgb_file> <output_header_file> [array_name]\n", prog_name);
    printf("  input_rgb_file:       ASCII file with RGB values (one value per line)\n");
    printf("  output_header_file:   output .h file to generate\n");
    printf("  array_name:           Optional custom array name (default: derived from input filename)\n");
    printf("\n");
    printf("Note: If output file exists, new array will be appended to it.\n\n");
    printf("Example: %s texture.ppm texture.h my_texture_data\n", prog_name);
}

// Check if file exists
int file_exists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Remove the last #endif from the file using a portable approach
long remove_last_endif(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return -1;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    
    // Read entire file into memory
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return -1;
    }
    
    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';
    fclose(file);
    
    // Find the last occurrence of "#endif"
    char* last_endif = NULL;
    char* current_pos = content;
    
    while ((current_pos = strstr(current_pos, "#endif")) != NULL) {
        last_endif = current_pos;
        current_pos++;
    }
    
    if (last_endif) {
        // Find the start of the line containing #endif
        char* line_start = last_endif;
        while (line_start > content && *(line_start - 1) != '\n') {
            line_start--;
        }
        
        long endif_pos = line_start - content;
        
        // Rewrite the file without the #endif line and everything after it
        file = fopen(filename, "w");
        if (file) {
            fwrite(content, 1, endif_pos, file);
            fclose(file);
            free(content);
            return endif_pos;
        }
    }
    
    free(content);
    return -1;
}

// Generate a valid C array name from the input filename
void generate_array_name(const char* filename, char* array_name) {
    char temp[256];                                                        // Temporary buffer for filename manipulation
    strcpy(temp, filename);                                                // Copy filename to temporary buffer
    
    // Extract basename (filename without path) and remove file extension
    char* base = basename(temp);                                           // Get just the filename part (no path)
    char* dot = strrchr(base, '.');                                        // Find the last dot in filename
    if (dot) *dot = '\0';                                                  // Remove extension by null-terminating at the dot
    
    // Convert filename to valid C identifier by replacing non-alphanumeric chars
    int i, j = 0;
    for (i = 0; base[i] && j < 255; i++) {                                 // Loop through each character
        if (isalnum(base[i])) {                                            // If character is alphanumeric
            array_name[j++] = base[i];                                     // Keep it as-is
        } else {
            array_name[j++] = '_';                                         // Replace with underscore
        }
    }
    array_name[j] = '\0';                                                  // Null-terminate the string
    
    // Ensure array name doesn't start with a digit (invalid in C)
    if (isdigit(array_name[0])) {
        memmove(array_name + 5, array_name, strlen(array_name) + 1);       // Shift string right
        memcpy(array_name, "data_", 5);                                    // Prepend "data_" prefix
    }
}

// Generate header guard macro name from array name
void generate_header_guard(const char* array_name, char* guard_name) {
    // Remove filetype from string (like .h)
    char *ptr;
    ptr = strchr(array_name, '.');
    if (ptr != NULL) {
        *ptr = '\0';
    }
    sprintf(guard_name, "%s_H", array_name);                               // Add "_H" suffix
    for (int i = 0; guard_name[i]; i++) {                                  // Convert all characters to uppercase
        guard_name[i] = toupper(guard_name[i]);
    }

    // Give the dot back
    if (ptr != NULL){
        *ptr = '.';
    }
}

// Check if file starts with P3 format and skip header if so
int skip_p3_header_if_present(FILE* file) {
    char first_line[32];
    
    // Read the first line to check for P3 format
    if (fgets(first_line, sizeof(first_line), file) == NULL) {
        rewind(file);                                                      // If can't read, go back to start
        return 0;                                                          // Not P3 format
    }
    
    // Check if first line starts with "P3"
    char* trimmed = first_line;
    while (isspace(*trimmed)) trimmed++;                                   // Skip leading whitespace
    
    if (strncmp(trimmed, "P3", 2) == 0) {
        // This is P3 format, skip the next 3 lines (total 4 lines including P3)
        char line[256];
        for (int i = 0; i < 3; i++) {                                      // Skip 3 more lines
            if (fgets(line, sizeof(line), file) == NULL) {
                break;                                                     // If can't read more lines, stop
            }
        }
        return 1;                                                          // P3 header was skipped
    } else {
        // Not P3 format, rewind to beginning
        rewind(file);
        return 0;                                                          // No P3 header skipped
    }
}

// Count the total number of RGB values in the file (after skipping P3 header if present)
int count_rgb_values(FILE* file) {
    int count = 0;                                                         // Counter for valid RGB values
    int value;                                                             // Temporary storage for parsed integer
    char line[32];                                                         // Buffer for reading each line
    
    // Skip P3 header if present
    int skipped_p3 = skip_p3_header_if_present(file);
    
    // Read through file line by line
    while (fgets(line, sizeof(line), file)) {
        // Skip empty lines and whitespace-only lines
        char* trimmed = line;                                              // Pointer to start of non-whitespace content
        while (isspace(*trimmed)) trimmed++;                               // Skip leading whitespace
        if (*trimmed == '\0') continue;                                    // Skip if line is empty after trimming
        
        // Check if line contains a valid integer
        if (sscanf(trimmed, "%d", &value) == 1) {
            count++;                                                       // Increment counter for each valid number
        }
    }
    
    rewind(file);                                                          // Reset file pointer to beginning for next read
    return count;                                                          // Return total count of RGB values
}

// Main function - entry point of the program
int main(int argc, char* argv[]) {
    // Check command line arguments - need at least input and output filenames
    if (argc < 3 || argc > 4) {
        usage(argv[0]);                                                    // Display usage if insufficient arguments
        return 1;                                                          // Exit with error code
    }
    
    // Extract command line arguments
    const char* input_file = argv[1];                                      // Input RGB file path
    const char* output_file = argv[2];                                     // Output header file path
    const char* custom_array_name = (argc > 3) ? argv[3] : NULL;           // Optional array name
    
    // Check if output file exists
    int append_mode = file_exists(output_file);
    
    // Open input file for reading
    FILE* infile = fopen(input_file, "r");
    if (!infile) {                                                         // Check if file opening failed
        fprintf(stderr, "Error: Input file '%s' not found\n", input_file);
        return 1;                                                          // Exit with error code
    }
    
    // Count total RGB values in the file
    int total_values = count_rgb_values(infile);
    int num_pixels = total_values / 3;                                     // Each pixel has 3 values (R, G, B)
    
    // Check if we have complete RGB triplets
    if (total_values % 3 != 0) {
        printf("Warning: File doesn't contain complete RGB triplets. Exiting...\n");
        fclose(infile);
        return 1;
    }
    
    // Verify we have at least one complete pixel
    if (num_pixels == 0) {
        fprintf(stderr, "Error: No valid pixels found in input file\n");
        fclose(infile);                                                    // Clean up file handle
        return 1;                                                          // Exit with error code
    }
    
    printf("Processing %d pixels from '%s'...\n", num_pixels, input_file);
    
    // Generate array name for the C header file
    char array_name[256];
    if (custom_array_name) {                                               // Use custom name if provided
        strcpy(array_name, custom_array_name);
    } else {                                                               // Generate from input filename
        generate_array_name(input_file, array_name);
    }
    
    // Generate header guard macro name
    char header_guard[512];
    generate_header_guard(output_file, header_guard);
    
    FILE* outfile;
    
    if (append_mode) {
        printf("Output file exists. Appending new array to '%s'...\n", output_file);
        
        // Remove the last #endif from the existing file
        long endif_pos = remove_last_endif(output_file);
        if (endif_pos == -1) {
            fprintf(stderr, "Warning: Could not find #endif in existing file. Proceeding with append.\n");
        }
        
        // Open file in append mode
        outfile = fopen(output_file, "a");
        if (!outfile) {
            fprintf(stderr, "Error: Cannot open output file '%s' for appending\n", output_file);
            fclose(infile);
            return 1;
        }
        
        // Add a newline for separation
        fprintf(outfile, "\n");
        
    } else {
        printf("Creating new header file '%s'...\n", output_file);
        
        // Open output file for writing (new file)
        outfile = fopen(output_file, "w");
        if (!outfile) {                                                    // Check if file creation failed
            fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
            fclose(infile);                                                // Clean up input file handle
            return 1;                                                      // Exit with error code
        }
        
        // Write C header file header section (only for new files)
        fprintf(outfile, "#ifndef %s\n", header_guard);                    // Header guard start
        fprintf(outfile, "#define %s\n\n", header_guard);                  // Header guard define
        fprintf(outfile, "#include <stdint.h>\n\n");                       // Include for uint32_t type
    }

    // Write array definition
    fprintf(outfile, "// Contains %d pixels in ARGB format (0xAARRGGBB)\n\n", num_pixels);
    fprintf(outfile, "static uint32_t %s[%d] = {\n", array_name, num_pixels);  // Array declaration
    
    // Skip P3 header if present before processing data
    skip_p3_header_if_present(infile);
    
    // Process RGB values and convert to ARGB format
    int pixel_count = 0;                                                   // Counter for completed pixels
    int value_count = 0;                                                   // Counter for individual RGB values
    int r, g, b;                                                           // Storage for red, green, blue components
    char line[32];                                                         // Buffer for reading each line
    
    // Read and process each line of RGB data
    while (fgets(line, sizeof(line), infile) && pixel_count < num_pixels) {
        // Skip empty lines and whitespace-only lines
        char* trimmed = line;
        while (isspace(*trimmed)) trimmed++;
        if (*trimmed == '\0') continue;
        
        int value;                                                         // Current RGB component value
        if (sscanf(trimmed, "%d", &value) != 1) {                          // Parse integer from line
            printf("Warning: Invalid RGB value '%s', skipping\n", trimmed);
            continue;                                                      // Skip invalid lines
        }
        
        // Validate RGB value range (0-255)
        if (value > 255 || value < 0) {
            printf("Warning: RGB value '%d' out of range (0-255)\n", value);
        }
        
        // Clamp value to valid range
        if (value > 255) value = 255;
        if (value < 0) value = 0;
        
        // Assign value to appropriate RGB component based on position
        switch (value_count % 3) {
            case 0: r = value; break;                                      // First value is red
            case 1: g = value; break;                                      // Second value is green
            case 2:                                                        // Third value is blue - complete pixel
                b = value;
                // Calculate ARGB value: Alpha=0xFF (opaque), then R, G, B
                uint32_t argb_val = 0xFF000000 | (r << 16) | (g << 8) | b;
                
                // Write pixel to output file with proper formatting
                if (pixel_count == num_pixels - 1) {
                    // Last element - no trailing comma
                    fprintf(outfile, "    0x%08X  // px%d: RGB(%d,%d,%d)\n", 
                            argb_val, pixel_count, r, g, b);
                } else {
                    // Regular element - with trailing comma
                    fprintf(outfile, "    0x%08X, // px%d: RGB(%d,%d,%d)\n", 
                            argb_val, pixel_count, r, g, b);
                }
                
                pixel_count++;                                            // Increment completed pixel counter
                break;
        }
        
        value_count++;                                                    // Increment total value counter
    }
    
    // Write C header file footer section
    fprintf(outfile, "};\n\n");                                           // Close array definition
    fprintf(outfile, "#define %s_SIZE %d\n\n", array_name, num_pixels);   // Define array size macro
    
    // Always write the #endif at the end (whether new file or appending)
    fprintf(outfile, "#endif // %s\n", header_guard);                     // Header guard end
    
    // Clean up file handles
    fclose(infile);                                                       // Close input file
    fclose(outfile);                                                      // Close output file
    
    // Display success information
    if (append_mode) {
        printf("Successfully appended to '%s'\n", output_file);
    } else {
        printf("Successfully generated '%s'\n", output_file);
    }
    printf("Array name: %s\n", array_name);
    printf("Array size: %d pixels\n", num_pixels);
    printf("Usage in your c project: #include \"%s\"\n", output_file);
    
    return 0;                                                             // Exit successfully
}
