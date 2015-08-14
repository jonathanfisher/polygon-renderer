#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <malloc.h>
#include <png.h>

#define INPUT_IMAGE "starry-night-200x200.png"

#define MAX_COLOR_VALUE 255

#define DEFAULT_POLYGON_POINTS 4

#define MAX_POLYGON_POINTS 10

#define N_POLYGONS 200

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/*
 * These macros are used to help the compiler do some branch prediction.
 */
#define likely(x)   __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

typedef struct {
    int red;
    int green;
    int blue;
} Color_t;

typedef struct {
    int x;
    int y;
} Point_t;

typedef struct {
    Point_t Points[MAX_POLYGON_POINTS];
} Polygon_t;

/**
 * This function is called when the program needs to end after an unrecoverable
 * error.  Specifically, this is called when there is a bad argument, or there
 * is something wrong with reading/writing a file.
 */
static void abort_(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

/**
 * Print a given color to stdout in a human-readable way.
 */
static void printColor(Color_t c)
{
    printf("RGB: (%d, %d, %d)\n", c.red, c.green, c.blue);
}

/**
 * Print a specific point to stdout in a human-readable way.
 */
static void printPoint(Point_t p)
{
    printf("(%d, %d)\n", p.x, p.y);
}

/**
 * Print a polygon to stdout in a human-readable way.  This is done by
 * iterating through the points that make up the polygon and printing them.
 */
static void printPolygon(Polygon_t p, int n_points)
{
    int i;

    for (i = 0; i < n_points; i++)
        printPoint(p.Points[i]);
}

/**
 * Convert a given X,Y coordinate to a linear index.  This is used because the
 * arrays used are actually vectors, but it is easier for me to think of X,Y
 * coordinates.
 */
static int coord_to_ind(int x, int y, int width)
{
    return (y * width) + x;
}

/**
 * Generate a random double in a given range.
 */
static double drandrange(double low, double high)
{
    return ((double)rand() / (double)((double)RAND_MAX + 1.0))
        * (high - low) + low;
}

/**
 * Generate a random integer in a given range.  Note that this is intended to be
 * inclusive (low & high are valid values).
 */
static int randrange(int low, int high)
{
    double r;

    if (low > high)
        abort_("Lower boundary must be <= high boundary.\n");

    r = ((double)rand() / (double)((double)RAND_MAX + 1.0))
        * (high - low + 1) + low;
    return (int)r;
}

/**
 * Generate a random RGB color.  This is done by setting each channel of the
 * color to a random value between 0 and the maximum color value (255).
 */
static Color_t getRandomColor(void)
{
    Color_t color;

    color.red   = randrange(0, MAX_COLOR_VALUE);
    color.green = randrange(0, MAX_COLOR_VALUE);
    color.blue  = randrange(0, MAX_COLOR_VALUE);

    return color;
}

/**
 * Get a random point in a given range.  This is done by generating one X value
 * and one Y value according to the size given.
 */
static Point_t getRandomPoint(int width, int height)
{
    Point_t pt;

    pt.x = randrange(0, width);
    pt.y = randrange(0, height);

    return pt;
}

/**
 * Generate a random polygon given maximum size and the number of points it
 * should be.
 */
static Polygon_t getRandomPolygon(int width, int height, int n_points)
{
    Polygon_t pg = {0};
    int i;

    for (i = 0; i < n_points; i++)
        pg.Points[i] = getRandomPoint(width, height);

    return pg;
}

/**
 * Calculate the weighted average of two integers.  The weight argument is
 * applied to the second argument.
 *
 * Example:
 * weightedAverage(2, 10, 0.25)
 *   = (1 - 0.25) x (2) + (0.25 x 10)
 *   = (0.75 x 2) + (0.25 x 10)
 *   = 1.5 + 2.5
 *   = 4
 */
static unsigned char weightedAverage(int a,
        int b, float weight)
{
    int result;

    if (unlikely(weight > 1.0f || weight < 0.0f))
        abort_("Weight must be between 0 and 1 (%f)\n", weight);

    if (unlikely(a == -1 && b == -1))
        abort_("%s: At least one of the arguments must be >= 0.\n", __func__);

    if (unlikely(a == -1))
        return b;

    if (unlikely(b == -1))
        return a;

    result = (unsigned char)( ((1.0-weight) * (float)a) +
            (weight * (float)b) );

    return result;
}

/**
 * Average two colors together according to the given weight.  This simply
 * averages each channel of the RGB color according to the given weight.
 */
static Color_t averageColors(Color_t a, Color_t b, float weight)
{
    Color_t avg;

    avg.red   = weightedAverage(a.red, b.red, weight);
    avg.green = weightedAverage(a.green, b.green, weight);
    avg.blue  = weightedAverage(a.blue, b.blue, weight);

    return avg;
}

/**
 * Set the color of a given X,Y pixel.
 */
static void setPixel(Color_t *canvas, int width, int height, int x, int y,
        int r, int g, int b)
{
    if (likely(x >= 0 && x < width && y >= 0 && y < height)) {
        int index = coord_to_ind(x, y, width);
        canvas[index].red   = r;
        canvas[index].green = g;
        canvas[index].blue  = b;
    }
}

/**
 * Essentially merges in a given color to an existing pixel according to the
 * given weight.
 */
static void setWeightedPixel(Color_t *canvas, int width, int height,
        int x, int y, Color_t color, float weight)
{
    Color_t avg = averageColors(canvas[coord_to_ind(x, y, width)],
            color, weight);

    setPixel(canvas, width, height, x, y, avg.red, avg.green, avg.blue);
}

/**
 * Sets all of the pixels in a given canvas to a specific value.
 */
static void clearCanvas(Color_t *canvas, int width, int height)
{
    int i, j;

    for (j = 0; j < height; j++)
        for (i = 0; i < width; i++)
            setPixel(canvas, width, height, i, j, 0, 0, 0);
}

/**
 * Merge the given polygon into the given canvas using the given color and
 * weight.
 */
static void drawPolygon(Color_t *canvas, int width, int height,
        Polygon_t pg, int n_points, Color_t color, float weight)
{
    int nodes, nodeX[MAX_POLYGON_POINTS], pixelX, pixelY, i, j, swap;

    for (pixelY = 0; pixelY < height; pixelY++) {
        // Build a list of nodes.
        nodes = 0;
        j = n_points - 1;
        for (i = 0; i < n_points; i++) {
            if (pg.Points[i].y < pixelY && pg.Points[j].y >= pixelY
                    || pg.Points[j].y < pixelY && pg.Points[i].y >= pixelY) {
                nodeX[nodes++] =
                    (int)((double)pg.Points[i].x + (double)(pixelY - pg.Points[i].y) / ((double)pg.Points[j].y - pg.Points[i].y) * (double)(pg.Points[j].x - pg.Points[i].x));
            }
            j = i;
        }

        // Sort the nodes, via a simple "Bubble" sort.
        i = 0;
        while (i < nodes - 1) {
            if (nodeX[i] > nodeX[i+1]) {
                swap       = nodeX[i];
                nodeX[i]   = nodeX[i+1];
                nodeX[i+1] = swap;
                if (i) i--;
            } else {
                i++;
            }
        }

        // Fill the pixels between node pairs.
        for (i = 0; i < nodes; i += 2) {
            if (nodeX[i] >= width) break;
            if (nodeX[i + 1] > 0) {
                if (nodeX[i] < 0) nodeX[i] = 0;
                if (nodeX[i+1] >= width) nodeX[i+1] = width-1;
                for (j = nodeX[i]; j < nodeX[i + 1]; j++) {
                    /* Average the pixels. */
                    setWeightedPixel(canvas, width, height, j, pixelY, color, weight);
                }
            }
        }
    }
}

static double calculate_distance(int x1, int y1, int x2, int y2)
{
    const int delta_y = y2 - y1;
    const int delta_x = x2 - x1;

    return sqrt((delta_y * delta_y) + (delta_x * delta_x));
}

/**
 * Merge the given circle into the given campus using the given color & weight
 */
static void drawCircle(Color_t *canvas, int width, int height,
        int circle_x, int circle_y, int radius, Color_t color, float weight)
{
    int top, left, bottom, right;
    int x, y;

    if (circle_x < 0 || circle_x > width)
    {
        fprintf(stderr, "Invalid circle location (0 <= x <= width).\n");
        return;
    }

    if (circle_y < 0 || circle_y > height)
    {
        fprintf(stderr, "Invalid circle location (0 <= y <= height).\n");
        return;
    }

    top  = MAX(0, circle_y - radius);
    left = MAX(0, circle_x - radius);
    bottom = MIN(height, circle_y + radius);
    right = MIN(width, circle_x + radius);

    for (y = top; y < bottom; y++)
    {
        for (x = left; x < right; x++)
        {
            if (calculate_distance(x, y, circle_x, circle_y) <= radius)
            {
                /* Color in the pixel */
                setWeightedPixel(canvas, width, height, x, y, color, weight);
            }
        }
    }
}

/**
 * Compare two canvases and return the difference between them.  This is done
 * by keeping a running sum of the differences.  This is rather slow, and
 * shouldn't be used anymore.
 */
static unsigned long canvasDiff(Color_t *src,
        Color_t *canvas, int width, int height)
{
    int x, y;
    int r1, g1, b1;
    int r2, g2, b2;
    int dr, dg, db;
    unsigned long d;

    d = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int index = coord_to_ind(x, y, width);

            r1 = src[index].red;
            g1 = src[index].green;
            b1 = src[index].blue;

            r2 = canvas[index].red;
            g2 = canvas[index].green;
            b2 = canvas[index].blue;

            if (r1 == -1 || g1 == -1 || b1 == -1
                    || r2 == -1 || g2 == -1 || b2 == -1)
                continue;

            dr = abs(r1 - r2);
            dg = abs(g1 - g2);
            db = abs(b1 - b2);

            d += dr + dg + db;
        }
    }

    return d;
}

