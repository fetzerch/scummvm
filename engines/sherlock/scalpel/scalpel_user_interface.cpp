/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "sherlock/scalpel/scalpel_user_interface.h"
#include "sherlock/sherlock.h"
#include "sherlock/scalpel/settings.h"

namespace Sherlock {

namespace Scalpel {

// Main user interface menu control locations
const int MENU_POINTS[12][4] = {
	{ 13, 153, 72, 165 },
	{ 13, 169, 72, 181 },
	{ 13, 185, 72, 197 },
	{ 88, 153, 152, 165 },
	{ 88, 169, 152, 181 },
	{ 88, 185, 152, 197 },
	{ 165, 153, 232, 165 },
	{ 165, 169, 232, 181 },
	{ 165, 185, 233, 197 },
	{ 249, 153, 305, 165 },
	{ 249, 169, 305, 181 },
	{ 249, 185, 305, 197 }
};

// Inventory control locations */
const int INVENTORY_POINTS[8][3] = {
	{ 4, 50, 29 },
	{ 52, 99, 77 },
	{ 101, 140, 123 },
	{ 142, 187, 166 },
	{ 189, 219, 198 },
	{ 221, 251, 234 },
	{ 253, 283, 266 },
	{ 285, 315, 294 }
};

const char COMMANDS[13] = "LMTPOCIUGJFS";
const char INVENTORY_COMMANDS[9] = { "ELUG-+,." };
const char *const PRESS_KEY_FOR_MORE = "Press any Key for More.";
const char *const PRESS_KEY_TO_CONTINUE = "Press any Key to Continue.";

const char *const MOPEN[] = {
	"This cannot be opened", "It is already open", "It is locked", "Wait for Watson", " ", "."
};
const char *const MCLOSE[] = {
	"This cannot be closed", "It is already closed", "The safe door is in the way"
};
const char *const MMOVE[] = {
	"This cannot be moved", "It is bolted to the floor", "It is too heavy", "The other crate is in the way"
};
const char *const MPICK[] = {
	"Nothing of interest here", "It is bolted down", "It is too big to carry", "It is too heavy",
	"I think a girl would be more your type", "Those flowers belong to Penny", "She's far too young for you!",
	"I think a girl would be more your type!", "Government property for official use only"
};
const char *const MUSE[] = {
	"You can't do that", "It had no effect", "You can't reach it", "OK, the door looks bigger! Happy?",
	"Doors don't smoke"
};

/*----------------------------------------------------------------*/


ScalpelUserInterface::ScalpelUserInterface(SherlockEngine *vm): UserInterface(vm) {
	if (_vm->_interactiveFl) {
		_controls = new ImageFile("menu.all");
		_controlPanel = new ImageFile("controls.vgs");
	} else {
		_controls = nullptr;
		_controlPanel = nullptr;
	}

	_keyPress = '\0';
	_lookHelp = 0;
	_bgFound = 0;
	_oldBgFound = -1;
	_help = _oldHelp = 0;
	_key = _oldKey = '\0';
	_temp = _oldTemp = 0;
	_oldLook = 0;
	_keyboardInput = false;
	_pause = false;
	_cNum = 0;
	_find = 0;
	_oldUse = 0;
}

ScalpelUserInterface::~ScalpelUserInterface() {
	delete _controls;
	delete _controlPanel;
}

void ScalpelUserInterface::reset() {
	_oldKey = -1;
	_help = _oldHelp = -1;
	_oldTemp = _temp = -1;
}

void ScalpelUserInterface::drawInterface(int bufferNum) {
	Screen &screen = *_vm->_screen;

	if (bufferNum & 1)
		screen._backBuffer1.transBlitFrom((*_controlPanel)[0], Common::Point(0, CONTROLS_Y));
	if (bufferNum & 2)
		screen._backBuffer2.transBlitFrom((*_controlPanel)[0], Common::Point(0, CONTROLS_Y));
	if (bufferNum == 3)
		screen._backBuffer2.fillRect(0, INFO_LINE, SHERLOCK_SCREEN_WIDTH, INFO_LINE + 10, INFO_BLACK);
}

void ScalpelUserInterface::handleInput() {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	People &people = *_vm->_people;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Talk &talk = *_vm->_talk;

	if (_menuCounter)
		whileMenuCounter();

	Common::Point pt = events.mousePos();
	_bgFound = scene.findBgShape(Common::Rect(pt.x, pt.y, pt.x + 1, pt.y + 1));
	_keyPress = '\0';

	// Check kbd and set the mouse released flag if Enter or space is pressed.
	// Otherwise, the pressed _key is stored for later use
	if (events.kbHit()) {
		Common::KeyState keyState = events.getKey();
		_keyPress = keyState.ascii;

		if (keyState.keycode == Common::KEYCODE_x && keyState.flags & Common::KBD_ALT) {
			_vm->quitGame();
			events.pollEvents();
			return;
		}
	}

	// Do button highlighting check
	if (!talk._scriptMoreFlag) {	// Don't if scripts are running
		if (((events._rightPressed || events._rightReleased) && _helpStyle) ||
				(!_helpStyle && !_menuCounter)) {
			// Handle any default commands if we're in STD_MODE
			if (_menuMode == STD_MODE) {
				if (pt.y < CONTROLS_Y &&
					(events._rightPressed || (!_helpStyle && !events._released)) &&
					(_bgFound != -1) && (_bgFound < 1000) &&
					(scene._bgShapes[_bgFound]._defaultCommand ||
					!scene._bgShapes[_bgFound]._description.empty())) {
					// If there is no default command, so set it to Look
					if (scene._bgShapes[_bgFound]._defaultCommand)
						_help = scene._bgShapes[_bgFound]._defaultCommand - 1;
					else
						_help = 0;

					// Reset 'help' if it is an invalid command
					if (_help > 5)
						_help = -1;
				} else if (pt.y < CONTROLS_Y &&
					((events._rightReleased && _helpStyle) || (events._released && !_helpStyle)) &&
					(_bgFound != -1 && _bgFound < 1000) &&
					(scene._bgShapes[_bgFound]._defaultCommand ||
					!scene._bgShapes[_bgFound]._description.empty())) {
					// If there is no default command, set it to Look
					if (scene._bgShapes[_bgFound]._defaultCommand)
						_menuMode = (MenuMode)scene._bgShapes[_bgFound]._defaultCommand;
					else
						_menuMode = LOOK_MODE;
					events._released = true;
					events._pressed = events._oldButtons = false;
					_help = _oldHelp = -1;

					if (_menuMode == LOOK_MODE) {
						// Set the flag to tell the game that this was a right-click
						// call to look and should exit without the look button being pressed
						_lookHelp = true;
					}
				} else {
					_help = -1;
				}

				// Check if highlighting a different button than last time
				if (_help != _oldHelp) {
					// If another button was highlighted previously, restore it
					if (_oldHelp != -1)
						restoreButton(_oldHelp);

					// If we're highlighting a new button, then draw it pressed
					if (_help != -1)
						depressButton(_help);

					_oldHelp = _help;
				}

				if (_bgFound != _oldBgFound || _oldBgFound == -1) {
					_infoFlag = true;
					clearInfo();

					if (_help != -1 && !scene._bgShapes[_bgFound]._description.empty()
							&& scene._bgShapes[_bgFound]._description[0] != ' ')
						screen.print(Common::Point(0, INFO_LINE + 1),
						INFO_FOREGROUND, "%s", scene._bgShapes[_bgFound]._description.c_str());

					_oldBgFound = _bgFound;
				}
			} else {
				// We're not in STD_MODE
				// If there isn't a window open, then revert back to STD_MODE
				if (!_windowOpen && events._rightReleased) {
					// Restore all buttons
					for (int idx = 0; idx < 12; ++idx)
						restoreButton(idx);

					_menuMode = STD_MODE;
					_key = _oldKey = -1;
					_temp = _oldTemp = _lookHelp = _invLookFlag = 0;
					events.clearEvents();
				}
			}
		}
	}

	// Reset the old bgshape number if the mouse button is released, so that
	// it can e re-highlighted when we come back here
	if ((events._rightReleased && _helpStyle) || (events._released && !_helpStyle))
		_oldBgFound = -1;

	// Do routines that should be done before input processing
	switch (_menuMode) {
	case LOOK_MODE:
		if (!_windowOpen) {
			if (events._released && _bgFound >= 0 && _bgFound < 1000) {
				if (!scene._bgShapes[_bgFound]._examine.empty())
					examine();
			} else {
				lookScreen(pt);
			}
		}
		break;

	case MOVE_MODE:
	case OPEN_MODE:
	case CLOSE_MODE:
	case PICKUP_MODE:
		lookScreen(pt);
		break;

	case TALK_MODE:
		if (!_windowOpen) {
			bool personFound;

			if (_bgFound >= 1000) {
				personFound = false;
				if (!events._released)
					lookScreen(pt);
			} else {
				personFound = _bgFound != -1 && scene._bgShapes[_bgFound]._aType == PERSON;
			}

			if (events._released && personFound)
				talk.talk(_bgFound);
			else if (personFound)
				lookScreen(pt);
			else if (_bgFound < 1000)
				clearInfo();
		}
		break;

	case USE_MODE:
	case GIVE_MODE:
	case INV_MODE:
		if (inv._invMode == INVMODE_LOOK || inv._invMode == INVMODE_USE || inv._invMode == INVMODE_GIVE) {
			if (pt.y > CONTROLS_Y)
				lookInv();
			else
				lookScreen(pt);
		}
		break;

	default:
		break;
	}

	//
	// Do input processing
	//
	if (events._pressed || events._released || events._rightPressed || _keyPress || _pause) {
		if (((events._released && (_helpStyle || _help == -1)) || (events._rightReleased && !_helpStyle)) &&
				(pt.y <= CONTROLS_Y) && (_menuMode == STD_MODE)) {
			// The mouse was clicked in the playing area with no action buttons down.
			// Check if the mouse was clicked in a script zone. If it was,
			// then execute the script. Otherwise, walk to the given position
			if (scene.checkForZones(pt, SCRIPT_ZONE) != 0 ||
					scene.checkForZones(pt, NOWALK_ZONE) != 0) {
				// Mouse clicked in script zone
				events._pressed = events._released = false;
			} else {
				people._walkDest = pt;
				people._allowWalkAbort = false;
				people.goAllTheWay();
			}

			if (_oldKey != -1) {
				restoreButton(_oldTemp);
				_oldKey = -1;
			}
		}

		// Handle action depending on selected mode
		switch (_menuMode) {
		case LOOK_MODE:
			if (_windowOpen)
				doLookControl();
			break;

		case MOVE_MODE:
			doMiscControl(ALLOW_MOVE);
			break;

		case TALK_MODE:
			if (_windowOpen)
				doTalkControl();
			break;

		case OPEN_MODE:
			doMiscControl(ALLOW_OPEN);
			break;

		case CLOSE_MODE:
			doMiscControl(ALLOW_CLOSE);
			break;

		case PICKUP_MODE:
			doPickControl();
			break;

		case USE_MODE:
		case GIVE_MODE:
		case INV_MODE:
			doInvControl();
			break;

		case FILES_MODE:
			doEnvControl();
			break;

		default:
			break;
		}

		// As long as there isn't an open window, do main input processing.
		// Windows are opened when in TALK, USE, INV, and GIVE modes
		if ((!_windowOpen && !_menuCounter && pt.y > CONTROLS_Y) ||
				_keyPress) {
			if (events._pressed || events._released || _pause || _keyPress)
				doMainControl();
		}

		if (pt.y < CONTROLS_Y && events._pressed && _oldTemp != (int)(_menuMode - 1) && _oldKey != -1)
			restoreButton(_oldTemp);
	}
}

void ScalpelUserInterface::depressButton(int num) {
	Screen &screen = *_vm->_screen;
	Common::Point pt(MENU_POINTS[num][0], MENU_POINTS[num][1]);

	ImageFrame &frame = (*_controls)[num];
	screen._backBuffer1.transBlitFrom(frame, pt);
	screen.slamArea(pt.x, pt.y, pt.x + frame._width, pt.y + frame._height);
}

void ScalpelUserInterface::restoreButton(int num) {
	Screen &screen = *_vm->_screen;
	Common::Point pt(MENU_POINTS[num][0], MENU_POINTS[num][1]);
	Graphics::Surface &frame = (*_controls)[num]._frame;

	screen._backBuffer1.blitFrom(screen._backBuffer2, pt,
		Common::Rect(pt.x, pt.y, pt.x + 90, pt.y + 19));
	screen.slamArea(pt.x, pt.y, pt.x + frame.w, pt.y + frame.h);

	if (!_menuCounter) {
		_infoFlag = true;
		clearInfo();
	}
}

void ScalpelUserInterface::pushButton(int num) {
	Events &events = *_vm->_events;
	_oldKey = -1;

	if (!events._released) {
		if (_oldHelp != -1)
			restoreButton(_oldHelp);
		if (_help != -1)
			restoreButton(_help);

		depressButton(num);
		events.wait(6);
	}

	restoreButton(num);
}

void ScalpelUserInterface::toggleButton(int num) {
	Screen &screen = *_vm->_screen;

	if (_menuMode != (MenuMode)(num + 1)) {
		_menuMode = (MenuMode)(num + 1);
		_oldKey = COMMANDS[num];
		_oldTemp = num;

		if (_keyboardInput) {
			if (_oldHelp != -1 && _oldHelp != num)
				restoreButton(_oldHelp);
			if (_help != -1 && _help != num)
				restoreButton(_help);

			_keyboardInput = false;

			ImageFrame &frame = (*_controls)[num];
			Common::Point pt(MENU_POINTS[num][0], MENU_POINTS[num][1]);
			screen._backBuffer1.transBlitFrom(frame, pt);
			screen.slamArea(pt.x, pt.y, pt.x + frame._width, pt.y + frame._height);
		}
	} else {
		_menuMode = STD_MODE;
		_oldKey = -1;
		restoreButton(num);
	}
}

void ScalpelUserInterface::clearInfo() {
	if (_infoFlag) {
		_vm->_screen->vgaBar(Common::Rect(16, INFO_LINE, SHERLOCK_SCREEN_WIDTH - 19,
			INFO_LINE + 10), INFO_BLACK);
		_infoFlag = false;
		_oldLook = -1;
	}
}

void ScalpelUserInterface::clearWindow() {
	if (_windowOpen) {
		_vm->_screen->vgaBar(Common::Rect(3, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2,
			SHERLOCK_SCREEN_HEIGHT - 2), INV_BACKGROUND);
	}
}

void ScalpelUserInterface::whileMenuCounter() {
	if (!(--_menuCounter) || _vm->_events->checkInput()) {
		_menuCounter = 0;
		_infoFlag = true;
		clearInfo();
	}
}

void ScalpelUserInterface::examine() {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	People &people = *_vm->_people;
	Scene &scene = *_vm->_scene;
	Talk &talk = *_vm->_talk;
	Common::Point pt = events.mousePos();

	if (pt.y < (CONTROLS_Y + 9)) {
		Object &obj = scene._bgShapes[_bgFound];

		if (obj._lookcAnim != 0) {
			int canimSpeed = ((obj._lookcAnim & 0xe0) >> 5) + 1;
			scene._cAnimFramePause = obj._lookFrames;
			_cAnimStr = obj._examine;
			_cNum = (obj._lookcAnim & 0x1f) - 1;

			scene.startCAnim(_cNum, canimSpeed);
		} else if (obj._lookPosition.y != 0) {
			// Need to walk to the object to be examined
			people.walkToCoords(Common::Point(obj._lookPosition.x, obj._lookPosition.y * 100), obj._lookFacing);
		}

		if (!talk._talkToAbort) {
			_cAnimStr = obj._examine;
			if (obj._lookFlag)
				_vm->setFlags(obj._lookFlag);
		}
	} else {
		// Looking at an inventory item
		_cAnimStr = inv[_selector]._examine;
		if (inv[_selector]._lookFlag)
			_vm->setFlags(inv[_selector]._lookFlag);
	}

	if (_invLookFlag) {
		// Don't close the inventory window when starting an examine display, since its
		// window will slide up to replace the inventory display
		_windowOpen = false;
		_menuMode = LOOK_MODE;
	}

	if (!talk._talkToAbort) {
		if (!scene._cAnimFramePause)
			printObjectDesc(_cAnimStr, true);
		else
			// description was already printed in startCAnimation
			scene._cAnimFramePause = 0;
	}
}

void ScalpelUserInterface::lookScreen(const Common::Point &pt) {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Common::Point mousePos = events.mousePos();
	int temp;
	Common::String tempStr;

	// Don't display anything for right button command
	if ((events._rightPressed || events._rightReleased) && !events._pressed)
		return;

	if (mousePos.y < CONTROLS_Y && (temp = _bgFound) != -1) {
		if (temp != _oldLook) {
			_infoFlag = true;
			clearInfo();

			if (temp < 1000)
				tempStr = scene._bgShapes[temp]._description;
			else
				tempStr = scene._bgShapes[temp - 1000]._description;

			_infoFlag = true;
			clearInfo();

			// Only print description if there is one
			if (!tempStr.empty() && tempStr[0] != ' ') {
				// If inventory is active and an item is selected for a Use or Give action
				if ((_menuMode == INV_MODE || _menuMode == USE_MODE || _menuMode == GIVE_MODE) &&
						(inv._invMode == INVMODE_USE || inv._invMode == INVMODE_GIVE)) {
					int width1 = 0, width2 = 0;
					int x, width;
					if (inv._invMode == INVMODE_USE) {
						// Using an object
						x = width = screen.stringWidth("Use ");

						if (temp < 1000 && scene._bgShapes[temp]._aType != PERSON)
							// It's not a person, so make it lowercase
							tempStr.setChar(tolower(tempStr[0]), 0);

						x += screen.stringWidth(tempStr);

						// If we're using an inventory object, add in the width
						// of the object name and the " on "
						if (_selector != -1) {
							width1 = screen.stringWidth(inv[_selector]._name);
							x += width1;
							width2 = screen.stringWidth(" on ");
							x += width2;
						}

						// If the line will be too long, keep cutting off characters
						// until the string will fit
						while (x > 280) {
							x -= screen.charWidth(tempStr.lastChar());
							tempStr.deleteLastChar();
						}

						int xStart = (SHERLOCK_SCREEN_WIDTH - x) / 2;
						screen.print(Common::Point(xStart, INFO_LINE + 1),
							INFO_FOREGROUND, "Use ");

						if (_selector != -1) {
							screen.print(Common::Point(xStart + width, INFO_LINE + 1),
								TALK_FOREGROUND, "%s", inv[_selector]._name.c_str());
							screen.print(Common::Point(xStart + width + width1, INFO_LINE + 1),
								INFO_FOREGROUND, " on ");
							screen.print(Common::Point(xStart + width + width1 + width2, INFO_LINE + 1),
								INFO_FOREGROUND, "%s", tempStr.c_str());
						} else {
							screen.print(Common::Point(xStart + width, INFO_LINE + 1),
								INFO_FOREGROUND, "%s", tempStr.c_str());
						}
					} else if (temp >= 0 && temp < 1000 && _selector != -1 &&
							scene._bgShapes[temp]._aType == PERSON) {
						// Giving an object to a person
						width1 = screen.stringWidth(inv[_selector]._name);
						x = width = screen.stringWidth("Give ");
						x += width1;
						width2 = screen.stringWidth(" to ");
						x += width2;
						x += screen.stringWidth(tempStr);

						// Ensure string will fit on-screen
						while (x > 280) {
							x -= screen.charWidth(tempStr.lastChar());
							tempStr.deleteLastChar();
						}

						int xStart = (SHERLOCK_SCREEN_WIDTH - x) / 2;
						screen.print(Common::Point(xStart, INFO_LINE + 1),
							INFO_FOREGROUND, "Give ");
						screen.print(Common::Point(xStart + width, INFO_LINE + 1),
							TALK_FOREGROUND, "%s", inv[_selector]._name.c_str());
						screen.print(Common::Point(xStart + width + width1, INFO_LINE + 1),
							INFO_FOREGROUND, " to ");
						screen.print(Common::Point(xStart + width + width1 + width2, INFO_LINE + 1),
							INFO_FOREGROUND, "%s", tempStr.c_str());
					}
				} else {
					screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "%s", tempStr.c_str());
				}

				_infoFlag = true;
				_oldLook = temp;
			}
		}
	} else {
		clearInfo();
	}
}

void ScalpelUserInterface::lookInv() {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	Screen &screen = *_vm->_screen;
	Common::Point mousePos = events.mousePos();

	if (mousePos.x > 15 && mousePos.x < 314 && mousePos.y > (CONTROLS_Y1 + 11)
			&& mousePos.y < (SHERLOCK_SCREEN_HEIGHT - 2)) {
		int temp = (mousePos.x - 6) / 52 + inv._invIndex;
		if (temp < inv._holdings) {
			if (temp < inv._holdings) {
				clearInfo();
				screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND,
					"%s", inv[temp]._description.c_str());
				_infoFlag = true;
				_oldLook = temp;
			}
		} else {
			clearInfo();
		}
	} else {
		clearInfo();
	}
}

