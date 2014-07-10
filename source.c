#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <png.h>

#define FILENAME    "raw.dat"
#define OUTPUT_FILE "c_raw.dat"

#define WIDTH  200
#define HEIGHT 200

#define POLYGON_POINTS 5

#define N_POLYGONS 200

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
    Point_t Points[POLYGON_POINTS];
} Polygon_t;

static Color_t mOriginal[WIDTH][HEIGHT];
static Color_t mCanvas[WIDTH][HEIGHT];
static Color_t mTemporary[WIDTH][HEIGHT];

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

static void printPolygon(Polygon_t p)
{
    int i;

    for (i = 0; i < POLYGON_POINTS; i++)
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

static Polygon_t getRandomPolygon(int width, int height)
{
    Polygon_t pg = {0};
    int i;

    for (i = 0; i < POLYGON_POINTS; i++)
        pg.Points[i] = getRandomPoint(width, height);

    return pg;
}

static unsigned char weightedAverage(int a,
        int b, float weight)
{
    int result;

    if (weight > 1.0f || weight < 0.0f)
        abort_("Weight must be between 0 and 1\n");

    if (a == -1 && b == -1)
        abort_("%s: At least one of the arguments must be >= 0.\n", __func__);

    if (a == -1)
        return b;

    if (b == -1)
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

static void setPixel(Color_t canvas[WIDTH][HEIGHT], int x, int y,
        int r, int g, int b)
{
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        canvas[x][y].red   = r;
        canvas[x][y].green = g;
        canvas[x][y].blue  = b;
    }
}

static void setWeightedPixel(Color_t canvas[WIDTH][HEIGHT], int x, int y,
        Color_t color, float weight)
{
    Color_t avg = averageColors(canvas[x][y], color, weight);

    setPixel(canvas, x, y, avg.red, avg.green, avg.blue);
}

static void clearCanvas(Color_t canvas[WIDTH][HEIGHT])
{
    int i, j;

    for (j = 0; j < HEIGHT; j++)
        for (i = 0; i < WIDTH; i++)
            setPixel(canvas, i, j, 0, 0, 0);
}

static void drawPolygon(Color_t canvas[WIDTH][HEIGHT],
        Polygon_t pg, Color_t color, float weight)
{
    int nodes, nodeX[POLYGON_POINTS], pixelX, pixelY, i, j, swap;

    for (pixelY = 0; pixelY < HEIGHT; pixelY++) {
        // Build a list of nodes.
        nodes = 0;
        j = POLYGON_POINTS - 1;
        for (i = 0; i < POLYGON_POINTS; i++) {
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
            if (nodeX[i] >= WIDTH) break;
            if (nodeX[i + 1] > 0) {
                if (nodeX[i] < 0) nodeX[i] = 0;
                if (nodeX[i+1] >= WIDTH) nodeX[i+1] = WIDTH-1;
                for (j = nodeX[i]; j < nodeX[i + 1]; j++) {
                    /* Fill the pixel. */
                    //setPixel(canvas, j, pixelY, color.red, color.green, color.blue);
                    /* Average the pixels. */
                    setWeightedPixel(canvas, j, pixelY, color, weight);
                }
            }
        }
    }
}

static unsigned long canvasDiff(Color_t src[WIDTH][HEIGHT],
        Color_t canvas[WIDTH][HEIGHT])
{
    int x, y;
    int r1, g1, b1;
    int r2, g2, b2;
    int dr, dg, db;
    unsigned long d;

    d = 0;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            r1 = src[x][y].red;
            g1 = src[x][y].green;
            b1 = src[x][y].blue;

            r2 = canvas[x][y].red;
            g2 = canvas[x][y].green;
            b2 = canvas[x][y].blue;

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

static int isSecondOneBetter(Color_t first[WIDTH][HEIGHT],
        Color_t second[WIDTH][HEIGHT], int difference)
{
    int x, y;
    int r1, g1, b1;
    int r2, g2, b2;
    int dr, dg, db;
    unsigned long d;

    d = 0;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            r1 = first[x][y].red;
            g1 = first[x][y].green;
            b1 = first[x][y].blue;

            r2 = second[x][y].red;
            g2 = second[x][y].green;
            b2 = second[x][y].blue;

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

static int readFile(char const *filename)
{
    FILE *fp;
    char line[80];
    int x, y;

    fp = fopen(filename, "rt");
    if (!fp)
        abort_("Unable to open file %s.\n", filename);

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            if (!fgets(line, sizeof(line), fp))
                abort_("Unable to read file.\n");
            sscanf(line, "%d, %d, %d",
                    &mOriginal[x][y].red,
                    &mOriginal[x][y].green,
                    &mOriginal[x][y].blue);
        }
    }

    fclose(fp);
    return 0;
}

static Color_t *readPNG(char const *filename, Color_t dst[WIDTH][HEIGHT],
        int *h, int *w)
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

            dst[x][y].red   = px[0];
            dst[x][y].green = px[1];
            dst[x][y].blue  = px[2];
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

static int writePNG(char const *filename, Color_t canvas[WIDTH][HEIGHT])
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
            WIDTH,
            HEIGHT,
            depth,
            PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);

    row_pointers = png_malloc(png_ptr, HEIGHT * sizeof(png_byte *));
    for (y = 0; y < HEIGHT; y++) {
        png_byte *row = png_malloc(png_ptr, sizeof(uint8_t) * WIDTH * pixel_size);
        row_pointers[y] = row;
        for (x = 0; x < WIDTH; x++) {
            *row++ = canvas[x][y].red;
            *row++ = canvas[x][y].green;
            *row++ = canvas[x][y].blue;
        }
    }

    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    status = 0;

    for (y = 0; y < HEIGHT; y++)
        png_free(png_ptr, row_pointers[y]);

    png_free(png_ptr, row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return status;
}

static void main_loop(Color_t const *original, int width, int height)
{
    int old_diff = -1;
    int new_diff;
    unsigned int n_used, n_tried;
    char output[64];

    n_used = n_tried = 0;

    clearCanvas(mCanvas);

    do {
        n_tried++;
        Color_t color = getRandomColor();
        Polygon_t polygon = getRandomPolygon(width, height);

        /* Create the temporary canvas. */
        memcpy(mTemporary, mCanvas, sizeof(mCanvas));

        /* Add a polygon. */
        drawPolygon(mTemporary, polygon, color, 0.5f);

        /* Compare to the original. */
        new_diff = isSecondOneBetter(mOriginal, mTemporary, old_diff);
        if (new_diff < 0)
            continue;

        /* If we've improved, keep the new version */
        if (old_diff < 0 || new_diff < old_diff) {
            n_used++;

            printf("Improved by %d!  %u / %u\n",
                    abs(old_diff - new_diff), n_used, n_tried);

            memcpy(mCanvas, mTemporary, sizeof(mCanvas));
            old_diff = new_diff;
            sprintf(output, "./out/img_%d.png", n_used);
            writePNG(output, mCanvas);
        }
    } while (n_used < N_POLYGONS);
}

int main(int argc, char **argv)
{
    char const *filename;
    Color_t *original;
    int width, height;

    srand(time(NULL));

    if (argc < 2)
        filename = FILENAME;
    else
        filename = argv[1];

    original = readPNG("starry-night-200x200.png", mOriginal, &width, &height);
    main_loop(original, width, height);

    free(original);
    return 0;
}

