#include <png.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#define FILENAME    "raw.dat"
#define OUTPUT_FILE "c_raw.dat"

#define WIDTH  200
#define HEIGHT 200

#define POLYGON_POINTS 6

#define N_POLYGONS 300

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

static void writeFile(char const *filename, Color_t canvas[WIDTH][HEIGHT])
{
    FILE *fp;
    int i, j;

    fp = fopen(filename, "wt");
    if (!fp)
        abort_("Unable to open file %s\n", filename);

    for (j = 0; j < HEIGHT; j++) {
        for (i = 0; i < WIDTH; i++) {
            if (canvas[i][j].red < 0)
                canvas[i][j].red = 0;
            if (canvas[i][j].green < 0)
                canvas[i][j].green = 0;
            if (canvas[i][j].blue < 0)
                canvas[i][j].blue = 0;

            fprintf(fp, "%d, %d, %d\n",
                    canvas[i][j].red, canvas[i][j].green, canvas[i][j].blue);
        }
    }

    fclose(fp);
}

static void main_loop(void)
{
    int old_diff = -1;
    int new_diff;
    unsigned int n_used, n_tried;
    char output[64];

    int n = 1000000;

    n_used = n_tried = 0;

    clearCanvas(mCanvas);

    do {
        n_tried++;
        Color_t color = getRandomColor();
        Polygon_t polygon = getRandomPolygon(WIDTH, HEIGHT);

        /* Create the temporary canvas. */
        memcpy(mTemporary, mCanvas, sizeof(mCanvas));

        /* Add a polygon. */
        drawPolygon(mTemporary, polygon, color, 0.5f);

        /* Compare to the original. */
        new_diff = canvasDiff(mOriginal, mTemporary);

        /* If we've improved, keep the new version */
        if (old_diff < 0 || new_diff < old_diff) {
            n_used++;

            printf("Improved by %d!  %u / %u\n",
                    abs(old_diff - new_diff), n_used, n_tried);

            memcpy(mCanvas, mTemporary, sizeof(mCanvas));
            old_diff = new_diff;
            sprintf(output, "./out/img_%d.dat", n_used);
            writeFile(output, mCanvas);
        }
    } while (n_used < N_POLYGONS);
}

int main(int argc, char **argv)
{
    char const *filename;

    srand(time(NULL));

    if (argc < 2)
        filename = FILENAME;
    else
        filename = argv[1];

    readFile(filename);

    main_loop();

    return 0;
}