void ScalpelUserInterface::doEnvControl() {
	Events &events = *_vm->_events;
	SaveManager &saves = *_vm->_saves;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Talk &talk = *_vm->_talk;
	Common::Point mousePos = events.mousePos();
	static const char ENV_COMMANDS[7] = "ELSUDQ";

	byte color;

	_key = _oldKey = -1;
	_keyboardInput = false;
	int found = saves.getHighlightedButton();

	if (events._pressed || events._released) {
		events.clearKeyboard();

		// Check for a filename entry being highlighted
		if ((events._pressed || events._released) && mousePos.y > (CONTROLS_Y + 10)) {
			int found1 = 0;
			for (_selector = 0; (_selector < ONSCREEN_FILES_COUNT) && !found1; ++_selector)
				if (mousePos.y > (CONTROLS_Y + 11 + _selector * 10) && mousePos.y < (CONTROLS_Y + 21 + _selector * 10))
					found1 = 1;

			if (_selector + saves._savegameIndex - 1 < MAX_SAVEGAME_SLOTS + (saves._envMode != SAVEMODE_LOAD))
				_selector = _selector + saves._savegameIndex - 1;
			else
				_selector = -1;

			if (!found1)
				_selector = -1;
		}

		// Handle selecting buttons, if any
		saves.highlightButtons(found);

		if (found == 0 || found == 5)
			saves._envMode = SAVEMODE_NONE;
	}

	if (_keyPress) {
		_key = toupper(_keyPress);

		// Escape _key will close the dialog
		if (_key == Common::KEYCODE_ESCAPE)
			_key = 'E';

		if (_key == 'E' || _key == 'L' || _key == 'S' || _key == 'U' || _key == 'D' || _key == 'Q') {
			const char *chP = strchr(ENV_COMMANDS, _key);
			int btnIndex = !chP ? -1 : chP - ENV_COMMANDS;
			saves.highlightButtons(btnIndex);
			_keyboardInput = true;

			if (_key == 'E' || _key == 'Q') {
				saves._envMode = SAVEMODE_NONE;
			} else if (_key >= '1' && _key <= '9') {
				_keyboardInput = true;
				_selector = _key - '1';
				if (_selector >= MAX_SAVEGAME_SLOTS + (saves._envMode == SAVEMODE_LOAD ? 0 : 1))
					_selector = -1;

				if (saves.checkGameOnScreen(_selector))
					_oldSelector = _selector;
			} else {
				_selector = -1;
			}
		}
	}

	if (_selector != _oldSelector)  {
		if (_oldSelector != -1 && _oldSelector >= saves._savegameIndex && _oldSelector < (saves._savegameIndex + ONSCREEN_FILES_COUNT)) {
			screen.print(Common::Point(6, CONTROLS_Y + 12 + (_oldSelector - saves._savegameIndex) * 10),
				INV_FOREGROUND, "%d.", _oldSelector + 1);
			screen.print(Common::Point(24, CONTROLS_Y + 12 + (_oldSelector - saves._savegameIndex) * 10),
				INV_FOREGROUND, "%s", saves._savegames[_oldSelector].c_str());
		}

		if (_selector != -1) {
			screen.print(Common::Point(6, CONTROLS_Y + 12 + (_selector - saves._savegameIndex) * 10),
				TALK_FOREGROUND, "%d.", _selector + 1);
			screen.print(Common::Point(24, CONTROLS_Y + 12 + (_selector - saves._savegameIndex) * 10),
				TALK_FOREGROUND, "%s", saves._savegames[_selector].c_str());
		}

		_oldSelector = _selector;
	}

	if (events._released || _keyboardInput) {
		if ((found == 0 && events._released) || _key == 'E') {
			banishWindow();
			_windowBounds.top = CONTROLS_Y1;

			events._pressed = events._released = _keyboardInput = false;
			_keyPress = '\0';
		} else if ((found == 1 && events._released) || _key == 'L') {
			saves._envMode = SAVEMODE_LOAD;
			if (_selector != -1) {
				saves.loadGame(_selector + 1);
			}
		} else if ((found == 2 && events._released) || _key == 'S') {
			saves._envMode = SAVEMODE_SAVE;
			if (_selector != -1) {
				if (saves.checkGameOnScreen(_selector))
					_oldSelector = _selector;

				if (saves.promptForDescription(_selector)) {
					saves.saveGame(_selector + 1, saves._savegames[_selector]);

					banishWindow(1);
					_windowBounds.top = CONTROLS_Y1;
					_key = _oldKey = -1;
					_keyPress = '\0';
					_keyboardInput = false;
				} else {
					if (!talk._talkToAbort) {
						screen._backBuffer1.fillRect(Common::Rect(6, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10,
							SHERLOCK_SCREEN_WIDTH - 2, CONTROLS_Y + 20 + (_selector - saves._savegameIndex) * 10), INV_BACKGROUND);
						screen.gPrint(Common::Point(6, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10), INV_FOREGROUND,
							"%d.", _selector + 1);
						screen.gPrint(Common::Point(24, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10), INV_FOREGROUND,
							"%s", saves._savegames[_selector].c_str());

						screen.slamArea(6, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10, 311, 10);
						_selector = _oldSelector = -1;
					}
				}
			}
		} else if (((found == 3 && events._released) || _key == 'U') && saves._savegameIndex) {
			bool moreKeys;
			do {
				saves._savegameIndex--;
				screen._backBuffer1.fillRect(Common::Rect(3, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2,
					SHERLOCK_SCREEN_HEIGHT - 1), INV_BACKGROUND);

				for (int idx = saves._savegameIndex; idx < (saves._savegameIndex + ONSCREEN_FILES_COUNT); ++idx) {
					color = INV_FOREGROUND;
					if (idx == _selector && idx >= saves._savegameIndex && idx < (saves._savegameIndex + ONSCREEN_FILES_COUNT))
						color = TALK_FOREGROUND;

					screen.gPrint(Common::Point(6, CONTROLS_Y + 11 + (idx - saves._savegameIndex) * 10), color, "%d.", idx + 1);
					screen.gPrint(Common::Point(24, CONTROLS_Y + 11 + (idx - saves._savegameIndex) * 10), color, "%s", saves._savegames[idx].c_str());
				}

				screen.slamRect(Common::Rect(3, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2, SHERLOCK_SCREEN_HEIGHT));

				color = !saves._savegameIndex ? COMMAND_NULL : COMMAND_FOREGROUND;
				screen.buttonPrint(Common::Point(ENV_POINTS[3][2], CONTROLS_Y), color, true, "Up");
				color = (saves._savegameIndex == MAX_SAVEGAME_SLOTS - ONSCREEN_FILES_COUNT) ? COMMAND_NULL : COMMAND_FOREGROUND;
				screen.buttonPrint(Common::Point(ENV_POINTS[4][2], CONTROLS_Y), color, true, "Down");

				// Check whether there are more pending U keys pressed
				moreKeys = false;
				if (events.kbHit()) {
					Common::KeyState keyState = events.getKey();

					_key = toupper(keyState.keycode);
					moreKeys = _key == 'U';
				}
			} while ((saves._savegameIndex) && moreKeys);
		} else if (((found == 4 && events._released) || _key == 'D') && saves._savegameIndex < (MAX_SAVEGAME_SLOTS - ONSCREEN_FILES_COUNT)) {
			bool moreKeys;
			do {
				saves._savegameIndex++;
				screen._backBuffer1.fillRect(Common::Rect(3, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2,
					SHERLOCK_SCREEN_HEIGHT - 1), INV_BACKGROUND);

				for (int idx = saves._savegameIndex; idx < (saves._savegameIndex + ONSCREEN_FILES_COUNT); ++idx) {
					if (idx == _selector && idx >= saves._savegameIndex && idx < (saves._savegameIndex + ONSCREEN_FILES_COUNT))
						color = TALK_FOREGROUND;
					else
						color = INV_FOREGROUND;

					screen.gPrint(Common::Point(6, CONTROLS_Y + 11 + (idx - saves._savegameIndex) * 10), color,
						"%d.", idx + 1);
					screen.gPrint(Common::Point(24, CONTROLS_Y + 11 + (idx - saves._savegameIndex) * 10), color,
						"%s", saves._savegames[idx].c_str());
				}

				screen.slamRect(Common::Rect(3, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2, SHERLOCK_SCREEN_HEIGHT));

				color = (!saves._savegameIndex) ? COMMAND_NULL : COMMAND_FOREGROUND;
				screen.buttonPrint(Common::Point(ENV_POINTS[3][2], CONTROLS_Y), color, true, "Up");

				color = (saves._savegameIndex == MAX_SAVEGAME_SLOTS - ONSCREEN_FILES_COUNT) ? COMMAND_NULL : COMMAND_FOREGROUND;
				screen.buttonPrint(Common::Point(ENV_POINTS[4][2], CONTROLS_Y), color, true, "Down");

				// Check whether there are more pending D keys pressed
				moreKeys = false;
				if (events.kbHit()) {
					Common::KeyState keyState;
					_key = toupper(keyState.keycode);

					moreKeys = _key == 'D';
				}
			} while (saves._savegameIndex < (MAX_SAVEGAME_SLOTS - ONSCREEN_FILES_COUNT) && moreKeys);
		} else if ((found == 5 && events._released) || _key == 'Q') {
			clearWindow();
			screen.print(Common::Point(0, CONTROLS_Y + 20), INV_FOREGROUND, "Are you sure you wish to Quit ?");
			screen.vgaBar(Common::Rect(0, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH, CONTROLS_Y + 10), BORDER_COLOR);

			screen.makeButton(Common::Rect(112, CONTROLS_Y, 160, CONTROLS_Y + 10), 136 - screen.stringWidth("Yes") / 2, "Yes");
			screen.makeButton(Common::Rect(161, CONTROLS_Y, 209, CONTROLS_Y + 10), 184 - screen.stringWidth("No") / 2, "No");
			screen.slamArea(112, CONTROLS_Y, 97, 10);

			do {
				scene.doBgAnim();

				if (talk._talkToAbort)
					return;

				events.pollEventsAndWait();
				events.setButtonState();
				mousePos = events.mousePos();

				if (events.kbHit()) {
					Common::KeyState keyState = events.getKey();
					_key = toupper(keyState.keycode);

					if (_key == 'X' && (keyState.flags & Common::KBD_ALT) != 0) {
						_vm->quitGame();
						events.pollEvents();
						return;
					}

					if (_key == Common::KEYCODE_ESCAPE)
						_key = 'N';

					if (_key == Common::KEYCODE_RETURN || _key == ' ') {
						events._pressed = false;
						events._released = true;
						events._oldButtons = 0;
						_keyPress = '\0';
					}
				}

				if (events._pressed || events._released) {
					if (mousePos.x > 112 && mousePos.x < 159 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 9))
						color = COMMAND_HIGHLIGHTED;
					else
						color = COMMAND_FOREGROUND;
					screen.buttonPrint(Common::Point(136, CONTROLS_Y), color, true, "Yes");

					if (mousePos.x > 161 && mousePos.x < 208 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 9))
						color = COMMAND_HIGHLIGHTED;
					else
						color = COMMAND_FOREGROUND;
					screen.buttonPrint(Common::Point(184, CONTROLS_Y), color, true, "No");
				}

				if (mousePos.x > 112 && mousePos.x < 159 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 9) && events._released)
					_key = 'Y';

				if (mousePos.x > 161 && mousePos.x < 208 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 9) && events._released)
					_key = 'N';
			} while (!_vm->shouldQuit() && _key != 'Y' && _key != 'N');

			if (_key == 'Y') {
				_vm->quitGame();
				events.pollEvents();
				return;
			} else {
				screen.buttonPrint(Common::Point(184, CONTROLS_Y), COMMAND_HIGHLIGHTED, true, "No");
				banishWindow(1);
				_windowBounds.top = CONTROLS_Y1;
				_key = -1;
			}
		} else {
			if (_selector != -1) {
				// Are we already in Load mode?
				if (saves._envMode == SAVEMODE_LOAD) {
					saves.loadGame(_selector + 1);
				} else if (saves._envMode == SAVEMODE_SAVE || saves.isSlotEmpty(_selector)) {
					// We're already in save mode, or pointing to an empty save slot
					if (saves.checkGameOnScreen(_selector))
						_oldSelector = _selector;

					if (saves.promptForDescription(_selector)) {
						saves.saveGame(_selector + 1, saves._savegames[_selector]);
						banishWindow();
						_windowBounds.top = CONTROLS_Y1;
						_key = _oldKey = -1;
						_keyPress = '\0';
						_keyboardInput = false;
					} else {
						if (!talk._talkToAbort) {
							screen._backBuffer1.fillRect(Common::Rect(6, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10,
								317, CONTROLS_Y + 20 + (_selector - saves._savegameIndex) * 10), INV_BACKGROUND);
							screen.gPrint(Common::Point(6, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10),
								INV_FOREGROUND, "%d.", _selector + 1);
							screen.gPrint(Common::Point(24, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10),
								INV_FOREGROUND, "%s", saves._savegames[_selector].c_str());
							screen.slamArea(6, CONTROLS_Y + 11 + (_selector - saves._savegameIndex) * 10, 311, 10);
							_selector = _oldSelector = -1;
						}
					}
				}
			}
		}
	}
}

