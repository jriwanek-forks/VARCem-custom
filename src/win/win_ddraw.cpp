/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Rendering module for Microsoft DirectDraw 9.
 *
 * Version:	@(#)win_ddraw.cpp	1.0.24	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#define UNICODE
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/device.h>
#include <86box/ui.h>
#include <86box/plat.h>
#ifdef USE_LIBPNG
# include <86box/png.h>
#endif
#ifdef _MSC_VER
# pragma warning(disable: 4200)
#endif
#include <86box/video.h>
#include <86box/win.h>


static LPDIRECTDRAW4		lpdd4 = NULL;
static LPDIRECTDRAWSURFACE4	lpdds_pri = NULL,
				lpdds_back = NULL,
				lpdds_back2 = NULL;
static LPDIRECTDRAWCLIPPER	lpdd_clipper = NULL;
static HWND			ddraw_hwnd;
static int			ddraw_w, ddraw_h,
				xs, ys, ys2;
static int			is_enabled;


static const char *
GetError(HRESULT hr)
{
    const char *err = "Unknown";

    switch(hr) {
	case DDERR_INCOMPATIBLEPRIMARY:
		err = "Incompatible Primary";
		break;

	case DDERR_INVALIDCAPS:
		err = "Invalid Caps";
		break;

	case DDERR_INVALIDOBJECT:
		err = "Invalid Object";
		break;

	case DDERR_INVALIDPARAMS:
		err = "Invalid Parameters";
		break;

	case DDERR_INVALIDPIXELFORMAT:
		err = "Invalid Pixel Format";
		break;

	case DDERR_NOALPHAHW:
		err = "Hardware does not support Alpha";
		break;

	case DDERR_NOCOOPERATIVELEVELSET:
		err = "No cooperative level set";
		break;

	case DDERR_NODIRECTDRAWHW:
		err = "Hardware does not support DirectDraw";
		break;

	case DDERR_NOEMULATION:
		err = "No emulation";
		break;

	case DDERR_NOEXCLUSIVEMODE:
		err = "No exclusive mode available";
		break;

	case DDERR_NOFLIPHW:
		err = "Hardware does not support flipping";
		break;

	case DDERR_NOMIPMAPHW:
		err = "Hardware does not support MipMap";
		break;

	case DDERR_NOOVERLAYHW:
		err = "Hardware does not support overlays";
		break;

	case DDERR_NOZBUFFERHW:
		err = "Hardware does not support Z buffers";
		break;

	case DDERR_OUTOFMEMORY:
		err = "Out of memory";
		break;

	case DDERR_OUTOFVIDEOMEMORY:
		err = "Out of video memory";
		break;

	case DDERR_PRIMARYSURFACEALREADYEXISTS:
		err = "Primary Surface already exists";
		break;

	case DDERR_UNSUPPORTEDMODE:
		err = "Mode not supported";
		break;

	default:
		break;
    }

    return(err);
}


static void
ddraw_fs_size_default(RECT w_rect, RECT *r_dest)
{
    r_dest->left   = 0;
    r_dest->top    = 0;
    r_dest->right  = (w_rect.right  - w_rect.left) - 1;
    r_dest->bottom = (w_rect.bottom - w_rect.top) - 1;
}


static void
ddraw_fs_size(RECT w_rect, RECT *r_dest, int w, int h)
{
    int ratio_w, ratio_h;
    double hsr, gsr, d, sh, sw, wh, ww, mh, mw;

    sh = (double)(w_rect.bottom - w_rect.top);
    sw = (double)(w_rect.right - w_rect.left);
    wh = (double)h;
    ww = (double)w;

    switch (config.vid_fullscreen_scale) {
	case FULLSCR_SCALE_FULL:
		ddraw_fs_size_default(w_rect, r_dest);
		break;

	case FULLSCR_SCALE_43:
	case FULLSCR_SCALE_KEEPRATIO:
		if (config.vid_fullscreen_scale == FULLSCR_SCALE_43) {
			mw = 4.0;
			mh = 3.0;
		} else {
			mw = ww;
			mh = wh;
		}

		hsr = sw / sh;
		gsr = mw / mh;

 		if (hsr > gsr) {
 			/* Host ratio is bigger than guest ratio. */
			d = (sw - (mw * (sh / mh))) / 2.0;
 
			r_dest->left   = (int) d;
			r_dest->right  = (int) (sw - d - 1.0);
 			r_dest->top    = 0;
			r_dest->bottom = (int) (sh - 1.0);
 		} else if (hsr < gsr) {
 			/* Host ratio is smaller or rqual than guest ratio. */
			d = (sh - (mh * (sw / mw))) / 2.0;
 
 			r_dest->left   = 0;
			r_dest->right  = (int) (sw - 1.0);
			r_dest->top    = (int) d;
			r_dest->bottom = (int) (sh - d - 1.0);
 		} else {
 			/* Host ratio is equal to guest ratio. */
 			ddraw_fs_size_default(w_rect, r_dest);
 		}
 		break;

	case FULLSCR_SCALE_INT:
		ratio_w = (w_rect.right  - w_rect.left) / w;
		ratio_h = (w_rect.bottom - w_rect.top)  / h;
		if (ratio_h < ratio_w)
			ratio_w = ratio_h;
		r_dest->left   = ((w_rect.right  - w_rect.left) / 2) - ((w * ratio_w) / 2);
		r_dest->right  = ((w_rect.right  - w_rect.left) / 2) + ((w * ratio_w) / 2) - 1;
		r_dest->top    = ((w_rect.bottom - w_rect.top)  / 2) - ((h * ratio_w) / 2);
		r_dest->bottom = ((w_rect.bottom - w_rect.top)  / 2) + ((h * ratio_w) / 2) - 1;
		break;
    }
}


static void
ddraw_blit_fs(bitmap_t *scr, int x, int y, int y1, int y2, int w, int h)
{
    DDSURFACEDESC2 ddsd;
    RECT r_src, r_dest, w_rect;
    DDBLTFX ddbltfx;
    HRESULT hr;
    int yy;

    if (! is_enabled) {
	video_blit_done();
	return;
    }

    if ((lpdds_back == NULL) || (y1 == y2) || (h <= 0)) {
	video_blit_done();
	return;
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    hr = lpdds_back->Lock(NULL, &ddsd,
			  DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back->Restore();
	lpdds_back->Lock(NULL, &ddsd,
			 DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
	device_force_redraw();
    }
    if (! ddsd.lpSurface) {
	video_blit_done();
	return;
    }

    for (yy = y1; yy < y2; yy++) {
	if (scr) {
		if (config.vid_grayscale || config.invert_display)
			video_transform_copy((uint32_t *)((uintptr_t)ddsd.lpSurface + (yy * ddsd.lPitch)), &scr->line[y + yy][x], w);
		else
			memcpy((void *)((uintptr_t)ddsd.lpSurface + (yy * ddsd.lPitch)), &scr->line[y + yy][x], w * 4);
	}
    }

    video_blit_done();

    lpdds_back->Unlock(NULL);

    w_rect.left = 0;
    w_rect.top = 0;
    w_rect.right = ddraw_w;
    w_rect.bottom = ddraw_h;
    ddraw_fs_size(w_rect, &r_dest, w, h);

    r_src.left   = 0;
    r_src.top    = 0;       
    r_src.right  = w;
    r_src.bottom = h;

    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwFillColor = 0;

    lpdds_back2->Blt(&w_rect, NULL, NULL,
		     DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);

    hr = lpdds_back2->Blt(&r_dest, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back2->Restore();
	lpdds_back2->Blt(&r_dest, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    }
	
    hr = lpdds_pri->Flip(NULL, DDFLIP_NOVSYNC);	
    if (hr == DDERR_SURFACELOST) {
	lpdds_pri->Restore();
	lpdds_pri->Flip(NULL, DDFLIP_NOVSYNC);
    }
}


static void
ddraw_blit(bitmap_t *scr, int x, int y, int y1, int y2, int w, int h)
{
    DDSURFACEDESC2 ddsd;
    RECT r_src, r_dest;
    HRESULT hr;
    POINT po;
    int yy;

    if (! is_enabled) {
	video_blit_done();
	return;
    }

    if ((lpdds_back == NULL) || (y1 == y2) || (h <= 0)) {
	video_blit_done();
	return;
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    hr = lpdds_back->Lock(NULL, &ddsd,
			  DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back->Restore();
	lpdds_back->Lock(NULL, &ddsd,
			 DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
	device_force_redraw();
    }

    if (! ddsd.lpSurface) {
	video_blit_done();
	return;
    }

    for (yy = y1; yy < y2; yy++) {
	if (scr) {
		if ((y + yy) >= 0 && (y + yy) < scr->h) {
			if (config.vid_grayscale || config.invert_display)
				video_transform_copy((uint32_t *) &(((uint8_t *) ddsd.lpSurface)[yy * ddsd.lPitch]), &scr->line[y + yy][x], w);
			else
				memcpy((uint32_t *) &(((uint8_t *) ddsd.lpSurface)[yy * ddsd.lPitch]), &scr->line[y + yy][x], w * 4);
		}
	}
    }

    video_blit_done();

    lpdds_back->Unlock(NULL);

    po.x = po.y = 0;
	
    ClientToScreen(ddraw_hwnd, &po);
    GetClientRect(ddraw_hwnd, &r_dest);
    OffsetRect(&r_dest, po.x, po.y);	
	
    r_src.left   = 0;
    r_src.top    = 0;       
    r_src.right  = w;
    r_src.bottom = h;

    hr = lpdds_back2->Blt(&r_src, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back2->Restore();
	lpdds_back2->Blt(&r_src, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    }

    lpdds_back2->Unlock(NULL);
	
    hr = lpdds_pri->Blt(&r_dest, lpdds_back2, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_pri->Restore();
	lpdds_pri->Blt(&r_dest, lpdds_back2, &r_src, DDBLT_WAIT, NULL);
    }
}


static void
ddraw_close(void)
{
    DEBUG("DDRAW: close\n");

    video_blit_set(NULL);

    if (lpdds_back2 != NULL) {
	lpdds_back2->Release();
	lpdds_back2 = NULL;
    }
    if (lpdds_back != NULL) {
	lpdds_back->Release();
	lpdds_back = NULL;
    }
    if (lpdds_pri != NULL) {
	lpdds_pri->Release();
	lpdds_pri = NULL;
    }
    if (lpdd_clipper != NULL) {
	lpdd_clipper->Release();
	lpdd_clipper = NULL;
    }
    if (lpdd4 != NULL) {
	lpdd4->Release();
	lpdd4 = NULL;
    }

    is_enabled = 0;
}


static int
ddraw_init(int fs)
{
    DDSURFACEDESC2 ddsd;
    LPDIRECTDRAW lpdd;
    HRESULT hr;
    DWORD dw;
    HWND h;

    INFO("DDraw: initializing (fs=%d)\n", fs);

    hr = DirectDrawCreate(NULL, &lpdd, NULL);
    if (FAILED(hr)) {
	ERRLOG("DDRAW: cannot create an instance (%s)\n", GetError(hr));
	return(0);
    }
    hr = lpdd->QueryInterface(IID_IDirectDraw4, (LPVOID *)&lpdd4);
    if (FAILED(hr)) {
	ERRLOG("DDRAW: no interfaces found (%s)\n", GetError(hr));
	return(0);
    }
    lpdd->Release();

    atexit(ddraw_close);

    if (fs) {
	dw = DDSCL_SETFOCUSWINDOW | DDSCL_CREATEDEVICEWINDOW | \
	     DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT;
	h = hwndMain;
    } else {
	dw = DDSCL_NORMAL;
	h = hwndRender;
    }
    hr = lpdd4->SetCooperativeLevel(h, dw);
    if (FAILED(hr)) {
	ERRLOG("DDRAW: SetCooperativeLevel failed (%s)\n", GetError(hr));
	return(0);
    }

    if (fs) {
	ddraw_w = GetSystemMetrics(SM_CXSCREEN);
	ddraw_h = GetSystemMetrics(SM_CYSCREEN);
	hr = lpdd4->SetDisplayMode(ddraw_w, ddraw_h, 32, 0, 0);
	if (FAILED(hr)) {
		ERRLOG("DDRAW: SetDisplayMode failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (fs) {
	ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps |= (DDSCAPS_COMPLEX | DDSCAPS_FLIP);
	ddsd.dwBackBufferCount = 1;
    }
    hr = lpdd4->CreateSurface(&ddsd, &lpdds_pri, NULL);
    if (FAILED(hr)) {
	ERRLOG("DDRAW: CreateSurface failed (%s)\n", GetError(hr));
	return(0);
    }

    if (fs) {
	memset(&ddsd, 0x00, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
	hr = lpdds_pri->GetAttachedSurface(&ddsd.ddsCaps, &lpdds_back2);
	if (FAILED(hr)) {
		ERRLOG("DDRAW: GetAttachedSurface failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    hr = lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL);
    if (FAILED(hr)) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	hr = lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL);
	if (FAILED(hr)) {
		ERRLOG("DDRAW: CreateSurface(back) failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    if (! fs) {
	memset(&ddsd, 0x00, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	hr = lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL);
	if (FAILED(hr)) {
		ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
		ddsd.dwWidth = 2048;
		ddsd.dwHeight = 2048;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		hr = lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL);
		if (FAILED(hr)) {
			ERRLOG("DDRAW: CreateSurface(back2) failed (%s)\n", GetError(hr));
			return(0);
		}
	}

	hr = lpdd4->CreateClipper(0, &lpdd_clipper, NULL);
	if (FAILED(hr)) {
		ERRLOG("DDRAW: CreateClipper failed (%s)\n", GetError(hr));
		return(0);
	}

	hr = lpdd_clipper->SetHWnd(0, h);
	if (FAILED(hr)) {
		ERRLOG("DDRAW: SetHWnd failed (%s)\n", GetError(hr));
		return(0);
	}

	hr = lpdds_pri->SetClipper(lpdd_clipper);
	if (FAILED(hr)) {
		ERRLOG("DDRAW: SetClipper failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    ddraw_hwnd = h;

    if (fs)
	video_blit_set(ddraw_blit_fs);
      else
	video_blit_set(ddraw_blit);

    is_enabled = 1;

    return(1);
}


static void
ddraw_enable(int yes)
{
    is_enabled = yes;
}


static int
SaveBMP(const wchar_t *fn, BITMAPINFO *bmi, uint8_t *pixels)
{
    BITMAPFILEHEADER bmpHdr; 
    FILE *fp;

    if ((fp = plat_fopen(fn, L"wb")) == NULL) {
	ERRLOG("[SaveBMP] File %ls could not be opened for writing!\n", fn);
	return(0);
    } 

    memset(&bmpHdr, 0x00, sizeof(BITMAPFILEHEADER));
    bmpHdr.bfSize = sizeof(BITMAPFILEHEADER) + \
		    sizeof(BITMAPINFOHEADER) + bmi->bmiHeader.biSizeImage;
    bmpHdr.bfType = 0x4D42;
    bmpHdr.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); 

    (void)fwrite(&bmpHdr, sizeof(BITMAPFILEHEADER), 1, fp);
    (void)fwrite(&bmi->bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
    (void)fwrite(pixels, bmi->bmiHeader.biSizeImage, 1, fp); 

    /* Clean up. */
    (void)fclose(fp);

    return(1);
}


static HBITMAP
CopySurface(IDirectDrawSurface4 *pDDSurface)
{ 
    HBITMAP hBmp, hOldBmp;
    DDSURFACEDESC2 ddsd;
    HDC hDC, hMemDC;

    pDDSurface->GetDC(&hDC);
    hMemDC = CreateCompatibleDC(hDC); 
    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    pDDSurface->GetSurfaceDesc(&ddsd);
    hBmp = CreateCompatibleBitmap(hDC, xs, ys);
    hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);
    BitBlt(hMemDC, 0, 0, xs, ys, hDC, 0, 0, SRCCOPY);    
    SelectObject(hMemDC, hOldBmp); 
    DeleteDC(hMemDC);
    pDDSurface->ReleaseDC(hDC);

    return(hBmp);
}


static void
ddraw_screenshot(const wchar_t *fn)
{
    wchar_t path[512];
    wchar_t temp[512];
    BITMAPINFO bmi;
    uint8_t *pixels;
    uint8_t *pix2;
    HBITMAP hBmp;
    HDC hDC;
    int i;

#if 0
    xs = xsize;
    ys = ys2 = ysize;

    /* For EGA/(S)VGA, the size is NOT adjusted for overscan. */
    if ((overscan_y > 16) && enable_overscan) {
	xs += overscan_x;
	ys += overscan_y;
    }

    /* For CGA, the width is adjusted for overscan, but the height is not. */
    if (overscan_y == 16) {
	if (ys2 <= 250)
		ys += (overscan_y >> 1);
	  else
		ys += overscan_y;
    }
#endif

    get_screen_size_natural(&xs, &ys);

    if (ysize <= 250) {
	ys >>= 1;
	ys2 >>= 1;
    }

    /* Create a surface copy and store it as a bitmap. */
    hBmp = CopySurface(lpdds_back);

    /* Create a compatible DC. */
    hDC = GetDC(NULL);

    /* Request the size info from the bitmap. */
    memset(&bmi, 0x00, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    GetDIBits(hDC, hBmp, 0, 0, NULL, &bmi, DIB_RGB_COLORS);
    if (bmi.bmiHeader.biSizeImage <= 0) {
	bmi.bmiHeader.biSizeImage = bmi.bmiHeader.biWidth * abs(bmi.bmiHeader.biHeight) * (bmi.bmiHeader.biBitCount + 7) / 8;
    }

    /* Allocate a buffer for the pixel data. */
    if ((pixels = (uint8_t *)mem_alloc(bmi.bmiHeader.biSizeImage)) == NULL) {
	ERRLOG("DDraw: unable to allocate bitmap memory!\n");
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	ReleaseDC(NULL, hDC); 
	return;
    }

    /* Now get the actual pixel data from the bitmap. */
    bmi.bmiHeader.biCompression = BI_RGB;
    GetDIBits(hDC, hBmp, 0, bmi.bmiHeader.biHeight,
	      pixels, &bmi, DIB_RGB_COLORS);

    /* No longer need the DC. */
    ReleaseDC(NULL, hDC); 

    /*
     * For some CGA modes (320x200, 640x200 etc) we double-up
     * the image height so it looks a little better. We simply
     * copy each real scanline.
     */
    if (ys <= 250) {
	/* Save current buffer. */
	pix2 = pixels;

	/* Update bitmap image size. */
	bmi.bmiHeader.biHeight <<= 1;
	bmi.bmiHeader.biSizeImage <<= 1;

	/* Allocate new buffer, doubled-up. */
	pixels = (uint8_t *)mem_alloc(bmi.bmiHeader.biSizeImage);

	/* Copy scanlines. */
	for (i = 0; i < ys; i++) {
		/* Copy original line. */
		memcpy(pixels + (i * xs * 8),
		       pix2 + (i * xs * 4), xs * 4);

		/* And copy it once more, doubling it. */
		memcpy(pixels + ((i * xs * 8) + (xs * 4)),
		       pix2 + (i * xs * 4), xs * 4);
	}

	/* No longer need original buffer. */
	free(pix2);
    }

    /* Save filename. */
    wcscpy(path, fn);

#ifdef USE_LIBPNG
    /* Save the screenshot, using PNG if available. */
    i = png_write_rgb(path, 1, pixels,
		      (int16_t)bmi.bmiHeader.biWidth,
		      (int16_t)abs(bmi.bmiHeader.biHeight));
    if (i == 0) {
#endif
	/* Use BMP, so fix the file name. */
	path[wcslen(path)-3] = L'b';
	path[wcslen(path)-2] = L'm';
	path[wcslen(path)-1] = L'p';
	i = SaveBMP(path, &bmi, pixels);
#ifdef USE_LIBPNG
    }
#endif

    /* Release pixel buffer. */
    free(pixels);

    /* Show error message if needed. */
    if (i == 0) {
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_SCRSHOT), path);
	ui_msgbox(MBX_ERROR, temp);
    }
}


const vidapi_t ddraw_vidapi = {
    "ddraw",
    "DirectDraw 9+",
    1,
    ddraw_init, ddraw_close, NULL,
    NULL,
    NULL,
    ddraw_enable,
    ddraw_screenshot,
    NULL
};
