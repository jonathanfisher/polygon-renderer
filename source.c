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

#define POLYGON_POINTS 8

#define MAX_POLYGON_POINTS 10

#define N_POLYGONS 200

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

static void abort_(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

static void printColor(Color_t c)
{
    printf("RGB: (%d, %d, %d)\n", c.red, c.green, c.blue);
}

static void printPoint(Point_t p)
{
    printf("(%d, %d)\n", p.x, p.y);
}

static void printPolygon(Polygon_t p, int n_points)
{
    int i;

    for (i = 0; i < n_points; i++)
        printPoint(p.Points[i]);
}

static int coord_to_ind(int x, int y, int width)
{
    return (y * width) + x;
}

static int randrange(int low, int high)
{
    double r;

    if (low > high)
        abort_("Lower boundary must be <= high boundary.\n");

    r = ((double)rand() / (double)((double)RAND_MAX + 1.0)) * (high - low + 1) + low;
    return (int)r;
}

static Color_t getRandomColor(void)
{
    Color_t color;

    color.red   = randrange(0, 255);
    color.green = randrange(0, 255);
    color.blue  = randrange(0, 255);

    return color;
}

static Point_t getRandomPoint(int width, int height)
{
    Point_t pt;

    pt.x = randrange(0, width);
    pt.y = randrange(0, height);

    return pt;
}

static Polygon_t getRandomPolygon(int width, int height, int n_points)
{
    Polygon_t pg = {0};
    int i;

    for (i = 0; i < n_points; i++)
        pg.Points[i] = getRandomPoint(width, height);

    return pg;
}

static unsigned char weightedAverage(int a,
        int b, float weight)
{
    int result;

    if (unlikely(weight > 1.0f || weight < 0.0f))
        abort_("Weight must be between 0 and 1\n");

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

static Color_t averageColors(Color_t a, Color_t b, float weight)
{
    Color_t avg;

    avg.red   = weightedAverage(a.red, b.red, weight);
    avg.green = weightedAverage(a.green, b.green, weight);
    avg.blue  = weightedAverage(a.blue, b.blue, weight);

    return avg;
}

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

static void setWeightedPixel(Color_t *canvas, int width, int height,
        int x, int y, Color_t color, float weight)
{
    Color_t avg = averageColors(canvas[coord_to_ind(x, y, width)],
            color, weight);

    setPixel(canvas, width, height, x, y, avg.red, avg.green, avg.blue);
}

static void clearCanvas(Color_t *canvas, int width, int height)
{
    int i, j;

    for (j = 0; j < height; j++)
        for (i = 0; i < width; i++)
            setPixel(canvas, width, height, i, j, 0, 0, 0);
}

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

    temporary = malloc(width * height * sizeof(Color_t));
    if (!temporary)
        abort_("Unable to allocate temporary buffer.\n");

    canvas = malloc(width * height * sizeof(Color_t));
    if (!canvas)
        abort_("Unable to allocate canvas buffer.\n");

    clearCanvas(canvas, width, height);

    do {
        n_tried++;

        /* Generate a randomly-colored polygon. */
        Color_t color = getRandomColor();
        Polygon_t polygon = getRandomPolygon(width, height, n_points);

        /* Create the temporary canvas. */
        memcpy(temporary, canvas, width*height*sizeof(Color_t));

        /* Add a polygon. */
        drawPolygon(temporary, width, height, polygon, n_points, color, 0.5f);

        /* Compare to the original. */
        new_diff = isSecondOneBetter(original, temporary, width, height, old_diff);
        if (new_diff < 0)
            continue;

        /* If we've improved, keep the new version */
        if (old_diff < 0 || new_diff < old_diff) {
            n_used++;
            current_percent =
                100.0f - ((100.0f * (double)new_diff) / (double)max_diff);

            printf("%d / %d (tested %d) -- %.02f%%\n",
                    n_used, n_polygons, n_tried, current_percent);

            memcpy(canvas, temporary, width*height*sizeof(Color_t));
            old_diff = new_diff;
            sprintf(output, "./out/img_%d.png", n_used);
            writePNG(output, canvas, width, height);

            if (current_percent > target_percentage && target_percentage > 0.0f)
                break;
        }
    } while (n_used < n_polygons);

    free(temporary);
    free(canvas);
}

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
            exit(0);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    Color_t *original;
    int width, height;
    int n_polygons = N_POLYGONS;
    int n_sides    = POLYGON_POINTS;
    double target_percentage = -1.0f;
    char *filename = INPUT_IMAGE;

    process_args(argc, argv, &filename, &n_sides, &n_polygons,
            &target_percentage);

    srand(time(NULL));

    original = readPNG(filename, &width, &height);
    main_loop(original, width, height, n_sides, n_polygons, target_percentage);

    free(original);
    return 0;
}