void ScalpelUserInterface::doInvControl() {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Talk &talk = *_vm->_talk;
	int colors[8];
	Common::Point mousePos = events.mousePos();

	_key = _oldKey = -1;
	_keyboardInput = false;

	// Check whether any inventory slot is highlighted
	int found = -1;
	Common::fill(&colors[0], &colors[8], (int)COMMAND_FOREGROUND);
	for (int idx = 0; idx < 8; ++idx) {
		Common::Rect r(INVENTORY_POINTS[idx][0], CONTROLS_Y1,
			INVENTORY_POINTS[idx][1], CONTROLS_Y1 + 10);
		if (r.contains(mousePos)) {
			found = idx;
			break;
		}
	}

	if (events._pressed || events._released) {
		events.clearKeyboard();

		if (found != -1)
			// If a slot highlighted, set its color
			colors[found] = COMMAND_HIGHLIGHTED;
		screen.buttonPrint(Common::Point(INVENTORY_POINTS[0][2], CONTROLS_Y1), colors[0], true, "Exit");

		if (found >= 0 && found <= 3) {
			screen.buttonPrint(Common::Point(INVENTORY_POINTS[1][2], CONTROLS_Y1), colors[1], true, "Look");
			screen.buttonPrint(Common::Point(INVENTORY_POINTS[2][2], CONTROLS_Y1), colors[2], true, "Use");
			screen.buttonPrint(Common::Point(INVENTORY_POINTS[3][2], CONTROLS_Y1), colors[3], true, "Give");
			inv._invMode = (InvMode)found;
			_selector = -1;
		}

		if (inv._invIndex) {
			screen.print(Common::Point(INVENTORY_POINTS[4][2], CONTROLS_Y1 + 1), colors[4], "^^");
			screen.print(Common::Point(INVENTORY_POINTS[5][2], CONTROLS_Y1 + 1), colors[5], "^");
		}

		if ((inv._holdings - inv._invIndex) > 6) {
			screen.print(Common::Point(INVENTORY_POINTS[6][2], CONTROLS_Y1 + 1), colors[6], "_");
			screen.print(Common::Point(INVENTORY_POINTS[7][2], CONTROLS_Y1 + 1), colors[7], "__");
		}

		bool flag = false;
		if (inv._invMode == INVMODE_LOOK || inv._invMode == INVMODE_USE || inv._invMode == INVMODE_GIVE) {
			Common::Rect r(15, CONTROLS_Y1 + 11, 314, SHERLOCK_SCREEN_HEIGHT - 2);
			if (r.contains(mousePos)) {
				_selector = (mousePos.x - 6) / 52 + inv._invIndex;
				if (_selector < inv._holdings)
					flag = true;
			}
		}

		if (!flag && mousePos.y >(CONTROLS_Y1 + 11))
			_selector = -1;
	}

	if (_keyPress) {
		_key = toupper(_keyPress);

		if (_key == Common::KEYCODE_ESCAPE)
			// Escape will also 'E'xit out of inventory display
			_key = 'E';

		if (_key == 'E' || _key == 'L' || _key == 'U' || _key == 'G'
				|| _key == '-' || _key == '+') {
			InvMode temp = inv._invMode;

			const char *chP = strchr(INVENTORY_COMMANDS, _key);
			inv._invMode = !chP ? INVMODE_INVALID : (InvMode)(chP - INVENTORY_COMMANDS);
			inv.invCommands(true);

			inv._invMode = temp;
			_keyboardInput = true;
			if (_key == 'E')
				inv._invMode = INVMODE_EXIT;
			_selector = -1;
		} else {
			_selector = -1;
		}
	}

	if (_selector != _oldSelector) {
		if (_oldSelector != -1) {
			// Un-highlight
			if (_oldSelector >= inv._invIndex && _oldSelector < (inv._invIndex + 6))
				inv.highlight(_oldSelector, BUTTON_MIDDLE);
		}

		if (_selector != -1)
			inv.highlight(_selector, 235);

		_oldSelector = _selector;
	}

	if (events._released || _keyboardInput) {
		if ((found == 0 && events._released) || _key == 'E') {
			inv.freeInv();
			_infoFlag = true;
			clearInfo();
			banishWindow(false);
			_key = -1;
			events.clearEvents();
			events.setCursor(ARROW);
		} else if ((found == 1 && events._released) || (_key == 'L')) {
			inv._invMode = INVMODE_LOOK;
		} else if ((found == 2 && events._released) || (_key == 'U')) {
			inv._invMode = INVMODE_USE;
		} else if ((found == 3 && events._released) || (_key == 'G')) {
			inv._invMode = INVMODE_GIVE;
		} else if (((found == 4 && events._released) || _key == ',') && inv._invIndex) {
			if (inv._invIndex >= 6)
				inv._invIndex -= 6;
			else
				inv._invIndex = 0;

			screen.print(Common::Point(INVENTORY_POINTS[4][2], CONTROLS_Y1 + 1),
				COMMAND_HIGHLIGHTED, "^^");
			inv.freeGraphics();
			inv.loadGraphics();
			inv.putInv(SLAM_DISPLAY);
			inv.invCommands(true);
		} else if (((found == 5 && events._released) || _key == '-') && inv._invIndex > 0) {
			--inv._invIndex;
			screen.print(Common::Point(INVENTORY_POINTS[4][2], CONTROLS_Y1 + 1), COMMAND_HIGHLIGHTED, "^");
			inv.freeGraphics();
			inv.loadGraphics();
			inv.putInv(SLAM_DISPLAY);
			inv.invCommands(true);
		} else if (((found == 6 && events._released) || _key == '+') &&  (inv._holdings - inv._invIndex) > 6) {
			++inv._invIndex;
			screen.print(Common::Point(INVENTORY_POINTS[6][2], CONTROLS_Y1 + 1), COMMAND_HIGHLIGHTED, "_");
			inv.freeGraphics();
			inv.loadGraphics();
			inv.putInv(SLAM_DISPLAY);
			inv.invCommands(true);
		} else if (((found == 7 && events._released) || _key == '.') && (inv._holdings - inv._invIndex) > 6) {
			inv._invIndex += 6;
			if ((inv._holdings - 6) < inv._invIndex)
				inv._invIndex = inv._holdings - 6;

			screen.print(Common::Point(INVENTORY_POINTS[7][2], CONTROLS_Y1 + 1), COMMAND_HIGHLIGHTED, "_");
			inv.freeGraphics();
			inv.loadGraphics();
			inv.putInv(SLAM_DISPLAY);
			inv.invCommands(true);
		} else {
			// If something is being given, make sure it's being given to a person
			if (inv._invMode == INVMODE_GIVE) {
				if (_bgFound != -1 && scene._bgShapes[_bgFound]._aType == PERSON)
					_find = _bgFound;
				else
					_find = -1;
			} else {
				_find = _bgFound;
			}

			if ((mousePos.y < CONTROLS_Y1) && (inv._invMode == INVMODE_LOOK) && (_find >= 0) && (_find < 1000)) {
				if (!scene._bgShapes[_find]._examine.empty() &&
						scene._bgShapes[_find]._examine[0] >= ' ')
					inv.refreshInv();
			} else if (_selector != -1 || _find >= 0) {
				// Selector is the inventory object that was clicked on, or selected.
				// If it's -1, then no inventory item is highlighted yet. Otherwise,
				// an object in the scene has been clicked.

				if (_selector != -1 && inv._invMode == INVMODE_LOOK
						&& mousePos.y >(CONTROLS_Y1 + 11))
					inv.refreshInv();

				if (talk._talkToAbort)
					return;

				// Now check for the Use and Give actions. If inv_mode is INVMODE_GIVE,
				// that means GIVE is in effect, _selector is the object being
				// given, and _find is the target.
				// The same applies to USE, except if _selector is -1, then USE
				// is being tried on an object in the scene without an inventory
				// object being highlighted first.

				if ((inv._invMode == INVMODE_USE || (_selector != -1 && inv._invMode == INVMODE_GIVE)) && _find >= 0) {
					events._pressed = events._released = false;
					_infoFlag = true;
					clearInfo();

					int tempSel = _selector;	// Save the selector
					_selector = -1;

					inv.putInv(SLAM_DISPLAY);
					_selector = tempSel;		// Restore it
					InvMode tempMode = inv._invMode;
					inv._invMode = INVMODE_USE55;
					inv.invCommands(true);

					_infoFlag = true;
					clearInfo();
					banishWindow(false);
					_key = -1;

					inv.freeInv();

					bool giveFl = (tempMode >= INVMODE_GIVE);
					if (_selector >= 0)
						// Use/Give inv object with scene object
						checkUseAction(&scene._bgShapes[_find]._use[0], inv[_selector]._name, MUSE, _find, giveFl);
					else
						// Now inv object has been highlighted
						checkUseAction(&scene._bgShapes[_find]._use[0], "*SELF*", MUSE, _find, giveFl);

					_selector = _oldSelector = -1;
				}
			}
		}
	}
}

