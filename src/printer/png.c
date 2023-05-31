/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Provide centralized access to the PNG image handler.
 *
 * Version:	@(#)png.c	1.0.9	2021/03/18
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018-2021 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/plat.h>
#include <86box/ui.h>
#include <86box/video.h>	// for color_transform()
#include <86box/png.h>


#ifdef USE_LIBPNG
# define PNG_DEBUG 0
# include <png.h>


# ifdef _WIN32
#  define PATH_PNG_DLL		"libpng16.dll"
# else
#  define PATH_PNG_DLL		"libpng16.so"
# endif
# define USE_CUSTOM_IO		1


static void			*png_handle = NULL;	/* handle to DLL */
# if USE_LIBPNG == 1
#  define PNGFUNC(x)		png_ ## x
# else
#  define PNGFUNC(x)		PNG_ ## x


/* Pointers to the real functions. */
static png_structp	(*PNG_create_write_struct)(png_const_charp user_png_ver,
						png_voidp error_ptr,
						png_error_ptr error_fn,
						png_error_ptr warn_fn);
static void		(*PNG_destroy_write_struct)(png_structpp png_ptr_ptr,
					    png_infopp info_ptr_ptr);
static png_infop	(*PNG_create_info_struct)(png_const_structrp png_ptr);
#  if USE_CUSTOM_IO
static void		(*PNG_set_write_fn)(png_structrp png_ptr,
					    png_voidp io_ptr,
					    png_rw_ptr write_data_fn,
					    png_flush_ptr output_flush_fn);
static png_voidp	(*PNG_get_io_ptr)(png_const_structrp png_ptr);
#  else
static void		(*PNG_init_io)(png_structrp png_ptr, png_FILE_p fp);
#  endif
static void		(*PNG_set_IHDR)(png_const_structrp png_ptr,
				     png_inforp info_ptr, png_uint_32 width,
				     png_uint_32 height, int bit_depth,
				     int color_type, int interlace_method,
				     int compression_method,
				     int filter_method);
static void		(*PNG_set_PLTE)(png_structrp png_ptr,
				        png_inforp info_ptr,
					png_const_colorp palette,
					int num_palette);
static void		(*PNG_set_rows)(png_const_structrp png_ptr,
				        png_inforp info_ptr,
				        png_bytepp row_pointers);
static void		(*PNG_write_png)(png_structrp png_ptr,
					 png_inforp info_ptr,
					 int transforms, png_voidp params);
static png_size_t	(*PNG_get_rowbytes)(png_const_structrp png_ptr,
				    png_const_inforp info_ptr);
static void		(*PNG_write_info)(png_structrp png_ptr,
				       png_const_inforp info_ptr);
static void		(*PNG_write_image)(png_structrp png_ptr,
					png_bytepp image);
static void		(*PNG_write_row)(png_structrp png_ptr,
					png_bytep row);
static void		(*PNG_write_rows)(png_structrp png_ptr,
					png_bytepp rows, int num);
static void		(*PNG_write_end)(png_structrp png_ptr,
				      png_inforp info_ptr);

static void		(*PNG_set_compression_level)(png_structrp png_ptr,
						     int level);
static void		(*PNG_set_compression_mem_level)(png_structrp png_ptr,
							 int mem_level);
static void		(*PNG_set_compression_strategy)(png_structrp png_ptr,
							int strategy);
static void		(*PNG_set_compression_window_bits)(png_structrp png_ptr,
							   int window_bits);
static void		(*PNG_set_compression_method)(png_structrp png_ptr,
						      int method);
static void		(*PNG_set_compression_buffer_size)(png_structrp png_ptr,
							   png_size_t size);


static const dllimp_t png_imports[] = {
  { "png_create_write_struct",		&PNG_create_write_struct	},
  { "png_destroy_write_struct",		&PNG_destroy_write_struct	},
  { "png_create_info_struct",		&PNG_create_info_struct		},
#  if USE_CUSTOM_IO
  { "png_set_write_fn",			&PNG_set_write_fn		},
  { "png_get_io_ptr",			&PNG_get_io_ptr			},
#  else
  { "png_init_io",			&PNG_init_io			},
#  endif
  { "png_set_IHDR",			&PNG_set_IHDR			},
  { "png_set_PLTE",			&PNG_set_PLTE			},
  { "png_get_rowbytes",			&PNG_get_rowbytes		},
  { "png_set_rows",			&PNG_set_rows			},
  { "png_write_png",			&PNG_write_png			},
  { "png_write_info",			&PNG_write_info			},
  { "png_write_row",			&PNG_write_row			},
  { "png_write_rows",			&PNG_write_rows			},
  { "png_write_image",			&PNG_write_image		},
  { "png_write_end",			&PNG_write_end			},
  { "png_set_compression_level",	&PNG_set_compression_level	},
  { "png_set_compression_mem_level",	&PNG_set_compression_mem_level	},
  { "png_set_compression_strategy",	&PNG_set_compression_strategy	},
  { "png_set_compression_window_bits",	&PNG_set_compression_window_bits},
  { "png_set_compression_method",	&PNG_set_compression_method	},
  { "png_set_compression_buffer_size",	&PNG_set_compression_buffer_size},
  { NULL,				NULL				}
};
# endif


static void
error_handler(png_structp arg, const char *str)
{
    ERRLOG("PNG: stream 0x%08lx error '%s'\n", arg, str);
}


static void
warning_handler(png_structp arg, const char *str)
{
    ERRLOG("PNG: stream 0x%08lx warning '%s'\n", arg, str);
}


# if USE_CUSTOM_IO
/* Use custom I/O Write function to avoid debug-dll issues. */
static void
write_handler(png_struct *png_ptr, png_byte *bufp, png_size_t len)
{
    FILE *fp = (FILE *)PNGFUNC(get_io_ptr)(png_ptr);

    if (fwrite(bufp, 1, len, fp) != len)
	ERRLOG("PNG: error writing file!\n");
}


/* Use custom I/O Flush function to avoid debug-dll issues. */
static void 
flush_handler(png_struct *png_ptr)
{
    FILE *fp = (FILE *)PNGFUNC(get_io_ptr)(png_ptr);

    fflush(fp);
}
# endif


/* Prepare for use, load DLL if needed. */
int
png_load(void)
{
# if USE_LIBPNG == 2
    wchar_t temp[512];
    const char *fn = PATH_PNG_DLL;
# endif

    /* If already loaded, good! */
    if (png_handle != NULL) return(1);

# if USE_LIBPNG == 2
    /* Try loading the DLL. */
    png_handle = dynld_module(fn, png_imports);
    if (png_handle == NULL) {
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_NOLIB), "PNG", fn);
	ui_msgbox(MBX_ERROR, temp);
	ERRLOG("PNG: unable to load '%s'; format disabled!\n", fn);
	return(0);
    } else
	INFO("PNG: module '%s' loaded.\n", fn);