/**
 * Compares two canvases.  This is an improvement upon the canvasDiff(...)
 * function because it will stop running after it determines that the canvases
 * are more different than the given difference argument.
 *
 * This saves time because this function doesn't require traversing the entire
 * canvas if it's not necessary.
 */
static int isSecondOneBetter(Color_t *first,
        Color_t *second, int width, int height, int difference)
{
    int x, y;
    int r1, g1, b1;
    int r2, g2, b2;
    int dr, dg, db;
    unsigned long d;

    d = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int index = coord_to_ind(x, y, width);

            r1 = first[index].red;
            g1 = first[index].green;
            b1 = first[index].blue;

            r2 = second[index].red;
            g2 = second[index].green;
            b2 = second[index].blue;

            if (r1 == -1 || g1 == -1 || b1 == -1
                    || r2 == -1 || g2 == -1 || b2 == -1)
                continue;

            dr = abs(r1 - r2);
            dg = abs(g1 - g2);
            db = abs(b1 - b2);

            d += dr + dg + db;
            if (d > difference && difference >= 0)
                return -1;
        }
    }

    if (difference < 0)
        return d;

    return (d < difference) ? d : -1;
}

static unsigned long regionDiff(Color_t *first, Color_t *second, int width, int height,
        int start_x, int start_y, int end_x, int end_y)
{
    int x, y;
    unsigned long d;

    d = 0;

    for (y = start_y; y <= end_y; y++)
    {
        for (x = start_x; x <= end_x; x++)
        {
            int index = coord_to_ind(x, y, width);
            int r1 = first[index].red;
            int g1 = first[index].green;
            int b1 = first[index].blue;
            int r2 = second[index].red;
            int g2 = second[index].green;
            int b2 = second[index].blue;

            if (r1 == -1 || g1 == -1 || b1 == -1 ||
                    r2 == -1 || g2 == -1 || b2 == -1)
            {
                continue;
            }

            int dr = abs(r1 - r2);
            int dg = abs(g1 - g2);
            int db = abs(b1 - b2);

            d += dr + dg + db;
        }
    }

    return d;
}