void ScalpelUserInterface::doLookControl() {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	Screen &screen = *_vm->_screen;

	_key = _oldKey = -1;
	_keyboardInput = (_keyPress != '\0');

	if (events._released || events._rightReleased || _keyboardInput) {
		// Is an inventory object being looked at?
		if (!_invLookFlag) {
			// Is there any remaining text to display?
			if (!_descStr.empty()) {
				printObjectDesc(_descStr, false);
			} else if (!_lookHelp) {
				// Need to close the window and depress the Look button
				Common::Point pt(MENU_POINTS[0][0], MENU_POINTS[0][1]);
				screen._backBuffer2.blitFrom((*_controls)[0], pt);
				banishWindow(true);

				_windowBounds.top = CONTROLS_Y1;
				_key = _oldKey = COMMANDS[LOOK_MODE - 1];
				_temp = _oldTemp = 0;
				_menuMode = LOOK_MODE;
				events.clearEvents();

				// Restore UI
				drawInterface();
			} else {
				events.setCursor(ARROW);
				banishWindow(true);
				_windowBounds.top = CONTROLS_Y1;
				_key = _oldKey = -1;
				_temp = _oldTemp = 0;
				_menuMode = STD_MODE;
				events.clearEvents();
			}
		} else {
			// Looking at an inventory object
			// Backup the user interface
			Surface tempSurface(SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT - CONTROLS_Y1);
			tempSurface.blitFrom(screen._backBuffer2, Common::Point(0, 0),
				Common::Rect(0, CONTROLS_Y1, SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));

			inv.drawInventory(INVENTORY_DONT_DISPLAY);
			banishWindow(true);

			// Restore the ui
			screen._backBuffer2.blitFrom(tempSurface, Common::Point(0, CONTROLS_Y1));

			_windowBounds.top = CONTROLS_Y1;
			_key = _oldKey = COMMANDS[LOOK_MODE - 1];
			_temp = _oldTemp = 0;
			events.clearEvents();
			_invLookFlag = false;
			_menuMode = INV_MODE;
			_windowOpen = true;
		}
	}
}

