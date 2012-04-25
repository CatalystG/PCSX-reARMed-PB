/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#ifndef __LINUX_H__
#define __LINUX_H__

#include "config.h"

#define DEFAULT_MEM_CARD_1 "shared/misc/pcsx-rearmed-pb/memcards/card1.mcd"
#define DEFAULT_MEM_CARD_2 "shared/misc/pcsx-rearmed-pb/memcards/card2.mcd"
#define MEMCARD_DIR "shared/misc/pcsx-rearmed-pb/memcards/"
#define PLUGINS_DIR "shared/misc/pcsx-rearmed-pb/plugins/"
#define PLUGINS_CFG_DIR "shared/misc/pcsx-rearmed-pb/plugins/cfg/"
#define PCSX_DOT_DIR "shared/misc/pcsx-rearmed-pb/"
#define STATES_DIR "shared/misc/pcsx-rearmed-pb/sstates/"
#define CHEATS_DIR "shared/misc/pcsx-rearmed-pb/cheats/"
#define PATCHES_DIR "shared/misc/pcsx-rearmed-pb/patches/"
#define BIOS_DIR "shared/misc/pcsx-rearmed-pb/bios/"
#define ISO_DIR "shared/misc/pcsx-rearmed-pb/iso/"

extern char cfgfile_basename[MAXPATHLEN];

extern int state_slot;
void emu_set_default_config(void);
int get_state_filename(char *buf, int size, int i);
int emu_check_state(int slot);
int emu_save_state(int slot);
int emu_load_state(int slot);

void set_cd_image(const char *fname);
void load_newiso(void);

extern unsigned long gpuDisp;
extern int ready_to_go;

extern char hud_msg[64];
extern int hud_new_msg;

enum sched_action {
	SACTION_NONE,
	SACTION_ENTER_MENU,
	SACTION_LOAD_STATE,
	SACTION_SAVE_STATE,
	SACTION_NEXT_SSLOT,
	SACTION_PREV_SSLOT,
	SACTION_TOGGLE_FSKIP,
	SACTION_SCREENSHOT,
	SACTION_VOLUME_UP,
	SACTION_VOLUME_DOWN,
	SACTION_MINIMIZE,
	SACTION_GUN_TRIGGER = 16,
	SACTION_GUN_A,
	SACTION_GUN_B,
	SACTION_GUN_TRIGGER2,
};

#define SACTION_GUN_MASK (0x0f << SACTION_GUN_TRIGGER)

static inline void emu_set_action(enum sched_action action_)
{
	extern enum sched_action emu_action, emu_action_old;
	extern int stop;

	if (action_ == SACTION_NONE)
		emu_action_old = 0;
	else if (action_ != emu_action_old)
		stop = 1;
	emu_action = action_;
}

#endif /* __LINUX_H__ */
