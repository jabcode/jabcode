/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file image.c
 * @brief Read and save png image
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "jabcode.h"
#include "png.h"

/**
 * @brief Save code bitmap as png image
 * @param bitmap the code bitmap
 * @param filename the image filename
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean saveImage(jab_bitmap* bitmap, jab_char* filename)
{
	png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

    if(bitmap->channel_count == 4)
    {
		image.format = PNG_FORMAT_RGBA;
		image.flags  = PNG_FORMAT_FLAG_ALPHA | PNG_FORMAT_FLAG_COLOR;
    }
    else
    {
		image.format = PNG_FORMAT_GRAY;
    }

    image.width  = bitmap->width;
    image.height = bitmap->height;

    if (png_image_write_to_file(&image,
								filename,
								0/*convert_to_8bit*/,
								bitmap->pixel,
								0/*row_stride*/,
								NULL/*colormap*/) == 0)
	{
		reportError(image.message);
		reportError("Saving png image failed");
		return JAB_FAILURE;
	}
	return JAB_SUCCESS;
}

/**
 * @brief Read image into code bitmap
 * @param filename the image filename
 * @return Pointer to the code bitmap read from image | NULL
*/
jab_bitmap* readImage(jab_char* filename)
{
	png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

	jab_bitmap* bitmap;

    if(png_image_begin_read_from_file(&image, filename))
	{
		image.format = PNG_FORMAT_RGBA;

		bitmap = (jab_bitmap *)calloc(1, sizeof(jab_bitmap) + PNG_IMAGE_SIZE(image));
		if(bitmap == NULL)
        {
			png_image_free(&image);
			reportError("Memory allocation failed");
			return NULL;
		}
		bitmap->width = image.width;
		bitmap->height= image.height;
		bitmap->bits_per_channel = BITMAP_BITS_PER_CHANNEL;
		bitmap->bits_per_pixel = BITMAP_BITS_PER_PIXEL;
		bitmap->channel_count = BITMAP_CHANNEL_COUNT;

        if(png_image_finish_read(&image,
								 NULL/*background*/,
								 bitmap->pixel,
								 0/*row_stride*/,
								 NULL/*colormap*/) == 0)
		{
			free(bitmap);
			reportError(image.message);
			reportError("Reading png image failed");
			return NULL;
		}
	}
	else
	{
		reportError(image.message);
		reportError("Opening png image failed");
		return NULL;
	}
	return bitmap;
}