void ScalpelUserInterface::doMainControl() {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	SaveManager &saves = *_vm->_saves;
	Common::Point pt = events.mousePos();

	if ((events._pressed || events._released) && pt.y > CONTROLS_Y) {
		events.clearKeyboard();
		_key = -1;

		// Check whether the mouse is in any of the command areas
		for (_temp = 0; (_temp < 12) && (_key == -1); ++_temp) {
			Common::Rect r(MENU_POINTS[_temp][0], MENU_POINTS[_temp][1],
				MENU_POINTS[_temp][2], MENU_POINTS[_temp][3]);
			if (r.contains(pt))
				_key = COMMANDS[_temp];
		}
		--_temp;
	} else if (_keyPress) {
		// Keyboard control
		_keyboardInput = true;

		if (_keyPress >= 'A' && _keyPress <= 'Z') {
			const char *c = strchr(COMMANDS, _keyPress);
			_temp = !c ? 12 : c - COMMANDS;
		} else {
			_temp = 12;
		}

		if (_temp == 12)
			_key = -1;

		if (events._rightPressed) {
			_temp = 12;
			_key = -1;
		}
	} else if (!events._released) {
		_key = -1;
	}

	// Check if the button being pointed to has changed
	if (_oldKey != _key && !_windowOpen) {
		// Clear the info line
		_infoFlag = true;
		clearInfo();

		// If there was an old button selected, restore it
		if (_oldKey != -1) {
			_menuMode = STD_MODE;
			restoreButton(_oldTemp);
		}

		// If a new button is being pointed to, highlight it
		if (_key != -1 && _temp < 12 && !_keyboardInput)
			depressButton(_temp);

		// Save the new button selection
		_oldKey = _key;
		_oldTemp = _temp;
	}

	if (!events._pressed && !_windowOpen) {
		switch (_key) {
		case 'L':
			toggleButton(0);
			break;
		case 'M':
			toggleButton(1);
			break;
		case 'T':
			toggleButton(2);
			break;
		case 'P':
			toggleButton(3);
			break;
		case 'O':
			toggleButton(4);
			break;
		case 'C':
			toggleButton(5);
			break;
		case 'I':
			pushButton(6);
			_selector = _oldSelector = -1;
			_menuMode = INV_MODE;
			inv.drawInventory(PLAIN_INVENTORY);
			break;
		case 'U':
			pushButton(7);
			_selector = _oldSelector = -1;
			_menuMode = USE_MODE;
			inv.drawInventory(USE_INVENTORY_MODE);
			break;
		case 'G':
			pushButton(8);
			_selector = _oldSelector = -1;
			_menuMode = GIVE_MODE;
			inv.drawInventory(GIVE_INVENTORY_MODE);
			break;
		case 'J':
			pushButton(9);
			_menuMode = JOURNAL_MODE;
			journalControl();
			break;
		case 'F':
			pushButton(10);

			// Create a thumbnail of the current screen before the files dialog is shown, in case
			// the user saves the game
			saves.createThumbnail();

			_selector = _oldSelector = -1;

			if (_vm->_showOriginalSavesDialog) {
				// Show the original dialog
				_menuMode = FILES_MODE;
				saves.drawInterface();
				_windowOpen = true;
			} else {
				// Show the ScummVM GMM instead
				_vm->_canLoadSave = true;
				_vm->openMainMenuDialog();
				_vm->_canLoadSave = false;
			}
			break;
		case 'S':
			pushButton(11);
			_menuMode = SETUP_MODE;
			Settings::show(_vm);
			break;
		default:
			break;
		}

		_help = _oldHelp = _oldBgFound = -1;
	}
}

void ScalpelUserInterface::doMiscControl(int allowed) {
	Events &events = *_vm->_events;
	Scene &scene = *_vm->_scene;
	Talk &talk = *_vm->_talk;

	if (events._released) {
		_temp = _bgFound;
		if (_bgFound != -1) {
			// Only allow pointing to objects, not people
			if (_bgFound < 1000) {
				events.clearEvents();
				Object &obj = scene._bgShapes[_bgFound];

				switch (allowed) {
				case ALLOW_OPEN:
					checkAction(obj._aOpen, MOPEN, _temp);
					if (_menuMode != TALK_MODE && !talk._talkToAbort) {
						_menuMode = STD_MODE;
						restoreButton(OPEN_MODE - 1);
						_key = _oldKey = -1;
					}
					break;

				case ALLOW_CLOSE:
					checkAction(obj._aClose, MCLOSE, _temp);
					if (_menuMode != TALK_MODE && !talk._talkToAbort) {
						_menuMode = STD_MODE;
						restoreButton(CLOSE_MODE - 1);
						_key = _oldKey = -1;
					}
					break;

				case ALLOW_MOVE:
					checkAction(obj._aMove, MMOVE, _temp);
					if (_menuMode != TALK_MODE && !talk._talkToAbort) {
						_menuMode = STD_MODE;
						restoreButton(MOVE_MODE - 1);
						_key = _oldKey = -1;
					}
					break;

				default:
					break;
				}
			}
		}
	}
}

void ScalpelUserInterface::doPickControl() {
	Events &events = *_vm->_events;
	Scene &scene = *_vm->_scene;
	Talk &talk = *_vm->_talk;

	if (events._released) {
		if ((_temp = _bgFound) != -1) {
			events.clearEvents();

			// Don't allow characters to be picked up
			if (_bgFound < 1000) {
				scene._bgShapes[_bgFound].pickUpObject(MPICK);

				if (!talk._talkToAbort && _menuMode != TALK_MODE) {
					_key = _oldKey = -1;
					_menuMode = STD_MODE;
					restoreButton(PICKUP_MODE - 1);
				}
			}
		}
	}
}

