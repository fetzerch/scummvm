/* ScummVM - Scumm Interpreter
 * Copyright (C) 2001  Ludvig Strigeus
 * Copyright (C) 2001-2004 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */
#include "stdafx.h"
#include "palm.h"

#include "common/scaler.h"
#include "common/util.h"
#include "common/config-manager.h"

#include <BmpGlue.h>
#include "start.h"	// appFileCreator
#include "globals.h"

#ifndef DISABLE_TAPWAVE
// Tapwave code will come here
#endif

enum {
	ftrBufferOverlay	= 1000,
	ftrBufferBackup,
	ftrBufferHotSwap
};

static const OSystem::GraphicsMode s_supportedGraphicsModes[] = {
	{"normal", "Normal (no scaling)", GFX_NORMAL},
	{"flipping", "Page Flipping", GFX_FLIPPING},
	{"buffered", "Buffered", GFX_BUFFERED},
	{"wide", "Wide (HiRes+ only)", GFX_WIDE},
	{0, 0, 0}
};

int OSystem_PALMOS::getDefaultGraphicsMode() const {
	return GFX_NORMAL;
}

const OSystem::GraphicsMode *OSystem_PALMOS::getSupportedGraphicsModes() const {
	return s_supportedGraphicsModes;
}

int OSystem_PALMOS::getGraphicsMode() const {
	return _mode;
}

bool OSystem_PALMOS::setGraphicsMode(int mode) {
	switch(mode) {
	case GFX_NORMAL:
	case GFX_FLIPPING:
	case GFX_BUFFERED:
	case GFX_WIDE:
		_setMode = mode;
		break;
	
	default:
		warning("unknown gfx mode %d", mode);
		_setMode = GFX_NORMAL;
		return false;
	}

	return true;
}

void OSystem_PALMOS::initSize(uint w, uint h) {
	_screenWidth	= w;
	_screenHeight	= h;
	_offScreenPitch	= gVars->screenPitch;	// direct screen / flipping use this, reset later if buffered
	_screenPitch	= gVars->screenPitch;

	_overlayVisible = false;
	_quitCount = 0;

	// 640x480 only on Zodiac and in GFX_WIDE mode
	if (h == 480)
		if (!(_mode == GFX_WIDE && OPTIONS_TST(kOptDeviceZodiac)))
			error("640x480 game can only be run on Zodiac in wide mode.");

	// lock the graffiti sizer
	if (gVars->slkRefNum != sysInvalidRefNum) {
		if (gVars->slkVersion == vskVersionNum1)
			SilkLibDisableResize(gVars->slkRefNum);
		else
			VskSetState(gVars->slkRefNum, vskStateEnable, vskResizeDisable);

	// Tapwave Zodiac and other DIA compatible devices
	} else if (OPTIONS_TST(kOptModeWide)) {
		PINSetInputTriggerState(pinInputTriggerDisabled);
	}

	// don't allow orientation change
	if (OPTIONS_TST(kOptCollapsible))
		SysSetOrientationTriggerState(sysOrientationTriggerDisabled);

	set_mouse_pos(200,150);
	
	unload_gfx_mode();
	_mode		= _setMode;
	_initMode	= _mode;
	load_gfx_mode();
}

int16 OSystem_PALMOS::getHeight() {
	return _screenHeight;
}

int16 OSystem_PALMOS::getWidth() {
	return _screenWidth;
}

static void HotSwap16bitMode(Boolean swap) {
	UInt32 width = hrWidth;
	UInt32 height= hrHeight;
	UInt32 depth = swap ? 16 : 8;
	Boolean color = true;

	if (OPTIONS_TST(kOptMode16Bit)) {
		WinScreenMode(winScreenModeSet, &width, &height, &depth, &color);
		ClearScreen();
		OPTIONS_SET(kOptDisableOnScrDisp);
	}
}

