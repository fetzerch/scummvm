/* ScummVM - Scumm Interpreter
 * Copyright (C) 2004 The ScummVM project
 *
 * The ReInherit Engine is (C)2000-2003 by Daniel Balsom.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */

// Main rendering loop
#include "saga.h"

#include "gfx.h"
#include "timer.h"
#include "actor_mod.h"
#include "console_mod.h"
#include "cvar_mod.h"
#include "font_mod.h"
#include "game_mod.h"
#include "interface_mod.h"
#include "scene_mod.h"
#include "sprite_mod.h"
#include "text_mod.h"

#include "actionmap.h"
#include "objectmap_mod.h"

#include "render.h"

namespace Saga {

const char *test_txt = "The quick brown fox jumped over the lazy dog. She sells sea shells down by the sea shore.";

int Render::reg(void) {
	return R_SUCCESS;
}

Render::Render(SagaEngine *vm, OSystem *system) : _vm(vm), _system(system), _initialized(false) {
	R_GAME_DISPLAYINFO disp_info;
	int tmp_w, tmp_h, tmp_bytepp;

	// Initialize system graphics
	GAME_GetDisplayInfo(&disp_info);

	_vm->_gfx = new Gfx(system, disp_info.logical_w, disp_info.logical_h);
	_gfx = _vm->_gfx;

	// Initialize FPS timer callback
	g_timer->installTimerProc(&fpsTimerCallback, 1000000, this);

	// Create background buffer 
	_bg_buf_w = disp_info.logical_w;
	_bg_buf_h = disp_info.logical_h;
	_bg_buf = (byte *)calloc(disp_info.logical_w, disp_info.logical_h);

	if (_bg_buf == NULL) {
		return;
	}

	// Allocate temp buffer for animation decoding, 
	// graphics scalers (2xSaI), etc.
	tmp_w = disp_info.logical_w;
	tmp_h = disp_info.logical_h + 4; // BG unbanking requres extra rows
	tmp_bytepp = 1;

	_tmp_buf = (byte *)calloc(1, tmp_w * tmp_h * tmp_bytepp);
	if (_tmp_buf == NULL) {
		free(_bg_buf);
		return;
	}

	_tmp_buf_w = tmp_w;
	_tmp_buf_h = tmp_h;

	_backbuf_surface = _gfx->getBackBuffer();
	_flags = 0;

	_initialized = true;
}

Render::~Render(void) {
	free(_bg_buf);
	free(_tmp_buf);

	_initialized = false;
}

bool Render::initialized() {
	return _initialized;
}

int Render::drawScene() {
	R_SURFACE *backbuf_surface;
	R_GAME_DISPLAYINFO disp_info;
	R_SCENE_INFO scene_info;
	SCENE_BGINFO bg_info;
	R_POINT bg_pt;
	char txt_buf[20];
	int fps_width;
	R_POINT mouse_pt;

	if (!_initialized) {
		return R_FAILURE;
	}

	_framecount++;

	backbuf_surface = _backbuf_surface;

	// Get mouse coordinates
	mouse_pt = SYSINPUT_GetMousePos();

	SCENE_GetBGInfo(&bg_info);
	GAME_GetDisplayInfo(&disp_info);
	bg_pt.x = 0;
	bg_pt.y = 0;

	// Display scene background
	SCENE_Draw(backbuf_surface);

	// Display scene maps, if applicable
	if (getFlags() & RF_OBJECTMAP_TEST) {
		OBJECTMAP_Draw(backbuf_surface, &mouse_pt, _gfx->getWhite(), _gfx->getBlack());
		_vm->_actionMap->draw(backbuf_surface, _gfx->matchColor(R_RGB_RED));
	}

	// Draw queued actors
	ACTOR_DrawList();

	// Draw queued text strings
	SCENE_GetInfo(&scene_info);

	TEXT_DrawList(scene_info.text_list, backbuf_surface);

	// Handle user input
	SYSINPUT_ProcessInput();

	// Display rendering information
	if (_flags & RF_SHOW_FPS) {
		sprintf(txt_buf, "%d", _fps);
		fps_width = FONT_GetStringWidth(SMALL_FONT_ID, txt_buf, 0, FONT_NORMAL);
		FONT_Draw(SMALL_FONT_ID, backbuf_surface, txt_buf, 0, backbuf_surface->buf_w - fps_width, 2,
					_gfx->getWhite(), _gfx->getBlack(), FONT_OUTLINE);
	}

	// Display "paused game" message, if applicable
	if (_flags & RF_RENDERPAUSE) {
		int msg_len = strlen(R_PAUSEGAME_MSG);
		int msg_w = FONT_GetStringWidth(BIG_FONT_ID, R_PAUSEGAME_MSG, msg_len, FONT_OUTLINE);
		FONT_Draw(BIG_FONT_ID, backbuf_surface, R_PAUSEGAME_MSG, msg_len,
				(backbuf_surface->buf_w - msg_w) / 2, 90, _gfx->getWhite(), _gfx->getBlack(), FONT_OUTLINE);
	}

	// Update user interface

	INTERFACE_Update(&mouse_pt, UPDATE_MOUSEMOVE);

	// Display text formatting test, if applicable
	if (_flags & RF_TEXT_TEST) {
		TEXT_Draw(MEDIUM_FONT_ID, backbuf_surface, test_txt, mouse_pt.x, mouse_pt.y,
				_gfx->getWhite(), _gfx->getBlack(), FONT_OUTLINE | FONT_CENTERED);
	}

	// Display palette test, if applicable
	if (_flags & RF_PALETTE_TEST) {
		_gfx->drawPalette(backbuf_surface);
	}

	// Draw console
	CON_Draw(backbuf_surface);

	_system->copyRectToScreen(backbuf_surface->buf, backbuf_surface->buf_w, 0, 0, 
							  backbuf_surface->buf_w, backbuf_surface->buf_h);

	_system->updateScreen();
	return R_SUCCESS;
}

unsigned int Render::getFrameCount() {
	return _framecount;
}

unsigned int Render::resetFrameCount() {
	unsigned int framecount = _framecount;

	_framecount = 0;

	return framecount;
}

void Render::fpsTimerCallback(void *refCon) {
	((Render *)refCon)->fpsTimer();
}

void Render::fpsTimer(void) {
	_fps = _framecount;
	_framecount = 0;
}

unsigned int Render::getFlags() {
	return _flags;
}

void Render::setFlag(unsigned int flag) {
	_flags |= flag;
}

void Render::toggleFlag(unsigned int flag) {
	_flags ^= flag;
}

int Render::getBufferInfo(R_BUFFER_INFO *r_bufinfo) {
	assert(r_bufinfo != NULL);

	r_bufinfo->r_bg_buf = _bg_buf;
	r_bufinfo->r_bg_buf_w = _bg_buf_w;
	r_bufinfo->r_bg_buf_h = _bg_buf_h;

	r_bufinfo->r_tmp_buf = _tmp_buf;
	r_bufinfo->r_tmp_buf_w = _tmp_buf_w;
	r_bufinfo->r_tmp_buf_h = _tmp_buf_h;

	return R_SUCCESS;
}

} // End of namespace Saga
