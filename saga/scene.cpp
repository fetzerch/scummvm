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

// Scene management module
#include "saga/saga.h"
#include "saga/yslib.h"

#include "saga/gfx.h"
#include "saga/game_mod.h"
#include "saga/animation.h"
#include "saga/console.h"
#include "saga/cvar_mod.h"
#include "saga/events.h"
#include "saga/actionmap.h"
#include "saga/isomap.h"
#include "saga/script_mod.h"
#include "saga/objectmap.h"
#include "saga/palanim.h"
#include "saga/render.h"
#include "saga/rscfile_mod.h"
#include "saga/script.h"
#include "saga/text.h"
#include "saga/sound.h"
#include "saga/music.h"

#include "saga/scene.h"
#include "saga/ite_introproc.h"
#include "saga/ihnm_introproc.h"

namespace Saga {

static void CF_scenechange(int argc, char *argv[], void *refCon);
static void CF_sceneinfo(int argc, char *argv[], void *refCon);

int defaultScene(int param, R_SCENE_INFO *scene_info);


int Scene::reg() {
	CVAR_Register_I(&_sceneNumber, "scene", NULL, R_CVAR_READONLY, 0, 0);
	CVAR_RegisterFunc(CF_scenechange, "scene_change", "<Scene number>", R_CVAR_NONE, 1, 1, this);
	CVAR_RegisterFunc(CF_sceneinfo, "scene_info", NULL, R_CVAR_NONE, 0, 0, this);

	return R_SUCCESS;
}

Scene::Scene(SagaEngine *vm) : _vm(vm), _initialized(false) {
	R_GAME_SCENEDESC gs_desc;
	byte *scene_lut_p;
	size_t scene_lut_len;
	int result;
	int i;

	// Load game-specific scene data
	GAME_GetSceneInfo(&gs_desc);

	// Load scene module resource context
	result = GAME_GetFileContext(&_sceneContext, R_GAME_RESOURCEFILE, 0);
	if (result != R_SUCCESS) {
		warning("Scene::Scene(): Couldn't load scene resource context");
		return;
	}

	// Initialize scene queue
	_sceneQueue = ys_dll_create();
	if (_sceneQueue == NULL) {
		return;
	}

	// Load scene lookup table
	debug(0, "Loading scene LUT from resource %u.", gs_desc.scene_lut_rn);
	result = RSC_LoadResource(_sceneContext, gs_desc.scene_lut_rn, &scene_lut_p, &scene_lut_len);
	if (result != R_SUCCESS) {
		warning("Scene::Scene(): Error: couldn't load scene LUT");
		return;
	}

	_sceneCount = scene_lut_len / 2;
	_sceneMax = _sceneCount - 1;
	_sceneLUT = (int *)malloc(_sceneMax * sizeof *_sceneLUT);
	if (_sceneLUT == NULL) {
		warning("Scene::Scene(): Memory allocation failed");
		return;
	}

	MemoryReadStream readS(scene_lut_p, scene_lut_len);

	for (i = 0; i < _sceneMax; i++) {
		_sceneLUT[i] = readS.readUint16LE();
	}

	free(scene_lut_p);

	if (gs_desc.first_scene != 0) {
		_firstScene = gs_desc.first_scene;
	}

	debug(0, "First scene set to %d.", _firstScene);

	debug(0, "LUT has %d entries.", _sceneMax);

	// Create scene module text list
	_textList = _vm->textCreateList();

	if (_textList == NULL) {
		warning("Scene::Scene(): Error: Couldn't create scene text list");
		return;
	}

	_sceneLoaded = false;
	_sceneMode = 0;
	_sceneNumber = 0;
	_sceneResNum = 0;
	_inGame = false;
	_loadDesc = false;
	memset(&_desc, 0, sizeof(_desc));
	_resListEntries = 0;
	_resList = NULL;
	_animEntries = 0;
	_animList = NULL;
	_sceneProc = NULL;
	memset(&_bg, 0, sizeof(_bg));
	memset(&_bgMask, 0, sizeof(_bgMask));

	_initialized = true;
}

Scene::~Scene() {
	if (_initialized) {
		endScene();
		free(_sceneLUT);
	}
}

int Scene::queueScene(R_SCENE_QUEUE *scene_queue) {
	assert(_initialized);
	assert(scene_queue != NULL);

	ys_dll_add_tail(_sceneQueue, scene_queue, sizeof *scene_queue);

	return R_SUCCESS;
}

int Scene::clearSceneQueue() {
	assert(_initialized);

	ys_dll_delete_all(_sceneQueue);

	return R_SUCCESS;
}

int Scene::startScene() {
	YS_DL_NODE *node;
	R_SCENE_QUEUE *scene_qdat;

	assert(_initialized);

	if (_sceneLoaded) {
		warning("Scene::start(): Error: Can't start game...scene already loaded");
		return R_FAILURE;
	}

	if (_inGame) {
		warning("Scene::start(): Error: Can't start game...game already started");
		return R_FAILURE;
	}

	switch (GAME_GetGameType()) {
	case R_GAMETYPE_ITE:
		ITE_StartProc();
		break;
	case R_GAMETYPE_IHNM:
		IHNM_StartProc();
		break;
	default:
		warning("Scene::start(): Error: Can't start game... gametype not supported");
		break;
	}

	// Load the head node in scene queue
	node = ys_dll_head(_sceneQueue);
	if (node == NULL) {
		return R_SUCCESS;
	}

	scene_qdat = (R_SCENE_QUEUE *)ys_dll_get_data(node);
	assert(scene_qdat != NULL);

	loadScene(scene_qdat->scene_n, scene_qdat->load_flag, scene_qdat->scene_proc, scene_qdat->scene_desc);

	return R_SUCCESS;
}

int Scene::nextScene() {
	YS_DL_NODE *node;
	R_SCENE_QUEUE *scene_qdat;

	assert(_initialized);

	if (!_sceneLoaded) {
		warning("Scene::next(): Error: Can't advance scene...no scene loaded");
		return R_FAILURE;
	}

	if (_inGame) {
		warning("Scene::next(): Error: Can't advance scene...game already started");
		return R_FAILURE;
	}

	endScene();

	// Delete the current head node in scene queue
	node = ys_dll_head(_sceneQueue);
	if (node == NULL) {
		return R_SUCCESS;
	}

	ys_dll_delete(node);

	// Load the head node in scene queue
	node = ys_dll_head(_sceneQueue);
	if (node == NULL) {
		return R_SUCCESS;
	}

	scene_qdat = (R_SCENE_QUEUE *)ys_dll_get_data(node);
	assert(scene_qdat != NULL);

	loadScene(scene_qdat->scene_n, scene_qdat->load_flag, scene_qdat->scene_proc, scene_qdat->scene_desc);

	return R_SUCCESS;
}

int Scene::skipScene() {
	YS_DL_NODE *node;
	YS_DL_NODE *prev_node;
	YS_DL_NODE *skip_node = NULL;

	R_SCENE_QUEUE *scene_qdat = NULL;
	R_SCENE_QUEUE *skip_qdat = NULL;

	assert(_initialized);

	if (!_sceneLoaded) {
		warning("Scene::skip(): Error: Can't skip scene...no scene loaded");
		return R_FAILURE;
	}

	if (_inGame) {
		warning("Scene::skip(): Error: Can't skip scene...game already started");
		return R_FAILURE;
	}

	// Walk down scene queue and try to find a skip target
	node = ys_dll_head(_sceneQueue);
	if (node == NULL) {
		warning("Scene::skip(): Error: Can't skip scene...no scenes in queue");
		return R_FAILURE;
	}

	for (node = ys_dll_next(node); node != NULL; node = ys_dll_next(node)) {
		scene_qdat = (R_SCENE_QUEUE *)ys_dll_get_data(node);
		assert(scene_qdat != NULL);

		if (scene_qdat->scene_skiptarget) {
			skip_node = node;
			skip_qdat = scene_qdat;
			break;
		}
	}

	// If skip target found, remove preceding scenes and load
	if (skip_node != NULL) {
		for (node = ys_dll_prev(skip_node); node != NULL; node = prev_node) {
			prev_node = ys_dll_prev(node);
			ys_dll_delete(node);
		}
		endScene();
		loadScene(skip_qdat->scene_n, skip_qdat->load_flag, skip_qdat->scene_proc, skip_qdat->scene_desc);
	}
	// Search for a scene to skip to

	return R_SUCCESS;
}

int Scene::changeScene(int scene_num) {
	assert(_initialized);

	if (!_sceneLoaded) {
		warning("Scene::changeScene(): Error: Can't change scene. No scene currently loaded. Game in invalid state");
		return R_FAILURE;
	}

	if ((scene_num < 0) || (scene_num > _sceneMax)) {
		warning("Scene::changeScene(): Error: Can't change scene. Invalid scene number");
		return R_FAILURE;
	}

	if (_sceneLUT[scene_num] == 0) {
		warning("Scene::changeScene(): Error: Can't change scene; invalid scene descriptor resource number (0)");
		return R_FAILURE;
	}

	endScene();
	loadScene(scene_num, BY_SCENE, defaultScene, NULL);

	return R_SUCCESS;
}

int Scene::getMode() {
	assert(_initialized);

	return _sceneMode;
}

int Scene::getZInfo(SCENE_ZINFO *zinfo) {
	assert(_initialized);

	zinfo->begin_slope = _desc.begin_slope;
	zinfo->end_slope = _desc.end_slope;

	return R_SUCCESS;
}

int Scene::getBGInfo(SCENE_BGINFO *bginfo) {
	R_GAME_DISPLAYINFO di;
	int x, y;

	assert(_initialized);

	bginfo->bg_buf = _bg.buf;
	bginfo->bg_buflen = _bg.buf_len;
	bginfo->bg_w = _bg.w;
	bginfo->bg_h = _bg.h;
	bginfo->bg_p = _bg.p;

	GAME_GetDisplayInfo(&di);
	x = 0;
	y = 0;

	if (_bg.w < di.logical_w) {
		x = (di.logical_w - _bg.w) / 2;
	}

	if (_bg.h < di.scene_h) {
		y = (di.scene_h - _bg.h) / 2;
	}

	bginfo->bg_x = x;
	bginfo->bg_y = y;

	return R_SUCCESS;
}

int Scene::getBGPal(PALENTRY **pal) {
	assert(_initialized);
	*pal = _bg.pal;

	return R_SUCCESS;
}

int Scene::getBGMaskInfo(int *w, int *h, byte **buf, size_t *buf_len) {
	assert(_initialized);

	if (!_bgMask.loaded) {
		return R_FAILURE;
	}

	*w = _bgMask.w;
	*h = _bgMask.h;
	*buf = _bgMask.buf;
	*buf_len = _bgMask.buf_len;

	return R_SUCCESS;
}

int Scene::isBGMaskPresent() {
	assert(_initialized);

	return _bgMask.loaded;
}

int Scene::getInfo(R_SCENE_INFO *si) {
	assert(_initialized);
	assert(si != NULL);

	si->text_list = _textList;

	return R_SUCCESS;
}

int Scene::loadScene(int scene_num, int load_flag, R_SCENE_PROC scene_proc, R_SCENE_DESC *scene_desc_param) {
	R_SCENE_INFO scene_info;
	uint32 res_number = 0;
	int result;
	int i;

	assert(_initialized);

	if (_sceneLoaded) {
		warning("Scene::loadScene(): Error, a scene is already loaded");
		return R_FAILURE;
	}

	_animList = ys_dll_create();
	_sceneMode = 0;
	_loadDesc = true;

	switch (load_flag) {
	case BY_RESOURCE:
		res_number = scene_num;
		break;
	case BY_SCENE:
		assert((scene_num > 0) && (scene_num < _sceneMax));
		res_number = _sceneLUT[scene_num];
		_sceneNumber = scene_num;
		break;
	case BY_DESC:
		assert(scene_desc_param != NULL);
		assert(scene_desc_param->res_list != NULL);
		_loadDesc = false;
		_desc = *scene_desc_param;
		_resList = scene_desc_param->res_list;
		_resListEntries = scene_desc_param->res_list_ct;
		break;
	default:
		warning("Scene::loadScene(): Error: Invalid scene load flag");
		return R_FAILURE;
		break;
	}

	// Load scene descriptor and resource list resources
	if (_loadDesc) {

		_sceneResNum = res_number;
		assert(_sceneResNum != 0);
		debug(0, "Loading scene resource %u:", res_number);

		if (loadSceneDescriptor(res_number) != R_SUCCESS) {
			warning("Scene::loadScene(): Error reading scene descriptor");
			return R_FAILURE;
		}

		if (loadSceneResourceList(_desc.res_list_rn) != R_SUCCESS) {
			warning("Scene::loadScene(): Error reading scene resource list");
			return R_FAILURE;
		}
	} else {
		debug(0, "Loading memory scene resource.");
	}

	// Load resources from scene resource list
	for (i = 0; i < _resListEntries; i++) {
		result = RSC_LoadResource(_sceneContext, _resList[i].res_number,
								&_resList[i].res_data, &_resList[i].res_data_len);
		if (result != R_SUCCESS) {
			warning("Scene::loadScene(): Error: Allocation failure loading scene resource list");
			return R_FAILURE;
		}
	}

	// Process resources from scene resource list
	if (processSceneResources() != R_SUCCESS) {
		warning("Scene::loadScene(): Error loading scene resources");
		return R_FAILURE;
	}

	// Load scene script data
	if (_desc.script_num > 0) {
		if (_vm->_script->loadScript(_desc.script_num) != R_SUCCESS) {
			warning("Scene::loadScene(): Error loading scene script");
			return R_FAILURE;
		}
	}

	_sceneLoaded = true;

	if (scene_proc == NULL) {
		_sceneProc = defaultScene;
	} else {
		_sceneProc = scene_proc;
	}

	getInfo(&scene_info);

	_sceneProc(SCENE_BEGIN, &scene_info);

	return R_SUCCESS;
}

int Scene::loadSceneDescriptor(uint32 res_number) {
	byte *scene_desc_data;
	size_t scene_desc_len;
	int result;

	result = RSC_LoadResource(_sceneContext, res_number, &scene_desc_data, &scene_desc_len);
	if (result != R_SUCCESS) {
		warning("Scene::loadSceneDescriptor(): Error: couldn't load scene descriptor");
		return R_FAILURE;
	}

	if (scene_desc_len != SAGA_SCENE_DESC_LEN) {
		warning("Scene::loadSceneDescriptor(): Error: scene descriptor length invalid");
		return R_FAILURE;
	}

	MemoryReadStream readS(scene_desc_data, scene_desc_len);

	_desc.unknown0 = readS.readUint16LE();
	_desc.res_list_rn = readS.readUint16LE();
	_desc.end_slope = readS.readUint16LE();
	_desc.begin_slope = readS.readUint16LE();
	_desc.script_num = readS.readUint16LE();
	_desc.scene_scriptnum = readS.readUint16LE();
	_desc.start_scriptnum = readS.readUint16LE();
	_desc.music_rn = readS.readSint16LE();

	RSC_FreeResource(scene_desc_data);

	return R_SUCCESS;
}

int Scene::loadSceneResourceList(uint32 reslist_rn) {
	byte *resource_list;
	size_t resource_list_len;
	int result;
	int i;

	// Load the scene resource table
	result = RSC_LoadResource(_sceneContext, reslist_rn, &resource_list, &resource_list_len);
	if (result != R_SUCCESS) {
		warning("Scene::loadSceneResourceList(): Error: couldn't load scene resource list");
		return R_FAILURE;
	}

	MemoryReadStream readS(resource_list, resource_list_len);

	// Allocate memory for scene resource list 
	_resListEntries = resource_list_len / SAGA_RESLIST_ENTRY_LEN;
	debug(0, "Scene resource list contains %d entries.", _resListEntries);
	_resList = (R_SCENE_RESLIST *)calloc(_resListEntries, sizeof *_resList);

	if (_resList == NULL) {
		warning("Scene::loadSceneResourceList(): Error: Memory allocation failed");
		return R_MEM;
	}

	// Load scene resource list from raw scene 
	// resource table
	debug(0, "Loading scene resource list...");

	for (i = 0; i < _resListEntries; i++) {
		_resList[i].res_number = readS.readUint16LE();
		_resList[i].res_type = readS.readUint16LE();
	}

	RSC_FreeResource(resource_list);

	return R_SUCCESS;
}

int Scene::processSceneResources() {
	const byte *res_data;
	size_t res_data_len;
	const byte *pal_p;
	int i;

	// Process the scene resource list
	for (i = 0; i < _resListEntries; i++) {
		res_data = _resList[i].res_data;
		res_data_len = _resList[i].res_data_len;
		switch (_resList[i].res_type) {
		case SAGA_BG_IMAGE: // Scene background resource
			if (_bg.loaded) {
				warning("Scene::processSceneResources(): Multiple background resources encountered");
				return R_FAILURE;
			}

			debug(0, "Loading background resource.");
			_bg.res_buf = _resList[i].res_data;
			_bg.res_len = _resList[i].res_data_len;
			_bg.loaded = 1;

			if (_vm->decodeBGImage(_bg.res_buf,
				_bg.res_len,
				&_bg.buf,
				&_bg.buf_len,
				&_bg.w,
				&_bg.h) != R_SUCCESS) {
				warning("Scene::ProcessSceneResources(): Error loading background resource: %u", _resList[i].res_number);
				return R_FAILURE;
			}

			pal_p = _vm->getImagePal(_bg.res_buf, _bg.res_len);
			memcpy(_bg.pal, pal_p, sizeof _bg.pal);
			_sceneMode = R_SCENE_MODE_NORMAL;
			break;
		case SAGA_BG_MASK: // Scene background mask resource
			if (_bgMask.loaded) {
				warning("Scene::ProcessSceneResources(): Duplicate background mask resource encountered");
			}
			debug(0, "Loading BACKGROUND MASK resource.");
			_bgMask.res_buf = _resList[i].res_data;
			_bgMask.res_len = _resList[i].res_data_len;
			_bgMask.loaded = 1;
			_vm->decodeBGImage(_bgMask.res_buf, _bgMask.res_len, &_bgMask.buf,
							&_bgMask.buf_len, &_bgMask.w, &_bgMask.h);
			break;
		case SAGA_OBJECT_NAME_LIST:
			debug(0, "Loading object name list resource...");
			_vm->_objectMap->loadNames(_resList[i].res_data, _resList[i].res_data_len);
			break;
		case SAGA_OBJECT_MAP:
			debug(0, "Loading object map resource...");
			if (_vm->_objectMap->load(res_data,
				res_data_len) != R_SUCCESS) {
				warning("Scene::ProcessSceneResources(): Error loading object map resource");
				return R_FAILURE;
			}
			break;
		case SAGA_ACTION_MAP:
			debug(0, "Loading exit map resource...");
			if (_vm->_actionMap->loadMap(res_data, res_data_len) != R_SUCCESS) {
				warning("Scene::ProcessSceneResources(): Error loading exit map resource");
				return R_FAILURE;
			}
			break;
		case SAGA_ISO_TILESET:
			if (_sceneMode == R_SCENE_MODE_NORMAL) {
				warning("Scene::ProcessSceneResources(): Isometric tileset incompatible with normal scene mode");
				return R_FAILURE;
			}

			debug(0, "Loading isometric tileset resource.");

			if (_vm->_isoMap->loadTileset(res_data, res_data_len) != R_SUCCESS) {
				warning("Scene::ProcessSceneResources(): Error loading isometric tileset resource");
				return R_FAILURE;
			}

			_sceneMode = R_SCENE_MODE_ISO;
			break;
		case SAGA_ISO_METAMAP:
			if (_sceneMode == R_SCENE_MODE_NORMAL) {
				warning("Scene::ProcessSceneResources(): Isometric metamap incompatible with normal scene mode");
				return R_FAILURE;
			}

			debug(0, "Loading isometric metamap resource.");

			if (_vm->_isoMap->loadMetamap(res_data, res_data_len) != R_SUCCESS) {
				warning("Scene::ProcessSceneResources(): Error loading isometric metamap resource");
				return R_FAILURE;
			}

			_sceneMode = R_SCENE_MODE_ISO;
			break;
		case SAGA_ISO_METATILESET:
			if (_sceneMode == R_SCENE_MODE_NORMAL) {
				warning("Scene::ProcessSceneResources(): Isometric metatileset incompatible with normal scene mode");
				return R_FAILURE;
			}

			debug(0, "Loading isometric metatileset resource.");

			if (_vm->_isoMap->loadMetaTileset(res_data, res_data_len) != R_SUCCESS) {
				warning("Scene::ProcessSceneResources(): Error loading isometric tileset resource");
				return R_FAILURE;
			}

			_sceneMode = R_SCENE_MODE_ISO;
			break;
		case SAGA_ANIM_1:
		case SAGA_ANIM_2:
		case SAGA_ANIM_3:
		case SAGA_ANIM_4:
		case SAGA_ANIM_5:
		case SAGA_ANIM_6:
		case SAGA_ANIM_7:
			{
				SCENE_ANIMINFO *new_animinfo;
				uint16 new_anim_id;

				debug(0, "Loading animation resource...");

				new_animinfo = (SCENE_ANIMINFO *)malloc(sizeof *new_animinfo);
				if (new_animinfo == NULL) {
					warning("Scene::ProcessSceneResources(): Memory allocation error");
					return R_MEM;
				}

				if (_vm->_anim->load(_resList[i].res_data,
					_resList[i].res_data_len, &new_anim_id) != R_SUCCESS) {
					warning("Scene::ProcessSceneResources(): Error loading animation resource");
					return R_FAILURE;
				}

				new_animinfo->anim_handle = new_anim_id;
				new_animinfo->anim_res_number =  _resList[i].res_number;
				ys_dll_add_tail(_animList, new_animinfo, sizeof *new_animinfo);
				_animEntries++;
			}
			break;
		case SAGA_PAL_ANIM:
			debug(0, "Loading palette animation resource.");
			_vm->_palanim->loadPalAnim(_resList[i].res_data, _resList[i].res_data_len);
			break;
		default:
			warning("Scene::ProcessSceneResources(): Encountered unknown resource type: %d", _resList[i].res_type);
			break;
		}
	}
	return R_SUCCESS;
}

int Scene::draw(R_SURFACE *dst_s) {
	R_GAME_DISPLAYINFO disp_info;
	R_BUFFER_INFO buf_info;
	R_POINT bg_pt;

	assert(_initialized);

	_vm->_render->getBufferInfo(&buf_info);
	GAME_GetDisplayInfo(&disp_info);

	bg_pt.x = 0;
	bg_pt.y = 0;

	switch (_sceneMode) {

	case R_SCENE_MODE_NORMAL:
		_vm->_gfx->bufToSurface(dst_s, buf_info.r_bg_buf, disp_info.logical_w,
						MAX(disp_info.scene_h, _bg.h), NULL, &bg_pt);
		break;
	case R_SCENE_MODE_ISO:
		_vm->_isoMap->draw(dst_s);
		break;
	default:
		// Unknown scene mode
		return R_FAILURE;
		break;
	};

	return R_SUCCESS;
}

int Scene::endScene() {
	R_SCENE_INFO scene_info;

	assert(_initialized);

	if (!_sceneLoaded) {
		warning("Scene::endScene(): No scene to end");
		return -1;
	}

	debug(0, "Ending scene...");

	getInfo(&scene_info);

	_sceneProc(SCENE_END, &scene_info);

	if (_desc.script_num > 0) {
		_vm->_script->freeScript();
	}

	// Free scene background
	if (_bg.loaded) {
		free(_bg.buf);
		_bg.loaded = 0;
	}

	// Free scene background mask
	if (_bgMask.loaded) {
		free(_bgMask.buf);
		_bgMask.loaded = 0;
	}

	// Free scene resource list
	if (_loadDesc) {

		free(_resList);
	}

	// Free animation info list
	_vm->_anim->reset();

	_vm->_palanim->freePalAnim();
	_vm->_objectMap->freeMem();
	_vm->_actionMap->freeMap();

	ys_dll_destroy(_animList);

	_animEntries = 0;

	_vm->_events->clearList();
	_vm->textClearList(_textList);

	_sceneLoaded = false;

	return R_SUCCESS;
}

void Scene::sceneChangeCmd(int argc, char *argv[]) {
	int scene_num = 0;

	if ((argc == 0) || (argc > 1)) {
		return;
	}

	scene_num = atoi(argv[0]);

	if ((scene_num < 1) || (scene_num > _sceneMax)) {
		_vm->_console->print("Invalid scene number.");
		return;
	}

	clearSceneQueue();

	if (changeScene(scene_num) == R_SUCCESS) {
		_vm->_console->print("Scene changed.");
	} else {
		_vm->_console->print("Couldn't change scene!");
	}
}

static void CF_scenechange(int argc, char *argv[], void *refCon) {
	((Scene *)refCon)->sceneChangeCmd(argc, argv);
}

void Scene::sceneInfoCmd(int argc, char *argv[]) {
	const char *fmt = "%-20s %d";

	_vm->_console->print(fmt, "Scene number:", _sceneNumber);
	_vm->_console->print(fmt, "Descriptor R#:", _sceneResNum);
	_vm->_console->print("-------------------------");
	_vm->_console->print(fmt, "Unknown:", _desc.unknown0);
	_vm->_console->print(fmt, "Resource list R#:", _desc.res_list_rn);
	_vm->_console->print(fmt, "End slope:", _desc.end_slope);
	_vm->_console->print(fmt, "Begin slope:", _desc.begin_slope);
	_vm->_console->print(fmt, "Script resource:", _desc.script_num);
	_vm->_console->print(fmt, "Scene script:", _desc.scene_scriptnum);
	_vm->_console->print(fmt, "Start script:", _desc.start_scriptnum);
	_vm->_console->print(fmt, "Music R#", _desc.music_rn);
}

static void CF_sceneinfo(int argc, char *argv[], void *refCon) {
	((Scene *)refCon)->sceneInfoCmd(argc, argv);
}

int defaultScene(int param, R_SCENE_INFO *scene_info) {
	R_EVENT event;

	switch (param) {
	case SCENE_BEGIN:
		// Set scene background
		event.type = R_ONESHOT_EVENT;
		event.code = R_BG_EVENT;
		event.op = EVENT_DISPLAY;
		event.param = SET_PALETTE;
		event.time = 0;

		_vm->_events->queue(&event);

		// Activate user interface
		event.type = R_ONESHOT_EVENT;
		event.code = R_INTERFACE_EVENT;
		event.op = EVENT_ACTIVATE;
		event.time = 0;

		_vm->_events->queue(&event);

		// Begin palette cycle animation if present
		event.type = R_ONESHOT_EVENT;
		event.code = R_PALANIM_EVENT;
		event.op = EVENT_CYCLESTART;
		event.time = 0;

		_vm->_events->queue(&event);
		break;
	case SCENE_END:
		break;
	default:
		warning("Scene::defaultScene(): Illegal scene procedure parameter");
		break;
	}

	return 0;
}

} // End of namespace Saga