void ScalpelUserInterface::doTalkControl() {
	Events &events = *_vm->_events;
	Journal &journal = *_vm->_journal;
	People &people = *_vm->_people;
	Screen &screen = *_vm->_screen;
	Sound &sound = *_vm->_sound;
	Talk &talk = *_vm->_talk;
	Common::Point mousePos = events.mousePos();

	_key = _oldKey = -1;
	_keyboardInput = false;

	if (events._pressed || events._released) {
		events.clearKeyboard();

		// Handle button printing
		if (mousePos.x > 99 && mousePos.x < 138 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 10) && !_endKeyActive)
			screen.buttonPrint(Common::Point(119, CONTROLS_Y), COMMAND_HIGHLIGHTED, true, "Exit");
		else if (_endKeyActive)
			screen.buttonPrint(Common::Point(119, CONTROLS_Y), COMMAND_FOREGROUND, true, "Exit");

		if (mousePos.x > 140 && mousePos.x < 170 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 10) && talk._moreTalkUp)
			screen.buttonPrint(Common::Point(159, CONTROLS_Y), COMMAND_HIGHLIGHTED, true, "Up");
		else if (talk._moreTalkUp)
			screen.buttonPrint(Common::Point(159, CONTROLS_Y), COMMAND_FOREGROUND, true, "Up");

		if (mousePos.x > 181&& mousePos.x < 220 && mousePos.y > CONTROLS_Y && mousePos.y < (CONTROLS_Y + 10) && talk._moreTalkDown)
			screen.buttonPrint(Common::Point(200, CONTROLS_Y), COMMAND_HIGHLIGHTED, true, "Down");
		else if (talk._moreTalkDown)
			screen.buttonPrint(Common::Point(200, CONTROLS_Y), COMMAND_FOREGROUND, true, "Down");

		bool found = false;
		for (_selector = talk._talkIndex; _selector < (int)talk._statements.size() && !found; ++_selector) {
			if (mousePos.y > talk._statements[_selector]._talkPos.top &&
					mousePos.y < talk._statements[_selector]._talkPos.bottom)
				found = true;
		}
		--_selector;
		if (!found)
			_selector = -1;
	}

	if (_keyPress) {
		_key = toupper(_keyPress);
		if (_key == Common::KEYCODE_ESCAPE)
			_key = 'E';

		// Check for number press indicating reply line
		if (_key >= '1' && _key <= ('1' + (int)talk._statements.size() - 1)) {
			for (uint idx = 0; idx < talk._statements.size(); ++idx) {
				if (talk._statements[idx]._talkMap == (_key - '1')) {
					// Found the given statement
					_selector = idx;
					_key = -1;
					_keyboardInput = true;
					break;
				}
			}
		} else if (_key == 'E' || _key == 'U' || _key == 'D') {
			_keyboardInput = true;
		} else {
			_selector = -1;
		}
	}

	if (_selector != _oldSelector) {
		// Remove highlighting from previous line, if any
		if (_oldSelector != -1) {
			if (!((talk._talkHistory[talk._converseNum][_oldSelector] >> (_oldSelector & 7)) & 1))
				talk.talkLine(_oldSelector, talk._statements[_oldSelector]._talkMap, INV_FOREGROUND,
					talk._statements[_oldSelector]._talkPos.top, true);
			else
				talk.talkLine(_oldSelector, talk._statements[_oldSelector]._talkMap, TALK_NULL,
					talk._statements[_oldSelector]._talkPos.top, true);
		}

		// Add highlighting to new line, if any
		if (_selector != -1)
			talk.talkLine(_selector, talk._statements[_selector]._talkMap, TALK_FOREGROUND,
				talk._statements[_selector]._talkPos.top, true);

		_oldSelector = _selector;
	}

	if (events._released || _keyboardInput) {
		if (((Common::Rect(99, CONTROLS_Y, 138, CONTROLS_Y + 10).contains(mousePos) && events._released)
				|| _key == 'E') && _endKeyActive) {
			talk.freeTalkVars();
			talk.pullSequence();

			drawInterface(2);
			banishWindow();
			_windowBounds.top = CONTROLS_Y1;
		} else if (((Common::Rect(140, CONTROLS_Y, 179, CONTROLS_Y + 10).contains(mousePos) && events._released)
				|| _key == 'U') && talk._moreTalkUp) {
			while (talk._statements[--talk._talkIndex]._talkMap == -1)
				;
			screen._backBuffer1.fillRect(Common::Rect(5, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2,
				SHERLOCK_SCREEN_HEIGHT - 1), INV_BACKGROUND);
			talk.displayTalk(false);

			screen.slamRect(Common::Rect(5, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH - 5, SHERLOCK_SCREEN_HEIGHT - 2));
		} else if (((Common::Rect(181, CONTROLS_Y, 220, CONTROLS_Y + 10).contains(mousePos) && events._released)
				|| _key == 'D') && talk._moreTalkDown) {
			do {
				++talk._talkIndex;
			} while (talk._talkIndex < (int)talk._statements.size() && talk._statements[talk._talkIndex]._talkMap == -1);

			screen._backBuffer1.fillRect(Common::Rect(5, CONTROLS_Y + 11, SHERLOCK_SCREEN_WIDTH - 2,
				SHERLOCK_SCREEN_HEIGHT - 1), INV_BACKGROUND);
			talk.displayTalk(false);

			screen.slamRect(Common::Rect(5, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH - 5, SHERLOCK_SCREEN_HEIGHT - 2));
		} else if (_selector != -1) {
			screen.buttonPrint(Common::Point(119, CONTROLS_Y), COMMAND_NULL, true, "Exit");
			screen.buttonPrint(Common::Point(159, CONTROLS_Y), COMMAND_NULL, true, "Up");
			screen.buttonPrint(Common::Point(200, CONTROLS_Y), COMMAND_NULL, true, "Down");

			// If the reply is new, add it to the journal
			if (!talk._talkHistory[talk._converseNum][_selector]) {
				journal.record(talk._converseNum, _selector);

				// Add any Holmes point to Holmes' total, if any
				if (talk._statements[_selector]._quotient)
					people._holmesQuotient += talk._statements[_selector]._quotient;
			}

			// Flag the response as having been used
			talk._talkHistory[talk._converseNum][_selector] = true;

			clearWindow();
			screen.print(Common::Point(16, CONTROLS_Y + 12), TALK_FOREGROUND, "Sherlock Holmes");
			talk.talkLine(_selector + 128, talk._statements[_selector]._talkMap, COMMAND_FOREGROUND, CONTROLS_Y + 21, true);

			switch (talk._statements[_selector]._portraitSide & 3) {
			case 0:
			case 1:
				people._portraitSide = 20;
				break;
			case 2:
				people._portraitSide = 220;
				break;
			case 3:
				people._portraitSide = 120;
				break;
			}

			// Check for flipping Holmes
			if (talk._statements[_selector]._portraitSide & REVERSE_DIRECTION)
				people._holmesFlip = true;

			talk._speaker = 0;
			people.setTalking(0);

			if (!talk._statements[_selector]._voiceFile.empty() && sound._voices) {
				sound.playSound(talk._statements[_selector]._voiceFile, WAIT_RETURN_IMMEDIATELY);

				// Set voices as an indicator for waiting
				sound._voices = 2;
				sound._speechOn = *sound._soundIsOn;
			} else {
				sound._speechOn = false;
			}

			talk.waitForMore(talk._statements[_selector]._statement.size());
			if (talk._talkToAbort)
				return;

			people.clearTalking();
			if (talk._talkToAbort)
				return;

			while (!_vm->shouldQuit()) {
				talk._scriptSelect = _selector;
				talk._speaker = talk._talkTo;
				talk.doScript(talk._statements[_selector]._reply);

				if (!talk._talkToAbort) {
					if (!talk._talkStealth)
						clearWindow();

					if (!talk._statements[_selector]._modified.empty()) {
						for (uint idx = 0; idx < talk._statements[_selector]._modified.size(); ++idx) {
							_vm->setFlags(talk._statements[_selector]._modified[idx]);
						}

						talk.setTalkMap();
					}

					// Check for another linked talk file
					Common::String linkFilename = talk._statements[_selector]._linkFile;
					if (!linkFilename.empty() && !talk._scriptMoreFlag) {
						talk.freeTalkVars();
						talk.loadTalkFile(linkFilename);

						// Find the first new statement
						int select = _selector = _oldSelector = -1;
						for (uint idx = 0; idx < talk._statements.size() && select == -1; ++idx) {
							if (!talk._statements[idx]._talkMap)
								select = talk._talkIndex = idx;
						}

						// See if the new statement is a stealth reply
						talk._talkStealth = talk._statements[select]._statement.hasPrefix("^") ? 2 : 0;

						// Is the new talk file a standard file, reply first file, or a stealth file
						if (!talk._statements[select]._statement.hasPrefix("*") &&
								!talk._statements[select]._statement.hasPrefix("^")) {
							// Not a reply first file, so display the new selections
							if (_endKeyActive)
								screen.buttonPrint(Common::Point(119, CONTROLS_Y), COMMAND_FOREGROUND, true, "Exit");
							else
								screen.buttonPrint(Common::Point(119, CONTROLS_Y), COMMAND_NULL, true, "Exit");

							talk.displayTalk(true);
							events.setCursor(ARROW);
							break;
						} else {
							_selector = select;

							if (!talk._talkHistory[talk._converseNum][_selector])
								journal.record(talk._converseNum, _selector);

							talk._talkHistory[talk._converseNum][_selector] = true;
						}
					} else {
						talk.freeTalkVars();
						talk.pullSequence();
						banishWindow();
						_windowBounds.top = CONTROLS_Y1;
						break;
					}
				} else {
					break;
				}
			}

			events._pressed = events._released = false;
			events._oldButtons = 0;
			talk._talkStealth = 0;

			// If a script was pushed onto the script stack, restore it
			if (!talk._scriptStack.empty()) {
				ScriptStackEntry stackEntry = talk._scriptStack.pop();
				talk._scriptName = stackEntry._name;
				talk._scriptSaveIndex = stackEntry._currentIndex;
				talk._scriptSelect = stackEntry._select;
			}
		}
	}
}

