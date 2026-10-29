#include <stdio.h>
#include "jpeglib.h"

int CreateJPG(FILE *outfile, int width, int height, JSAMPROW (*getRow)(int))
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row;

    if (outfile && width && height && getRow) {
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&cinfo);
        cinfo.input_components = 3;
        cinfo.X_density = 72;
        cinfo.Y_density = 72;
        cinfo.data_precision = 8;
        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.density_unit = 1;
        jpeg_default_colorspace(&cinfo);
        jpeg_stdio_dest(&cinfo, outfile);
        jpeg_start_compress(&cinfo, TRUE);
        while (height--) {
            row = getRow(width);
            jpeg_write_scanlines(&cinfo, &row, 1);
        }
        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
        return 0;
    }
    return 1;
}