/**
 * Read in a PNG file and return it in a buffer pointer.  Make sure to fill
 * in the w & h parameters with the width & height of the image.  Given filename
 * must point to a PNG file.
 */
static Color_t *readPNG(char const *filename,
        int *w, int *h)
{
    int x, y;
    png_byte color_type;
    png_structp png_ptr;
    png_infop info_ptr;
    png_byte bit_depth;
    png_bytep *row_pointers;
    int width, height;
    char header[8];
    FILE *fp;
    Color_t *buffer;

    fp = fopen(filename, "rb");
    if (!fp)
        abort_("Unable to open %s.\n", filename);

    if (fread(header, 1, sizeof(header), fp) != 8)
        abort_("Read mismatch on header.\n");

    if (png_sig_cmp(header, 0, 8))
        abort_("File %s is not recognized as a PNG file.\n", filename);

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        abort_("Unable to create read struct.\n");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        abort_("Unable to create info structure.\n");

    if (setjmp(png_jmpbuf(png_ptr)))
        abort_("Error during read IO\n");

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    png_read_update_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
        abort_("Error during read_image\n");

    row_pointers = png_malloc(png_ptr, sizeof(png_bytep) * height);
    for (y = 0; y < height; y++)
        row_pointers[y] = png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));

    png_read_image(png_ptr, row_pointers);
    fclose(fp);

    if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGB)
        abort_("Expected RGB image.\n");

    /* Allocate memory for the buffer. */
    buffer = malloc(width * height * sizeof(Color_t));
    if (!buffer)
        abort_("Unable to allocate memory.\n");

    for (y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (x = 0; x < width; x++) {
            png_bytep px = &(row[x * 3]);
            Color_t *pColor = &buffer[coord_to_ind(x, y, width)];

            pColor->red   = px[0];
            pColor->green = px[1];
            pColor->blue  = px[2];
        }
    }

    for (y = 0; y < height; y++)
        png_free(png_ptr, row_pointers[y]);

    png_free(png_ptr, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (h)
        *h = height;
    if (w)
        *w = width;

    return buffer;
}