void ScalpelUserInterface::journalControl() {
	Events &events = *_vm->_events;
	Journal &journal = *_vm->_journal;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	bool doneFlag = false;

	// Draw the journal screen
	journal.drawInterface();

	// Handle journal events
	do {
		_key = -1;
		events.setButtonState();

		// Handle keypresses
		if (events.kbHit()) {
			Common::KeyState keyState = events.getKey();
			if (keyState.keycode == Common::KEYCODE_x && (keyState.flags & Common::KBD_ALT)) {
				_vm->quitGame();
				return;
			} else if (keyState.keycode == Common::KEYCODE_e || keyState.keycode == Common::KEYCODE_ESCAPE) {
				doneFlag = true;
			} else {
				_key = toupper(keyState.keycode);
			}
		}

		if (!doneFlag)
			doneFlag = journal.handleEvents(_key);
	} while (!_vm->shouldQuit() && !doneFlag);

	// Finish up
	_infoFlag = _keyboardInput = false;
	_keyPress = '\0';
	_windowOpen = false;
	_windowBounds.top = CONTROLS_Y1;
	_key = -1;
	_menuMode = STD_MODE;

	// Reset the palette
	screen.setPalette(screen._cMap);

	screen._backBuffer1.blitFrom(screen._backBuffer2);
	scene.updateBackground();
	screen.slamArea(0, 0, SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT);
}

void ScalpelUserInterface::printObjectDesc(const Common::String &str, bool firstTime) {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	Screen &screen = *_vm->_screen;
	Talk &talk = *_vm->_talk;

	if (str.hasPrefix("_")) {
		_lookScriptFlag = true;
		events.setCursor(MAGNIFY);
		int savedSelector = _selector;
		talk.talkTo(str.c_str() + 1);
		_lookScriptFlag = false;

		if (talk._talkToAbort) {
			events.setCursor(ARROW);
			return;
		}

		// Check if looking at an inventory object
		if (!_invLookFlag) {
			// See if this look was called by a right button click or not
			if (!_lookHelp) {
				// If it wasn't a right button click, then we need depress
				// the look button before we close the window. So save a copy of the
				// menu area, and draw the controls onto it
				Surface tempSurface((*_controls)[0]._frame.w, (*_controls)[0]._frame.h);
				Common::Point pt(MENU_POINTS[0][0], MENU_POINTS[0][1]);

				tempSurface.blitFrom(screen._backBuffer2, Common::Point(0, 0),
					Common::Rect(pt.x, pt.y, pt.x + tempSurface.w(), pt.y + tempSurface.h()));
				screen._backBuffer2.transBlitFrom((*_controls)[0], pt);

				banishWindow(1);
				events.setCursor(MAGNIFY);
				_windowBounds.top = CONTROLS_Y1;
				_key = _oldKey = COMMANDS[LOOK_MODE - 1];
				_temp = _oldTemp = 0;
				_menuMode = LOOK_MODE;
				events.clearEvents();

				screen._backBuffer2.blitFrom(tempSurface, pt);
			} else {
				events.setCursor(ARROW);
				banishWindow(true);
				_windowBounds.top = CONTROLS_Y1;
				_key = _oldKey = -1;
				_temp = _oldTemp = 0;
				_menuMode = STD_MODE;
				_lookHelp = 0;
				events.clearEvents();
			}
		} else {
			// Looking at an inventory object
			_selector = _oldSelector = savedSelector;

			// Reload the inventory graphics and draw the inventory
			inv.loadInv();
			inv.putInv(SLAM_SECONDARY_BUFFER);
			inv.freeInv();
			banishWindow(1);

			_windowBounds.top = CONTROLS_Y1;
			_key = _oldKey = COMMANDS[INV_MODE - 1];
			_temp = _oldTemp = 0;
			events.clearEvents();

			_invLookFlag = 0;
			_menuMode = INV_MODE;
			_windowOpen = true;
		}

		return;
	}

	Surface &bb = *screen._backBuffer;
	if (firstTime) {
		// Only draw the border on the first call
		_infoFlag = true;
		clearInfo();

		bb.fillRect(Common::Rect(0, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH,
			CONTROLS_Y1 + 10), BORDER_COLOR);
		bb.fillRect(Common::Rect(0, CONTROLS_Y + 10, 1, SHERLOCK_SCREEN_HEIGHT - 1),
			BORDER_COLOR);
		bb.fillRect(Common::Rect(SHERLOCK_SCREEN_WIDTH - 2, CONTROLS_Y + 10,
			SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT), BORDER_COLOR);
		bb.fillRect(Common::Rect(0, SHERLOCK_SCREEN_HEIGHT - 1, SHERLOCK_SCREEN_WIDTH,
			SHERLOCK_SCREEN_HEIGHT), BORDER_COLOR);
	}

	// Clear background
	bb.fillRect(Common::Rect(2, CONTROLS_Y + 10, SHERLOCK_SCREEN_WIDTH - 2,
		SHERLOCK_SCREEN_HEIGHT - 2), INV_BACKGROUND);

	_windowBounds.top = CONTROLS_Y;
	events.clearEvents();

	// Loop through displaying up to five lines
	bool endOfStr = false;
	const char *msgP = str.c_str();
	for (int lineNum = 0; lineNum < ONSCREEN_FILES_COUNT && !endOfStr; ++lineNum) {
		int width = 0;
		const char *lineStartP = msgP;

		// Determine how much can be displayed on the line
		do {
			width += screen.charWidth(*msgP++);
		} while (width < 300 && *msgP);

		if (*msgP)
			--msgP;
		else
			endOfStr = true;

		// If the line needs to be wrapped, scan backwards to find
		// the end of the previous word as a splitting point
		if (width >= 300) {
			while (*msgP != ' ')
				--msgP;
			endOfStr = false;
		}

		// Print out the line
		Common::String line(lineStartP, msgP);
		screen.gPrint(Common::Point(16, CONTROLS_Y + 12 + lineNum * 9),
			INV_FOREGROUND, "%s", line.c_str());

		if (!endOfStr)
			// Start next line at start of the nxet word after space
			++msgP;
	}

	// Handle display depending on whether all the message was shown
	if (!endOfStr) {
		screen.makeButton(Common::Rect(46, CONTROLS_Y, 272, CONTROLS_Y + 10),
			(SHERLOCK_SCREEN_WIDTH - screen.stringWidth(PRESS_KEY_FOR_MORE)) / 2,
			PRESS_KEY_FOR_MORE);
		screen.gPrint(Common::Point((SHERLOCK_SCREEN_WIDTH -
			screen.stringWidth(PRESS_KEY_FOR_MORE)) / 2, CONTROLS_Y),
			COMMAND_FOREGROUND, "P");
		_descStr = msgP;
	} else {
		screen.makeButton(Common::Rect(46, CONTROLS_Y, 272, CONTROLS_Y + 10),
			(SHERLOCK_SCREEN_WIDTH - screen.stringWidth(PRESS_KEY_TO_CONTINUE)) / 2,
			PRESS_KEY_TO_CONTINUE);
		screen.gPrint(Common::Point((SHERLOCK_SCREEN_WIDTH -
			screen.stringWidth(PRESS_KEY_TO_CONTINUE)) / 2, CONTROLS_Y),
			COMMAND_FOREGROUND, "P");
		_descStr = "";
	}

	if (firstTime) {
		if (!_slideWindows) {
			screen.slamRect(Common::Rect(0, CONTROLS_Y,
				SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));
		} else {
			// Display the window
			summonWindow();
		}

		_selector = _oldSelector = -1;
		_windowOpen = true;
	} else {
		screen.slamRect(Common::Rect(0, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH,
			SHERLOCK_SCREEN_HEIGHT));
	}
}

void ScalpelUserInterface::printObjectDesc() {
	printObjectDesc(_cAnimStr, true);
}

void ScalpelUserInterface::summonWindow(const Surface &bgSurface, bool slideUp) {
	Events &events = *_vm->_events;
	Screen &screen = *_vm->_screen;

	if (_windowOpen)
		// A window is already open, so can't open another one
		return;

	if (slideUp) {
		// Gradually slide up the display of the window
		for (int idx = 1; idx <= bgSurface.h(); idx += 2) {
			screen._backBuffer->blitFrom(bgSurface, Common::Point(0, SHERLOCK_SCREEN_HEIGHT - idx),
				Common::Rect(0, 0, bgSurface.w(), idx));
			screen.slamRect(Common::Rect(0, SHERLOCK_SCREEN_HEIGHT - idx,
				SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));

			events.delay(10);
		}
	} else {
		// Gradually slide down the display of the window
		for (int idx = 1; idx <= bgSurface.h(); idx += 2) {
			screen._backBuffer->blitFrom(bgSurface,
				Common::Point(0, SHERLOCK_SCREEN_HEIGHT - bgSurface.h()),
				Common::Rect(0, bgSurface.h() - idx, bgSurface.w(), bgSurface.h()));
			screen.slamRect(Common::Rect(0, SHERLOCK_SCREEN_HEIGHT - bgSurface.h(),
				SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT - bgSurface.h() + idx));

			events.delay(10);
		}
	}

	// Final display of the entire window
	screen._backBuffer->blitFrom(bgSurface, Common::Point(0, SHERLOCK_SCREEN_HEIGHT - bgSurface.h()),
		Common::Rect(0, 0, bgSurface.w(), bgSurface.h()));
	screen.slamArea(0, SHERLOCK_SCREEN_HEIGHT - bgSurface.h(), bgSurface.w(), bgSurface.h());

	_windowOpen = true;
}

void ScalpelUserInterface::summonWindow(bool slideUp, int height) {
	Screen &screen = *_vm->_screen;

	// Extract the window that's been drawn on the back buffer
	Surface tempSurface(SHERLOCK_SCREEN_WIDTH,
		(SHERLOCK_SCREEN_HEIGHT - height));
	Common::Rect r(0, height, SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT);
	tempSurface.blitFrom(screen._backBuffer1, Common::Point(0, 0), r);

	// Remove drawn window with original user interface
	screen._backBuffer1.blitFrom(screen._backBuffer2,
		Common::Point(0, height), r);

	// Display the window gradually on-screen
	summonWindow(tempSurface, slideUp);
}