# else
    png_handle = (void *)1;	/* just to indicate always therse */
# endif

    return(1);
}


/* No longer needed, unload DLL if needed. */
void
png_unload(void)
{
# if USE_LIBPNG == 2
    /* Unload the DLL if possible. */
    if (png_handle != NULL)
	dynld_close(png_handle);
# endif
    png_handle = NULL;
}


/* Write the given image as an 8-bit GrayScale file. */
int
png_write_gray(const wchar_t *fn, int inv, uint8_t *pix, int16_t w, int16_t h)
{
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep row;
    int16_t x, y;
    FILE *fp;

    /* Load the DLL if needed, give up if that fails. */
    if (! png_load()) return(0);

    /* Create the image file. */
    fp = plat_fopen(fn, L"wb");
    if (fp == NULL) {
	/* Yes, this looks weird. */
	if (fp == NULL)
		ERRLOG("PNG: file %ls could not be opened for writing!\n", fn);
	  else
error:
	ERRLOG("PNG: fatal error, bailing out, error = %i\n", errno);
	if (png != NULL)
		PNGFUNC(destroy_write_struct)(&png, &info);
	if (fp != NULL)
		(void)fclose(fp);
	return(0);
    }

    /* Initialize PNG stuff. */
    png = PNGFUNC(create_write_struct)(PNG_LIBPNG_VER_STRING, NULL,
				       error_handler, warning_handler);
    if (png == NULL) {
	ERRLOG("PNG: create_write_struct failed!\n");
	goto error;
    }

    info = PNGFUNC(create_info_struct)(png);
    if (info == NULL) {
	ERRLOG("PNG: create_info_struct failed!\n");
	goto error;
    }

# if USE_LIBPNG == 1
    /* Set up error handling. */
    if (setjmp(png_jmpbuf(png))) {
	/* If we get here, we had a problem writing the file */
	goto error;
    }
# endif

# if USE_CUSTOM_IO
    /*
     * We use our own I/O routines, because it seems that some of
     * the PNG DLL's out there are compiled in Debug mode, which
     * causes the 'FILE' definition to be different. This in turn
     * results in pretty bad crashes when trying to write..
     *
     * Using custom I/O routines, we avoid this issue.
     */
    PNGFUNC(set_write_fn)(png, (void *)fp, write_handler, flush_handler);
# else
    PNGFUNC(init_io)(png, fp);
# endif
    PNGFUNC(set_compression_level)(png, 9);

    /* Set other "zlib" parameters. */
    PNGFUNC(set_compression_mem_level)(png, 8);
    PNGFUNC(set_compression_strategy)(png, PNG_Z_DEFAULT_STRATEGY);
    PNGFUNC(set_compression_window_bits)(png, 15);
    PNGFUNC(set_compression_method)(png, 8);
    PNGFUNC(set_compression_buffer_size)(png, 8192);    

    PNGFUNC(set_IHDR)(png, info, w, h, 8, PNG_COLOR_TYPE_GRAY,
		      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

    PNGFUNC(write_info)(png, info);

    /* Create a buffer for one scanline of pixels. */
    row = (png_bytep)mem_alloc(PNGFUNC(get_rowbytes)(png, info));

    /* Process all scanlines in the image. */
    for (y = 0; y < h; y++) {
	for (x = 0; x < w; x++) {
                /* Copy the pixel data. */
		if (inv)
                	row[x] = 255 - pix[(y * w) + x];
		  else
                	row[x] = pix[(y * w) + x];
	}

	/* Write image to the file. */
	PNGFUNC(write_rows)(png, &row, 1);
    }

    /* No longer need the row buffer. */
    free(row);

    PNGFUNC(write_end)(png, NULL);

    PNGFUNC(destroy_write_struct)(&png, &info);

    /* Clean up. */
    (void)fclose(fp);

    return(1);
}


/* Write the given BITMAP-format image as an 8-bit RGBA file. */
int
png_write_rgb(const wchar_t *fn, int flip, uint8_t *pix, int16_t w, int16_t h)
{
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytepp rows;
    uint8_t *r, *b;
    uint32_t *rgb;
    int k, y, x;
    FILE *fp;

    /* Load the DLL if needed, give up if that fails. */
    if (! png_load()) return(0);

    /* Create the image file. */
    fp = plat_fopen(fn, L"wb");
    if (fp == NULL) {
	ERRLOG("PNG: File %ls could not be opened for writing!\n", fn);
error:
	if (png != NULL)
		PNGFUNC(destroy_write_struct)(&png, &info);
	if (fp != NULL)
		(void)fclose(fp);
	return(0);
    }

    /* Initialize PNG stuff. */
    png = PNGFUNC(create_write_struct)(PNG_LIBPNG_VER_STRING, NULL,
				       error_handler, warning_handler);
    if (png == NULL) {
	ERRLOG("PNG: create_write_struct failed!\n");
	goto error;
    }

    info = PNGFUNC(create_info_struct)(png);
    if (info == NULL) {
	ERRLOG("PNG: create_info_struct failed!\n");
	goto error;
    }

# if USE_CUSTOM_IO
    /*
     * We use our own I/O routines, because it seems that some of
     * the PNG DLL's out there are compiled in Debug mode, which
     * causes the 'FILE' definition to be different. This in turn
     * results in pretty bad crashes when trying to write..
     *
     * Using custom I/O routines, we avoid this issue.
     */
    PNGFUNC(set_write_fn)(png, (void *)fp, write_handler, flush_handler);
# else
    PNGFUNC(init_io)(png, fp);
# endif
    PNGFUNC(set_compression_level)(png, 9);

    /* Set other "zlib" parameters. */
    PNGFUNC(set_compression_mem_level)(png, 8);
    PNGFUNC(set_compression_strategy)(png, PNG_Z_DEFAULT_STRATEGY);
    PNGFUNC(set_compression_window_bits)(png, 15);
    PNGFUNC(set_compression_method)(png, 8);
    PNGFUNC(set_compression_buffer_size)(png, 8192);    

    PNGFUNC(set_IHDR)(png, info, w, h, 8, PNG_COLOR_TYPE_RGB,
		      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

    PNGFUNC(write_info)(png, info);

    /* Create a buffer for scanlines of pixels. */
    rows = (png_bytepp)mem_alloc(sizeof(png_bytep) * h);

    /* Process all scanlines in the image. */
    for (y = 0; y < h; y++) {
	/*
	 * If the bitmap is in 'bottom-up' mode, we have
	 * to 'flip' the image to the normal top-down mode.
	 */
	k = (flip) ? (h - 1) - y : y;

	/* Create a buffer for this scanline. */
	rows[k] = (png_bytep)mem_alloc(PNGFUNC(get_rowbytes)(png, info));

	/* Process all pixels on this line */
	r = rows[k];
	for (x = 0; x < w; x++) {
               	b = &pix[((y * w) + x) * 4];

		/* Transform if needed. */
		if (config.vid_grayscale || config.invert_display) {
			rgb = (uint32_t *)b;
			*rgb = video_color_transform(*rgb);
		}

		if (flip) {
			/* Convert all pixels to RGB mode. */
               		*r++ = b[2];
               		*r++ = b[1];
               		*r++ = b[0];
		} else {
               		/* Copy the pixel data. */
               		*r++ = b[0];
               		*r++ = b[1];
               		*r++ = b[2];
		}
	}
    }

    /* Write image to the file. */
    PNGFUNC(write_image)(png, rows);

    /* No longer need the row buffers. */
    for (y = 0; y < h; y++)
	free(rows[y]);
    free(rows);

    PNGFUNC(write_end)(png, NULL);

    PNGFUNC(destroy_write_struct)(&png, &info);

    /* Clean up. */
    (void)fclose(fp);

    return(1);
}


/* Write the given BITMAP-format image as an 8-bit color palette file. */
int
png_write_pal(const wchar_t *fn, uint8_t *pix, int16_t w, int16_t h, uint16_t pitch, RGB_PAL pal)
{
    png_color palette[256];
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytepp rows;
    FILE *fp;
    int i;

    /* Load the DLL if needed, give up if that fails. */
    if (! png_load()) return(0);

    /* Create the image file. */
    fp = plat_fopen(fn, L"wb");
    if (fp == NULL) {
	ERRLOG("PNG: File %ls could not be opened for writing!\n", fn);
error:
	if (png != NULL)
		PNGFUNC(destroy_write_struct)(&png, &info);
	if (fp != NULL)
		(void)fclose(fp);
	return(0);
    }

    /* Initialize PNG stuff. */
    png = PNGFUNC(create_write_struct)(PNG_LIBPNG_VER_STRING, NULL,
				       error_handler, warning_handler);
    if (png == NULL) {
	ERRLOG("PNG: create_write_struct failed!\n");
	goto error;
    }

    info = PNGFUNC(create_info_struct)(png);
    if (info == NULL) {
	ERRLOG("PNG: create_info_struct failed!\n");
	goto error;
    }

# if USE_CUSTOM_IO
    /*
     * We use our own I/O routines, because it seems that some of
     * the PNG DLL's out there are compiled in Debug mode, which
     * causes the 'FILE' definition to be different. This in turn
     * results in pretty bad crashes when trying to write..
     *
     * Using custom I/O routines, we avoid this issue.
     */
    PNGFUNC(set_write_fn)(png, (void *)fp, write_handler, flush_handler);
# else
    PNGFUNC(init_io)(png, fp);
# endif
    PNGFUNC(set_compression_level)(png, 9);

    /* Set other "zlib" parameters. */
    PNGFUNC(set_compression_mem_level)(png, 8);
    PNGFUNC(set_compression_strategy)(png, PNG_Z_DEFAULT_STRATEGY);
    PNGFUNC(set_compression_window_bits)(png, 15);
    PNGFUNC(set_compression_method)(png, 8);
    PNGFUNC(set_compression_buffer_size)(png, 8192);    

    PNGFUNC(set_IHDR)(png, info, w, h, 8, PNG_COLOR_TYPE_PALETTE,
		      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

    PNGFUNC(write_info)(png, info);

    /* Create the palette. */
    for (i = 0; i < 256; i++) {
	    palette[i].red = pal[i].r;
	    palette[i].green = pal[i].g;
	    palette[i].blue = pal[i].b;
    }
    PNGFUNC(set_PLTE)(png, info, palette, i);

    /* Create a buffer for scanlines of pixels. */
    rows = (png_bytep *)mem_alloc(sizeof(png_bytep) * h);
    for (i = 0; i < h; i++) {
	/* Create a buffer for this scanline. */
	rows[i] = (pix + (i * pitch));
    }
    PNGFUNC(set_rows)(png, info, rows);

    /* Write image to the file. */
    PNGFUNC(write_png)(png, info, 0, NULL);

    /* No longer need the row buffers. */
    free(rows);

    PNGFUNC(destroy_write_struct)(&png, &info);

    /* Clean up. */
    (void)fclose(fp);

    return(1);
}


#endif	/*USE_PNG*/