void OSystem_PALMOS::load_gfx_mode() {
	Err e;

	if (!_modeChanged) {
		// get command line config
		_fullscreen = (ConfMan.getBool("fullscreen") && OPTIONS_TST(kOptModeWide));
		_adjustAspectRatio = ConfMan.getBool("aspect_ratio");

		// get the actual palette
		WinPalette(winPaletteGet, 0, 256, _currentPalette);
	
		// set only if _mode not changed
		const byte startupPalette[] = {
			0	,0	,0	,0,
			0	,0	,171,0,
			0	,171, 0	,0,
			0	,171,171,0,
			171	,0	,0	,0, 
			171	,0	,171,0,
			171	,87	,0	,0,
			171	,171,171,0,
			87	,87	,87	,0,
			87	,87	,255,0,
			87	,255,87	,0,
			87	,255,255,0,
			255	,87	,87	,0,
			255	,87	,255,0,
			255	,255,87	,0,
			255	,255,255,0
		};

		// palette for preload dialog
		setPalette(startupPalette, 0, 16);
	}

	// check HiRes+
	if (_mode == GFX_WIDE) {
		if (OPTIONS_TST(kOptModeWide)) {
			Boolean std = true;

#ifndef DISABLE_TAPWAVE
// Tapwave code will come here
#endif
			if (std) {
				// only for 320x200 games
				if (!(_screenWidth == 320 && _screenHeight == 200)) {
					warning("Wide display not avalaible for this game, switching to GFX_NORMAL mode.");
					_mode = GFX_NORMAL;
				}
			}

		} else {
			warning("HiRes+ not avalaible on this device, switching to GFX_NORMAL mode.");
			_mode = GFX_NORMAL;
		}
	}

	if (_fullscreen || _mode == GFX_WIDE) {
		// Sony wide
		if (gVars->slkRefNum != sysInvalidRefNum) {
			if (gVars->slkVersion == vskVersionNum1) {
				SilkLibEnableResize (gVars->slkRefNum);
				SilkLibResizeDispWin(gVars->slkRefNum, silkResizeMax);
				SilkLibDisableResize(gVars->slkRefNum);
			} else {
				VskSetState(gVars->slkRefNum, vskStateEnable, (gVars->slkVersion != vskVersionNum3 ? vskResizeVertically : vskResizeHorizontally));
				VskSetState(gVars->slkRefNum, vskStateResize, vskResizeNone);
				VskSetState(gVars->slkRefNum, vskStateEnable, vskResizeDisable);
			}

		// Tapwave Zodiac and other DIA compatible devices
		} else if (OPTIONS_TST(kOptModeWide)) {
			PINSetInputAreaState(pinInputAreaClosed);
			StatHide();
		}
	}

	if (_modeChanged) {
		WinSetBackColor(RGBToColor(0,0,0));
		WinEraseWindow();
	}

	if (_mode == GFX_WIDE) {
		OPTIONS_SET(kOptDisableOnScrDisp);
		_fullscreen = false;
		_wide = true;

		if (OPTIONS_TST(kOptDeviceZodiac)) {
			// landscape
			_screenOffset.x = 0;
			_screenOffset.y = (_adjustAspectRatio) ? 10 : 0;
			_screenOffset.addr = _screenOffset.y * _screenPitch;

		// others only for 320x200
		} else {
			_wide = false;

			_screenOffset.x = 0;
			_screenOffset.y = 10;
			_screenOffset.addr = (OPTIONS_TST(kOptModeLandscape) ? _screenOffset.y : _screenOffset.x) * _screenPitch;
		}

	} else {
		OPTIONS_RST(kOptDisableOnScrDisp);

		_screenOffset.x = ((_fullscreen ? gVars->screenFullWidth : gVars->screenWidth) - _screenWidth) >> 1;
		_screenOffset.y = ((_fullscreen ? gVars->screenFullHeight : gVars->screenHeight) - _screenHeight) >> 1;
		_screenOffset.addr = _screenOffset.x + _screenOffset.y * _screenPitch;
	}

	if (OPTIONS_TST(kOptModeHiDensity))
		WinSetCoordinateSystem(kCoordinatesNative);

	// init screens
	switch(_mode) {
		case GFX_FLIPPING:
			gVars->screenLocked = true;
			_offScreenP	= WinScreenLock(winLockErase) + _screenOffset.addr;
			_screenP	= _offScreenP;
			_offScreenH	= WinGetDisplayWindow();
			_screenH	= _offScreenH;	
			_renderer_proc = &OSystem_PALMOS::updateScreen__flipping;
			break;
		case GFX_WIDE:
		case GFX_BUFFERED:
			_screenH	= WinGetDisplayWindow();
			_offScreenH	= WinCreateOffscreenWindow(_screenWidth, _screenHeight, nativeFormat, &e);
			_offScreenP	= (byte *)(BmpGetBits(WinGetBitmap(_offScreenH)));

			if (_mode == GFX_WIDE) {
#ifndef DISABLE_TAPWAVE
// Tapwave code will come here
#endif			
				{
					gVars->screenLocked = true;
					_screenP = WinScreenLock(winLockErase) + _screenOffset.addr;
					
					if (OPTIONS_TST(kOptDeviceARM))
						_arm[PNO_WIDE].pnoPtr = _PnoInit((OPTIONS_TST(kOptModeLandscape) ? ARM_OWIDELS : ARM_OWIDEPT), &_arm[PNO_WIDE].pnoDesc);
					
					_renderer_proc = (OPTIONS_TST(kOptModeLandscape)) ?
						&OSystem_PALMOS::updateScreen__wide_landscape :
						&OSystem_PALMOS::updateScreen__wide_portrait;
				}

			} else {
				_screenP = (byte *)(BmpGetBits(WinGetBitmap(_screenH))) + _screenOffset.addr;
				_renderer_proc =  &OSystem_PALMOS::updateScreen__buffered;
			}
			_offScreenPitch = _screenWidth;
			break;

		case GFX_NORMAL:
		default:
			_offScreenH	= WinGetDisplayWindow();
			_screenH = _offScreenH;
			_offScreenP	= (byte *)(BmpGetBits(WinGetBitmap(_offScreenH))) + _screenOffset.addr;
			_screenP	= _offScreenP;
			_renderer_proc =  &OSystem_PALMOS::updateScreen__direct;
			break;
	}

	if (!_modeChanged) {
		// try to allocate on storage heap, TODO : error if failed
		FtrPtrNew(appFileCreator, ftrBufferOverlay, _screenWidth * _screenHeight, (void **)&_tmpScreenP);
		FtrPtrNew(appFileCreator, ftrBufferBackup, _screenWidth * _screenHeight, (void **)&_tmpBackupP);
		// only if wide mode avalaible
		if OPTIONS_TST(kOptModeWide)
			FtrPtrNew(appFileCreator, ftrBufferHotSwap, _screenWidth * _screenHeight, (void **)&_tmpHotSwapP);

		_gfxLoaded = true;
	}
	
}