void ScalpelUserInterface::banishWindow(bool slideUp) {
	Events &events = *_vm->_events;
	Screen &screen = *_vm->_screen;

	if (_windowOpen) {
		if (slideUp || !_slideWindows) {
			// Slide window down
			// Only slide the window if the window style allows it
			if (_slideWindows) {
				for (int idx = 2; idx < (SHERLOCK_SCREEN_HEIGHT - CONTROLS_Y); idx += 2) {
					// Shift the window down by 2 lines
					byte *pSrc = (byte *)screen._backBuffer1.getBasePtr(0, CONTROLS_Y + idx - 2);
					byte *pSrcEnd = (byte *)screen._backBuffer1.getBasePtr(0, SHERLOCK_SCREEN_HEIGHT - 2);
					byte *pDest = (byte *)screen._backBuffer1.getBasePtr(0, SHERLOCK_SCREEN_HEIGHT);
					Common::copy_backward(pSrc, pSrcEnd, pDest);

					// Restore lines from the ui in the secondary back buffer
					screen._backBuffer1.blitFrom(screen._backBuffer2,
						Common::Point(0, CONTROLS_Y),
						Common::Rect(0, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH, CONTROLS_Y + idx));

					screen.slamArea(0, CONTROLS_Y + idx - 2, SHERLOCK_SCREEN_WIDTH,
						SHERLOCK_SCREEN_HEIGHT - CONTROLS_Y - idx + 2);
					events.delay(10);
				}

				// Restore final two old lines
				screen._backBuffer1.blitFrom(screen._backBuffer2,
					Common::Point(0, SHERLOCK_SCREEN_HEIGHT - 2),
					Common::Rect(0, SHERLOCK_SCREEN_HEIGHT - 2,
						SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));
				screen.slamArea(0, SHERLOCK_SCREEN_HEIGHT - 2, SHERLOCK_SCREEN_WIDTH, 2);
			} else {
				// Restore old area to completely erase window
				screen._backBuffer1.blitFrom(screen._backBuffer2,
					Common::Point(0, CONTROLS_Y),
					Common::Rect(0, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));
				screen.slamRect(Common::Rect(0, CONTROLS_Y, SHERLOCK_SCREEN_WIDTH,
					SHERLOCK_SCREEN_HEIGHT));
			}
		} else {
			// Slide the original user interface up to cover the dialog
			for (int idx = 1; idx < (SHERLOCK_SCREEN_HEIGHT - CONTROLS_Y1); idx += 2) {
				byte *pSrc = (byte *)screen._backBuffer2.getBasePtr(0, CONTROLS_Y1);
				byte *pSrcEnd = (byte *)screen._backBuffer2.getBasePtr(0, CONTROLS_Y1 + idx);
				byte *pDest = (byte *)screen._backBuffer1.getBasePtr(0, SHERLOCK_SCREEN_HEIGHT - idx);
				Common::copy(pSrc, pSrcEnd, pDest);

				screen.slamArea(0, SHERLOCK_SCREEN_HEIGHT - idx, SHERLOCK_SCREEN_WIDTH,
					SHERLOCK_SCREEN_HEIGHT);
				events.delay(10);
			}

			// Show entire final area
			screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(0, CONTROLS_Y1),
				Common::Rect(0, CONTROLS_Y1, SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));
			screen.slamRect(Common::Rect(0, CONTROLS_Y1, SHERLOCK_SCREEN_WIDTH, SHERLOCK_SCREEN_HEIGHT));
		}

		_infoFlag = false;
		_windowOpen = false;
	}

	_menuMode = STD_MODE;
}

void ScalpelUserInterface::checkUseAction(const UseType *use, const Common::String &invName,
		const char *const messages[], int objNum, bool giveMode) {
	Events &events = *_vm->_events;
	Inventory &inv = *_vm->_inventory;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Talk &talk = *_vm->_talk;
	bool printed = messages == nullptr;

	if (objNum >= 1000) {
		// Holmes was specified, so do nothing
		_infoFlag = true;
		clearInfo();
		_infoFlag = true;

		// Display error message
		_menuCounter = 30;
		screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "You can't do that to yourself.");
		return;
	}

	// Scan for target item
	int targetNum = -1;
	if (giveMode) {
		for (int idx = 0; idx < USE_COUNT && targetNum == -1; ++idx) {
			if ((use[idx]._target.equalsIgnoreCase("*GIVE*") || use[idx]._target.equalsIgnoreCase("*GIVEP*"))
					&& use[idx]._names[0].equalsIgnoreCase(invName)) {
				// Found a match
				targetNum = idx;
				if (use[idx]._target.equalsIgnoreCase("*GIVE*"))
					inv.deleteItemFromInventory(invName);
			}
		}
	} else {
		for (int idx = 0; idx < USE_COUNT && targetNum == -1; ++idx) {
			if (use[idx]._target.equalsIgnoreCase(invName))
				targetNum = idx;
		}
	}

	if (targetNum != -1) {
		// Found a target, so do the action
		const UseType &action = use[targetNum];

		events.setCursor(WAIT);

		if (action._useFlag)
			_vm->setFlags(action._useFlag);

		if (action._cAnimNum != 99) {
			if (action._cAnimNum == 0)
				scene.startCAnim(9, action._cAnimSpeed);
			else
				scene.startCAnim(action._cAnimNum - 1, action._cAnimSpeed);
		}

		if (!talk._talkToAbort) {
			Object &obj = scene._bgShapes[objNum];
			for (int idx = 0; idx < NAMES_COUNT && !talk._talkToAbort; ++idx) {
				if (obj.checkNameForCodes(action._names[idx], messages)) {
					if (!talk._talkToAbort)
						printed = true;
				}
			}

			// Print "Done..." as an ending, unless flagged for leaving scene or otherwise flagged
			if (scene._goToScene != 1 && !printed && !talk._talkToAbort) {
				_infoFlag = true;
				clearInfo();
				screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "Done...");
				_menuCounter = 25;
			}
		}
	} else {
		// Couldn't find target, so print error
		_infoFlag = true;
		clearInfo();

		if (giveMode) {
			screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "No, thank you.");
		} else if (messages == nullptr) {
			screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "You can't do that.");
		} else {
			screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "%s", messages[0]);
		}

		_infoFlag = true;
		_menuCounter = 30;
	}

	events.setCursor(ARROW);
}

void ScalpelUserInterface::checkAction(ActionType &action, const char *const messages[], int objNum) {
	Events &events = *_vm->_events;
	People &people = *_vm->_people;
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Talk &talk = *_vm->_talk;
	Common::Point pt(-1, -1);

	if (objNum >= 1000)
		// Ignore actions done on characters
		return;

	if (!action._cAnimSpeed) {
		// Invalid action, to print error message
		_infoFlag = true;
		clearInfo();
		screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "%s", messages[action._cAnimNum]);
		_infoFlag = true;

		// Set how long to show the message
		_menuCounter = 30;
	} else {
		Object &obj = scene._bgShapes[objNum];

		int cAnimNum;
		if (action._cAnimNum == 0)
			// Really a 10
			cAnimNum = 9;
		else
			cAnimNum = action._cAnimNum - 1;

		int dir = -1;
		if (action._cAnimNum != 99) {
			CAnim &anim = scene._cAnim[cAnimNum];

			if (action._cAnimNum != 99) {
				if (action._cAnimSpeed & REVERSE_DIRECTION) {
					pt = anim._teleportPos;
					dir = anim._teleportDir;
				} else {
					pt = anim._goto;
					dir = anim._gotoDir;
				}
			}
		} else {
			pt = Common::Point(-1, -1);
			dir = -1;
		}

		// Has a value, so do action
		// Show wait cursor whilst walking to object and doing action
		events.setCursor(WAIT);
		bool printed = false;

		for (int nameIdx = 0; nameIdx < NAMES_COUNT; ++nameIdx) {
			if (action._names[nameIdx].hasPrefix("*") && action._names[nameIdx].size() >= 2
					&& toupper(action._names[nameIdx][1]) == 'W') {
				if (obj.checkNameForCodes(Common::String(action._names[nameIdx].c_str() + 2), messages)) {
					if (!talk._talkToAbort)
						printed = true;
				}
			}
		}

		bool doCAnim = true;
		for (int nameIdx = 0; nameIdx < NAMES_COUNT; ++nameIdx) {
			if (action._names[nameIdx].hasPrefix("*") && action._names[nameIdx].size() >= 2) {
				char ch = toupper(action._names[nameIdx][1]);

				if (ch == 'T' || ch == 'B') {
					printed = true;
					if (pt.x != -1)
						// Holmes needs to walk to object before the action is done
						people.walkToCoords(pt, dir);

					if (!talk._talkToAbort) {
						// Ensure Holmes is on the exact intended location
						people[AL]._position = pt;
						people[AL]._sequenceNumber = dir;
						people.gotoStand(people[AL]);

						talk.talkTo(action._names[nameIdx].c_str() + 2);
						if (ch == 'T')
							doCAnim = false;
					}
				}
			}
		}

		if (doCAnim && !talk._talkToAbort) {
			if (pt.x != -1)
				// Holmes needs to walk to object before the action is done
				people.walkToCoords(pt, dir);
		}

		for (int nameIdx = 0; nameIdx < NAMES_COUNT; ++nameIdx) {
			if (action._names[nameIdx].hasPrefix("*") && action._names[nameIdx].size() >= 2
					&& toupper(action._names[nameIdx][1]) == 'F') {
				if (obj.checkNameForCodes(action._names[nameIdx].c_str() + 2, messages)) {
					if (!talk._talkToAbort)
						printed = true;
				}
			}
		}

		if (doCAnim && !talk._talkToAbort && action._cAnimNum != 99)
			scene.startCAnim(cAnimNum, action._cAnimSpeed);

		if (!talk._talkToAbort) {
			for (int nameIdx = 0; nameIdx < NAMES_COUNT && !talk._talkToAbort; ++nameIdx) {
				if (obj.checkNameForCodes(action._names[nameIdx], messages)) {
					if (!talk._talkToAbort)
						printed = true;
				}
			}

			// Unless we're leaving the scene, print a "Done" message unless the printed flag has been set
			if (scene._goToScene != 1 && !printed && !talk._talkToAbort) {
				_infoFlag = true;
				clearInfo();
				screen.print(Common::Point(0, INFO_LINE + 1), INFO_FOREGROUND, "Done...");

				// Set how long to show the message
				_menuCounter = 30;
			}
		}
	}

	// Reset cursor back to arrow
	events.setCursor(ARROW);
}

} // End of namespace Scalpel

} // End of namespace Sherlock