/**
 * Write a given canvas to a specific file.
 */
static int writePNG(char const *filename, Color_t *canvas,
        int width, int height)
{
    FILE *fp;
    png_structp png_ptr = NULL;
    png_infop info_ptr  = NULL;
    png_byte ** row_pointers = NULL;
    int status = -1;
    int pixel_size = 3;
    int depth = 8;
    size_t x, y;

    fp = fopen(filename, "wb");
    if (!fp)
        abort_("Unable to open file %s\n", filename);

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        abort_("Unable to create PNG write struct.\n");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        abort_("Unable to create info structure.\n");

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during PNG creation.\n");
        return -1;
    }

    png_set_IHDR(png_ptr,
            info_ptr,
            width,
            height,
            depth,
            PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);

    row_pointers = png_malloc(png_ptr, height * sizeof(png_byte *));
    for (y = 0; y < height; y++) {
        png_byte *row = png_malloc(png_ptr, sizeof(uint8_t) * width* pixel_size);
        row_pointers[y] = row;
        for (x = 0; x < width; x++) {
            int index = coord_to_ind(x, y, width);
            *row++ = canvas[index].red;
            *row++ = canvas[index].green;
            *row++ = canvas[index].blue;
        }
    }

    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    status = 0;

    for (y = 0; y < height; y++)
        png_free(png_ptr, row_pointers[y]);

    png_free(png_ptr, row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return status;
}

/**
 * This is the main loop for the program.  It is given a reference image and
 * the size, as well as various parameters used to define polygon type,
 * how many to use, and the target difference percentage.
 */