void OSystem_PALMOS::unload_gfx_mode() {
	if (!_gfxLoaded)
		return;

	WinSetDrawWindow(WinGetDisplayWindow());

	if (OPTIONS_TST(kOptModeHiDensity))
		WinSetCoordinateSystem(kCoordinatesStandard);

	switch (_mode) {
		case GFX_FLIPPING:
			WinScreenUnlock();
			gVars->screenLocked = false;
			break;

		case GFX_WIDE:
#ifndef DISABLE_TAPWAVE
// Tapwave code will come here
#endif
			{
				WinScreenUnlock();
				gVars->screenLocked = false;

				if (OPTIONS_TST(kOptDeviceARM) && _arm[PNO_WIDE].pnoPtr)
					_PnoFree(&_arm[PNO_WIDE].pnoDesc, _arm[PNO_WIDE].pnoPtr);
			}
				// continue to GFX_BUFFERED

		case GFX_BUFFERED:
			WinDeleteWindow(_offScreenH, false);
			break;
	}

	// restore silkarea
	// -- Sony wide
	if (gVars->slkRefNum != sysInvalidRefNum) {
		if (gVars->slkVersion == vskVersionNum1) {
			SilkLibEnableResize (gVars->slkRefNum);
			SilkLibResizeDispWin(gVars->slkRefNum, silkResizeNormal);
			SilkLibDisableResize(gVars->slkRefNum);
		} else {
			VskSetState(gVars->slkRefNum, vskStateEnable, (gVars->slkVersion != vskVersionNum3 ? vskResizeVertically : vskResizeHorizontally));
			VskSetState(gVars->slkRefNum, vskStateResize, vskResizeMax);
			VskSetState(gVars->slkRefNum, vskStateEnable, vskResizeDisable);
		}

	// -- Tapwave Zodiac and other DIA compatible devices
	} else if (OPTIONS_TST(kOptModeWide)) {
		StatShow();
		PINSetInputAreaState(pinInputAreaOpen);
	}

	if (!_modeChanged) {
		// free only if _mode not changed
		if (_tmpScreenP)
			FtrPtrFree(appFileCreator, ftrBufferOverlay);

		if (_tmpBackupP)
			FtrPtrFree(appFileCreator, ftrBufferBackup);

		if OPTIONS_TST(kOptModeWide)
			if (_tmpHotSwapP)
				FtrPtrFree(appFileCreator, ftrBufferHotSwap);
	}
}

