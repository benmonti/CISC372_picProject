#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "pthreads_image.h"
#include <pthread.h>
#include <sys/time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int thread_count, grid_rows, grid_cols;

// An array of kernel matrices to be used for image convolution.
// The indexes of these match the enumeration from the header file. ie. algorithms[BLUR] returns the kernel corresponding to a box blur.
Matrix algorithms[] = {
    {{0, -1, 0}, {-1, 4, -1}, {0, -1, 0}},
    {{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}},
    {{1 / 9.0, 1 / 9.0, 1 / 9.0}, {1 / 9.0, 1 / 9.0, 1 / 9.0}, {1 / 9.0, 1 / 9.0, 1 / 9.0}},
    {{1.0 / 16, 1.0 / 8, 1.0 / 16}, {1.0 / 8, 1.0 / 4, 1.0 / 8}, {1.0 / 16, 1.0 / 8, 1.0 / 16}},
    {{-2, -1, 0}, {-1, 1, 1}, {0, 1, 2}},
    {{0, 0, 0}, {0, 1, 0}, {0, 0, 0}}};

// getPixelValue - Computes the value of a specific pixel on a specific channel using the selected convolution kernel
// Paramters: srcImage:  An Image struct populated with the image being convoluted
//            x: The x coordinate of the pixel
//           y: The y coordinate of the pixel
//           bit: The color channel being manipulated
//           algorithm: The 3x3 kernel matrix to use for the convolution
// Returns: The new value for this x,y pixel and bit channel
uint8_t getPixelValue(Image *srcImage, int x, int y, int bit, Matrix *algorithm)
{
    int px, mx, py, my, i, span;
    span = srcImage->width * srcImage->bpp;
    // for the edge pixes, just reuse the edge pixel
    px = x + 1;
    py = y + 1;
    mx = x - 1;
    my = y - 1;
    if (mx < 0)
        mx = 0;
    if (my < 0)
        my = 0;
    if (px >= srcImage->width)
        px = srcImage->width - 1;
    if (py >= srcImage->height)
        py = srcImage->height - 1;
    uint8_t result =
        (*algorithm)[0][0] * srcImage->data[Index(mx, my, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[0][1] * srcImage->data[Index(x, my, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[0][2] * srcImage->data[Index(px, my, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[1][0] * srcImage->data[Index(mx, y, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[1][1] * srcImage->data[Index(x, y, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[1][2] * srcImage->data[Index(px, y, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[2][0] * srcImage->data[Index(mx, py, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[2][1] * srcImage->data[Index(x, py, srcImage->width, bit, srcImage->bpp)] +
        (*algorithm)[2][2] * srcImage->data[Index(px, py, srcImage->width, bit, srcImage->bpp)];
    return result;
}

typedef struct
{
    Image *srcImage;
    Image *destImage;
    Matrix *algorithm;
    long threadRank;
    int start_row;
    int end_row;
    int start_col;
    int end_col;
} ThreadData;

// convolute:  Applies a kernel matrix to an image
// Parameters: srcImage: The image being convoluted
//             destImage: A pointer to a  pre-allocated (including space for the pixel array) structure to receive the convoluted image.  It should be the same size as srcImage
//             algorithm: The kernel matrix to use for the convolution
// Returns: Nothing
void *convolute(void *td)
{
    ThreadData *data = (ThreadData *)td;
    int my_rank = data->threadRank;
    int row, pix, bit, span;
    Image *srcImage = data->srcImage;
    Image *destImage = data->destImage;
    span = srcImage->bpp * srcImage->bpp;
    int start_row, end_row, start_col, end_col;
    start_row = data->start_row;
    end_row = data->end_row;
    start_col = data->start_col;
    end_col = data->end_col;
    printf("Thread: %d - Start row: %d - End row: %d\n", my_rank, start_row, end_row);
    printf("Thread: %d - Start col: %d - End col: %d\n", my_rank, start_col, end_col);
    for (row = start_row; row < end_row; row++)
    {
        for (pix = start_col; pix < end_col; pix++)
        {
            for (bit = 0; bit < srcImage->bpp; bit++)
            {
                destImage->data[Index(pix, row, srcImage->width, bit, srcImage->bpp)] = getPixelValue(srcImage, pix, row, bit, data->algorithm);
            }
        }
    }
    return NULL;
}

// Usage: Prints usage information for the program
// Returns: -1
int Usage()
{
    printf("Usage: image <filename> <type>\n\twhere type is one of (edge,sharpen,blur,gauss,emboss,identity)\n");
    return -1;
}

// GetKernelType: Converts the string name of a convolution into a value from the KernelTypes enumeration
// Parameters: type: A string representation of the type
// Returns: an appropriate entry from the KernelTypes enumeration, defaults to IDENTITY, which does nothing but copy the image.
enum KernelTypes GetKernelType(char *type)
{
    if (!strcmp(type, "edge"))
        return EDGE;
    else if (!strcmp(type, "sharpen"))
        return SHARPEN;
    else if (!strcmp(type, "blur"))
        return BLUR;
    else if (!strcmp(type, "gauss"))
        return GAUSE_BLUR;
    else if (!strcmp(type, "emboss"))
        return EMBOSS;
    else
        return IDENTITY;
}

// main:
// argv is expected to take 2 arguments.  First is the source file name (can be jpg, png, bmp, tga).  Second is the lower case name of the algorithm.
int main(int argc, char **argv)
{

    pthread_t *thread_handles;
    long t1, t2, thread;

    stbi_set_flip_vertically_on_load(0);
    if (argc != 4)
        return Usage();
    char *fileName = argv[1];
    if (!strcmp(argv[1], "pic4.jpg") && !strcmp(argv[2], "gauss"))
    {
        printf("You have applied a gaussian filter to Gauss which has caused a tear in the time-space continum.\n");
    }
    enum KernelTypes type = GetKernelType(argv[2]);

    Image srcImage, destImage, bwImage;
    srcImage.data = stbi_load(fileName, &srcImage.width, &srcImage.height, &srcImage.bpp, 0);
    if (!srcImage.data)
    {
        printf("Error loading file %s.\n", fileName);
        return -1;
    }
    destImage.bpp = srcImage.bpp;
    destImage.height = srcImage.height;
    destImage.width = srcImage.width;
    destImage.data = malloc(sizeof(uint8_t) * destImage.width * destImage.bpp * destImage.height);
    thread_count = strtol(argv[3], NULL, 10);

    thread_handles = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    grid_rows = (int)floor(sqrt(thread_count));
    while (thread_count % grid_rows != 0)
    {
        grid_rows--;
    }
    grid_cols = thread_count / grid_rows;
    int tile_height = srcImage.height / grid_rows;
    int tile_width = srcImage.width / grid_cols;
    struct timespec start, finish;
    clock_gettime(CLOCK_REALTIME, &start);
    // t1 = time(NULL);
    for (thread = 0; thread < thread_count; thread++)
    {
        ThreadData *td = malloc(sizeof(ThreadData));
        td->srcImage = &srcImage;
        td->destImage = &destImage;
        td->algorithm = &algorithms[type];
        td->threadRank = thread;
        int r = thread / grid_cols;
        int c = thread % grid_cols;
        td->start_row = r * tile_height;
        td->end_row = (r == grid_rows - 1) ? srcImage.height : (r + 1) * tile_height;
        td->start_col = c * tile_width;
        td->end_col = (c == grid_cols - 1) ? srcImage.width : (c + 1) * tile_width;
        pthread_create(&thread_handles[thread], NULL, &convolute, td);
    }
    for (thread = 0; thread < thread_count; thread++)
    {
        pthread_join(thread_handles[thread], NULL);
    }
    free(thread_handles);
    printf("Image height: %d\nImage width: %d\n", destImage.height, destImage.width);
    stbi_write_png("output.png", destImage.width, destImage.height, destImage.bpp, destImage.data, destImage.bpp * destImage.width);
    stbi_image_free(srcImage.data);

    free(destImage.data);
    clock_gettime(CLOCK_REALTIME, &finish);
    // t2 = time(NULL);
    double time_spent = (double)(finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Took %f seconds\n", time_spent);
    return 0;
}