static void main_loop(Color_t *original, int width, int height,
        int n_points, int n_polygons, double target_percentage)
{
    int old_diff = -1;
    unsigned int new_diff;
    unsigned int n_used, n_tried;
    unsigned int max_diff;
    double current_percent = 0.0f;
    char output[64];
    Color_t *temporary;
    Color_t *canvas;

    n_used = n_tried = 0;

    /* The most different a test image can be. */
    max_diff = MAX_COLOR_VALUE * (unsigned int)width * (unsigned int)height * 3;

    /* Allocate the buffers used--one for a temporary buffer, and one for
     * holding our work-in-progress. */
    temporary = malloc(width * height * sizeof(Color_t));
    if (!temporary)
        abort_("Unable to allocate temporary buffer.\n");

    canvas = malloc(width * height * sizeof(Color_t));
    if (!canvas)
        abort_("Unable to allocate canvas buffer.\n");

    /* Start with a blank canvas. */
    clearCanvas(canvas, width, height);

    do {
        n_tried++;

        /* Generate a randomly-colored circle. */
        Color_t color = getRandomColor();
        double weight = drandrange(0.25, 0.75);
        int x = randrange(0, width);
        int y = randrange(0, height);
        int radius = randrange(1, MIN(width, height) / 15);

        /* Create the temporary canvas by starting with the current work in
         * progress. */
        memcpy(temporary, canvas, width*height*sizeof(Color_t));

        /* Add a polygon. */
        //drawPolygon(temporary, width, height, polygon, n_points, color, weight);
        drawCircle(temporary, width, height, x, y, radius, color, weight);

        unsigned long diff = regionDiff(original, temporary,
                width, height,
                MAX(0, x - radius), MAX(0, y - radius),
                MIN(width, x + radius), MIN(height, y + radius));
        unsigned long prev_diff = regionDiff(original, canvas,
                width, height,
                MAX(0, x - radius), MAX(0, y - radius),
                MIN(width, x + radius), MIN(height, y + radius));

        if (diff < prev_diff)
        {
            /* Use the changes */
            n_used++;

            printf("%d / %d (tested %d) (Weight %2.2f)\n",
                    n_used, n_polygons, n_tried, weight);
            memcpy(canvas, temporary, width*height*sizeof(Color_t));
            sprintf(output, "./out/img_%d.png", n_used);
            writePNG(output, canvas, width, height);
        }

#if 0
        /* Compare to the original. */
        new_diff = isSecondOneBetter(original, temporary, width, height, old_diff);
        if (new_diff < 0)
            continue;

        /* If we've improved, keep the new version */
        if (old_diff < 0 || new_diff < old_diff) {
            n_used++;
            current_percent =
                100.0f - ((100.0f * (double)new_diff) / (double)max_diff);

            printf("%d / %d (tested %d) -- %.02f%% (Weight %2.2f)\n",
                    n_used, n_polygons, n_tried, current_percent, weight);

            memcpy(canvas, temporary, width*height*sizeof(Color_t));
            old_diff = new_diff;
            sprintf(output, "./out/img_%d.png", n_used);
            writePNG(output, canvas, width, height);

            if (current_percent > target_percentage && target_percentage > 0.0f)
                break;
        }
#endif
    } while (n_used < n_polygons);

    free(temporary);
    free(canvas);
}

/**
 * Get any arguments from the command line.  This should be cleaned up, because
 * it feels way hackier than it should.
 */
static void process_args(int argc, char **argv,
        char **src_path, int *n_points, int *n_polygons, double *percentage)
{
    int c;
    int digit_optind = 0;
    static struct option long_options[] = {
        { "src",   required_argument, 0, 0 },
        { "sides", required_argument, 0, 0 },
        { "npoly", required_argument, 0, 0 },
        { "perc",  required_argument, 0, 0 },
        { NULL, 0, NULL, 0 }
    };
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
        case 0:
            if (option_index == 0) {
                *src_path = optarg;
            } else if (option_index == 1) {
                *n_points = atoi(optarg);
                if (*n_points <= 2)
                    abort_("Must have at least 3 points.\n");
                else if (*n_points > MAX_POLYGON_POINTS)
                    abort_("Must have <= %d points.\n", MAX_POLYGON_POINTS);
            } else if (option_index == 2) {
                *n_polygons = atoi(optarg);
                if (*n_polygons <= 0)
                    abort_("Must have at least 1 polygon.\n");
            } else if (option_index == 3) {
                *percentage = atof(optarg);
            }
            break;

        default:
            printf("Usage: %s\n", argv[0]);
            printf("\t--src <path to PNG src image>\n");
            printf("\t--sides <# of polygon sides>\n");
            printf("\t--npoly <# of polygons to generate>\n");
            printf("\t--perc <Target accuracy percentage <= 100.0f>\n");
            exit(0);
            break;
        }
    }
}

/**
 * Entry point for the program.
 */
int main(int argc, char **argv)
{
    Color_t *original;
    int width, height;
    int n_polygons           = N_POLYGONS;
    int n_sides              = DEFAULT_POLYGON_POINTS;
    double target_percentage = -1.0f;
    char *filename           = INPUT_IMAGE;

    /* Check for any command-line arguments. */
    process_args(argc, argv, &filename, &n_sides, &n_polygons,
            &target_percentage);

    /* Seed the random number generated with the current time. */
    srand(time(NULL));

    /* Read in the original image. */
    original = readPNG(filename, &width, &height);

    /* Kick off the polygon loop. */
    main_loop(original, width, height, n_sides, n_polygons, target_percentage);

    free(original);
    return 0;
}