void OSystem_PALMOS::hotswap_gfx_mode(int mode) {
	// save current offscreen
	byte *src = _offScreenP;
	byte *dst = _tmpHotSwapP;
	int h = _screenHeight;

	UInt32 offset = 0;
	do {
		DmWrite(dst, offset, src, _screenWidth);
		offset += _screenWidth;
		src += _offScreenPitch;
	} while (--h);	
	_modeChanged = true;
	
	// reset offscreen pitch
	_offScreenPitch	= gVars->screenPitch;

	// free old mode memory
	unload_gfx_mode();
	
	// load new gfx mode
	_mode = mode;
	load_gfx_mode();
	
	// restore offscreen
	copyRectToScreen(_tmpHotSwapP, _screenWidth, 0, 0, _screenWidth, _screenHeight);
	_modeChanged = false;
	
	// force palette update
	_paletteDirtyStart = 0;
	_paletteDirtyEnd = 256;

	updateScreen();
}


// TODO : move GFX functions here

void OSystem_PALMOS::setPalette(const byte *colors, uint start, uint num) {
	if (_quitCount)
		return;

	const byte *b = colors;
	uint i;
	RGBColorType *base = _currentPalette + start;
	for(i=0; i < num; i++) {
		base[i].r = b[0];
		base[i].g = b[1];
		base[i].b = b[2];
		b += 4;
	}

	if (start < _paletteDirtyStart)
		_paletteDirtyStart = start;

	if (start + num > _paletteDirtyEnd)
		_paletteDirtyEnd = start + num;
}

byte OSystem_PALMOS::RGBToColor(uint8 r, uint8 g, uint8 b) {
	byte color;

	if (gVars->stdPalette) {
		RGBColorType rgb = {0, r, g, b};
		color = WinRGBToIndex(&rgb);

	} else {
		byte nearest = 255;
		byte check;
		byte r2, g2, b2;

		color = 255;

		for (int i = 0; i < 256; i++)
		{
			r2 = _currentPalette[i].r;
			g2 = _currentPalette[i].g;
			b2 = _currentPalette[i].b;

			check = (ABS(r2 - r) + ABS(g2 - g) + ABS(b2 - b)) / 3;

			if (check == 0)				// perfect match
				return i;
			else if (check < nearest) { // else save and continue
				color = i;
				nearest = check;
			}
		}
	}
	
	return color;
}

void OSystem_PALMOS::ColorToRGB(byte color, uint8 &r, uint8 &g, uint8 &b) {
	r = _currentPalette[color].r;
	g = _currentPalette[color].g;
	b = _currentPalette[color].b;
}

void OSystem_PALMOS::copyRectToScreen(const byte *buf, int pitch, int x, int y, int w, int h) {
	/* Clip the coordinates */
	if (x < 0) {
		w += x;
		buf -= x;
		x = 0;
	}

	if (y < 0) {
		h += y;
		buf -= y * pitch;
		y = 0;
	}

	if (w > _screenWidth - x)
		w = _screenWidth - x;

	if (h > _screenHeight - y)
		h = _screenHeight - y;

	if (w <= 0 || h <= 0)
		return;

	/* FIXME: undraw mouse only if the draw rect intersects with the mouse rect */
	if (_mouseDrawn)
		undraw_mouse();

	byte *dst = _offScreenP + y * _offScreenPitch + x;

#ifndef DISABLE_ARM
	if (OPTIONS_TST(kOptDeviceARM)) {
		OSysCopyType userData = { dst, buf, pitch, _offScreenPitch, w, h };
		_PnoCall(&_arm[PNO_COPY].pnoDesc, &userData);
		return;
	}
#ifdef DEBUG_ARM
	if (OPTIONS_TST(kOptDeviceProcX86)) {
		OSysCopyType userData = { dst, buf, pitch, _offScreenPitch, w, h };
		UInt32 result = PceNativeCall((NativeFuncType*)"ARMlet.dll\0ARMlet_Main", &userData);
		return;
	}
#endif
#endif	
	// if no ARM
	if (_offScreenPitch == pitch && pitch == w) {
		MemMove(dst, buf, h * w);
	} else {
		do {
			MemMove(dst, buf, w);
			dst += _offScreenPitch;
			buf += pitch;
		} while (--h);
	}
}

