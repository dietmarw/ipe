// ipebitmap_unix.cpp
// Code dependent on libjpeg and libpng
/*

    This file is part of the extensible drawing editor Ipe.
    Copyright (C) 1993-2016  Otfried Cheong

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, you have permission to link Ipe with the
    CGAL library and distribute executables, as long as you follow the
    requirements of the Gnu General Public License in regard to all of
    the software in the executable aside from CGAL.

    Ipe is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with Ipe; if not, you can find it at
    "http://www.gnu.org/copyleft/gpl.html", or write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ipebitmap.h"
#include "ipeutils.h"

#include <png.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#else
#include <csetjmp>
#include <jpeglib.h>
#endif

using namespace ipe;

// --------------------------------------------------------------------

#ifdef __APPLE__

bool dctDecode(Buffer dctData, Buffer pixelData, int components)
{
  CGDataProviderRef source =
    CGDataProviderCreateWithData(NULL, dctData.data(),
				 dctData.size(), NULL);
  CGImageRef bitmap =
    CGImageCreateWithJPEGDataProvider(source, NULL, false,
				      kCGRenderingIntentDefault);

  if (CGImageGetBitsPerComponent(bitmap) != 8)
    return false;

  int w = CGImageGetWidth(bitmap);
  int h = CGImageGetHeight(bitmap);
  int bits = CGImageGetBitsPerPixel(bitmap);
  int stride = CGImageGetBytesPerRow(bitmap);
  if (bits != 8 && bits != 24 && bits != 32)
    return false;
  int bytes = bits / 8;

  CGBitmapInfo info = CGImageGetBitmapInfo(bitmap);
  // TODO: check for alpha channel, float pixel values, and byte order?
  ipeDebug("dctDecode: %d x %d x %d, stride %d, info %x",
	   w, h, bytes, stride, info);

  // TODO: Is it necessary to copy the data?
  CFDataRef pixels = CGDataProviderCopyData(CGImageGetDataProvider(bitmap));
  const uchar *inRow = CFDataGetBytePtr(pixels);

  uchar *q = (uchar *) pixelData.data();
  if (bits !=  8) {
    for (int y = 0; y < h; ++y) {
      const uchar *p = inRow;
      for (int x = 0; x < w; ++x) {
	*q++ = p[0];
	*q++ = p[1];
	*q++ = p[2];
	p += bytes;
      }
      inRow += stride;
    }
  } else {
    for (int y = 0; y < h; ++y) {
      const uchar *p = inRow;
      for (int x = 0; x < w; ++x)
	*q++ = *p++;
      inRow += stride;
    }
  }
  CFRelease(pixels);
  CGImageRelease(bitmap);
  CGDataProviderRelease(source);
  return true;
}

#else

// Decode jpeg image using libjpeg API with error handling
// Code contributed by Michael Thon, 2015.

// The following is error-handling code for decompressing jpeg using the
// standard libjpeg API. Taken from the example.c and stackoverflow.
struct jpegErrorManager {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static char jpegLastErrorMsg[JMSG_LENGTH_MAX];

static void jpegErrorExit (j_common_ptr cinfo)
{
  jpegErrorManager *myerr = (jpegErrorManager*) cinfo->err;
  (*(cinfo->err->format_message)) (cinfo, jpegLastErrorMsg);
  longjmp(myerr->setjmp_buffer, 1);
}

bool dctDecode(Buffer dctData, Buffer pixelData, int components)
{
  struct jpeg_decompress_struct cinfo;

  // Error handling:
  struct jpegErrorManager jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpegErrorExit;
  if (setjmp(jerr.setjmp_buffer)) {
    ipeDebug("jpeg decompression failed: %s", jpegLastErrorMsg);
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  // Decompression:
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, (unsigned char *) dctData.data(), dctData.size());
  jpeg_read_header(&cinfo, 1);
  cinfo.out_color_space = ((components == 3) ? JCS_RGB : JCS_GRAYSCALE);
  jpeg_start_decompress(&cinfo);
  while (cinfo.output_scanline < cinfo.output_height) {
    int row_stride = cinfo.output_width * cinfo.output_components;
    int index = cinfo.output_scanline * row_stride;
    unsigned char *buffer[1];
    buffer[0] = (unsigned char *) &(pixelData[index]);
    jpeg_read_scanlines(&cinfo, buffer, 1);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  return true;
}
#endif

// --------------------------------------------------------------------

//! Read PNG image from file.
/*! Returns the image as a Bitmap.
  It will be compressed if \a deflate is set.
  Sets \a dotsPerInch if the image file contains a resolution,
  otherwise sets it to (0,0).
  If reading the file fails, returns a null Bitmap,
  and sets the error message \a errmsg.
*/
Bitmap Bitmap::readPNG(const char *fname, bool deflate,
		       Vector &dotsPerInch, const char * &errmsg)
{
  FILE *fp = Platform::fopen(fname, "rb");
  if (!fp) {
    errmsg = "Error opening file";
    return Bitmap();
  }

  static const char pngerr[] = "PNG library error";
  uchar header[8];
  if (fread(header, 1, 8, fp) != 8 ||
      png_sig_cmp(header, 0, 8)) {
    errmsg = "The file does not appear to be a PNG image";
    fclose(fp);
    return Bitmap();
  }

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
					       (png_voidp) NULL, NULL, NULL);
  if (!png_ptr) {
    errmsg = pngerr;
    fclose(fp);
    return Bitmap();
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
    errmsg = pngerr;
    return Bitmap();
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
    errmsg = pngerr;
    fclose(fp);
    return Bitmap();
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  int width = png_get_image_width(png_ptr, info_ptr);
  int height = png_get_image_height(png_ptr, info_ptr);
  int depth = png_get_bit_depth(png_ptr, info_ptr);
  int color_type = png_get_color_type(png_ptr, info_ptr);
  TColorSpace cs = EDeviceRGB;
  int samples = 3;
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    cs = EDeviceGray;
    samples = 1;
  }

  bool hasColorKey = false;
  int colorKey = 0;

  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_bytep trans_alpha = 0;
    int num_trans = 0;
    png_color_16p trans_color = 0;

    png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color);
    if (num_trans == 1 && trans_alpha[0] == 0) {
      // exactly one fully transparent color at index 0
      int num_palette = 0;
      png_colorp p = 0;
      png_get_PLTE(png_ptr, info_ptr, &p, &num_palette);
      if (num_palette > 0) {
	hasColorKey = true;
	colorKey = (p[0].red << 16) | (p[0].green << 8) | p[0].blue;
      }
    }

    png_set_palette_to_rgb(png_ptr);
    if (trans_alpha)
      png_set_strip_alpha(png_ptr);
  }

  if (color_type & PNG_COLOR_MASK_ALPHA)
    png_set_strip_alpha(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (depth == 16)
#if PNG_LIBPNG_VER >= 10504
    png_set_scale_16(png_ptr);
#else
  png_set_strip_16(png_ptr);
#endif
  png_read_update_info(png_ptr, info_ptr);

  depth = png_get_bit_depth(png_ptr, info_ptr);
  if (depth != 8) {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
    errmsg = "Depth of PNG image is not eight bits.";
    fclose(fp);
    return Bitmap();
  }

  const double mpi = 25.4/1000.0;
  dotsPerInch = Vector(mpi * png_get_x_pixels_per_meter(png_ptr, info_ptr),
		       mpi * png_get_y_pixels_per_meter(png_ptr, info_ptr));

  color_type = png_get_color_type(png_ptr, info_ptr);
  ipeDebug("Depth %d, color type %d, samples %d", depth, color_type, samples);

  // get data
  Buffer pixels(width * height * samples);
  png_bytep row[height];
  for (int y = 0; y < height; ++y)
    row[y] = (png_bytep) pixels.data() + width * samples * y;
  png_read_image(png_ptr, row);

  png_read_end(png_ptr, (png_infop) NULL);
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
  fclose(fp);
  Bitmap bm(width, height, cs, depth, pixels, Bitmap::EDirect, deflate);
  if (hasColorKey)
    bm.setColorKey(colorKey);
  return bm;
}

// --------------------------------------------------------------------