void OSystem_PALMOS::updateScreen() {
	if(_quitCount)
		return;

	// Make sure the mouse is drawn, if it should be drawn.
	draw_mouse();

	// Check whether the palette was changed in the meantime and update the
	// screen surface accordingly. 
	if (_paletteDirtyEnd != 0) {
		UInt8 oldCol;

		if (gVars->stdPalette) {
			WinSetDrawWindow(WinGetDisplayWindow());	// hack by Doug
			WinPalette(winPaletteSet, _paletteDirtyStart, _paletteDirtyEnd - _paletteDirtyStart,_currentPalette + _paletteDirtyStart);
		} else {
			HwrDisplayPalette(winPaletteSet, _paletteDirtyStart, _paletteDirtyEnd - _paletteDirtyStart,_currentPalette + _paletteDirtyStart);
		}
		_paletteDirtyEnd = 0;
		oldCol = gVars->indicator.on;
		gVars->indicator.on = RGBToColor(0,255,0);

		if (oldCol != gVars->indicator.on) {	
			// redraw if needed
			if (_lastKeyModifier)
				draw1BitGfx((kDrawKeyState + _lastKeyModifier - 1), 2, getHeight() + 2, true);
			
			if(_useNumPad)
				draw1BitGfx(kDrawNumPad, (getWidth() >> 1) - 32, getHeight() + 2, true);

			if (_showBatLow)
				draw1BitGfx(kDrawBatLow, (getWidth() >> 1), -16, true);
		}
	}

	if (_overlayVisible) {
		byte *src = _tmpScreenP;
		byte *dst = _offScreenP;
		UInt16 h = _screenHeight;
		
		do {
			memcpy(dst, src, _screenWidth);
			dst += _offScreenPitch;
			src += _screenWidth;
		} while (--h);
	}

	// redraw the screen
	((this)->*(_renderer_proc))();
}

void OSystem_PALMOS::move_screen(int dx, int dy, int height) {
	// Short circuit check - do we have to do anything anyway?
	if ((dx == 0 && dy == 0) || height <= 0)
		return;

	// Hide the mouse
	if (_mouseDrawn)
		undraw_mouse();

	RectangleType r, dummy;
	WinSetDrawWindow(_offScreenH);
	RctSetRectangle(&r, ((_offScreenH != _screenH) ? 0 : _screenOffset.x), ((_offScreenH != _screenH) ? 0 : _screenOffset.y), _screenWidth, _screenHeight);

	// vertical movement
	if (dy > 0) {
		// move down - copy from bottom to top
		if (_useHRmode) {
			// need to set the draw window
			HRWinScrollRectangle(gVars->HRrefNum, &r, winDown, dy, &dummy);
		} else {
			WinScrollRectangle(&r, winDown, dy, &dummy);
		}
	} else if (dy < 0) {
		// move up - copy from top to bottom
		dy = -dy;
		if (_useHRmode) {
			// need to set the draw window
			HRWinScrollRectangle(gVars->HRrefNum, &r, winUp, dy, &dummy);
		} else {
			WinScrollRectangle(&r, winUp, dy, &dummy);
		}
	}

	// horizontal movement
	if (dx > 0) {
		// move right - copy from right to left
		if (_useHRmode) {
			// need to set the draw window
			HRWinScrollRectangle(gVars->HRrefNum, &r, winRight, dx, &dummy);
		} else {
			WinScrollRectangle(&r, winRight, dx, &dummy);
		}
	} else if (dx < 0)  {
		// move left - copy from left to right
		dx = -dx;
		if (_useHRmode) {
			// need to set the draw window
			HRWinScrollRectangle(gVars->HRrefNum, &r, winLeft, dx, &dummy);
		} else {
			WinScrollRectangle(&r, winLeft, dx, &dummy);
		}
	}


	WinSetDrawWindow(_screenH);
	// Prevent crash on Clie device using successive [HR]WinScrollRectangle !
	SysTaskDelay(1);
}

void OSystem_PALMOS::draw1BitGfx(UInt16 id, Int32 x, Int32 y, Boolean show) {
	if (OPTIONS_TST(kOptDisableOnScrDisp))
		return;

	MemHandle hTemp = DmGetResource(bitmapRsc, id);
	
	if (hTemp) {
		BitmapType *bmTemp;
		UInt32 *bmData;
		UInt8 ih, iw, ib;
		Coord w, h;
		Int16 blocks, next;

		UInt8 *scr = _screenP + x + _screenPitch * y;

		bmTemp	= (BitmapType *)MemHandleLock(hTemp);
		bmData	= (UInt32 *)BmpGetBits(bmTemp);
		BmpGlueGetDimensions(bmTemp, &w, &h, 0);

		blocks = w >> 5;
		next = w - (blocks << 5);

		if (next)
			blocks++;
		
		for (ih = 0; ih < h; ih++) {			// line
			for (ib = 0; ib < blocks; ib++) {	// 32pix block
				next = w - (ib << 5);
				next = MIN(next, (Coord)32);
		
				for (iw = 0; iw < next; iw++) {	// row

					*scr++ = ((*bmData & (1 << (31 - iw))) && show) ?
						gVars->indicator.on :
						gVars->indicator.off;
				}

				bmData++;
			}
			scr += _screenPitch - w;
		}

		MemPtrUnlock(bmTemp);
		DmReleaseResource(hTemp);
	}
}