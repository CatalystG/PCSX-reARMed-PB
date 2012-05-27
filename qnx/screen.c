/*
 * (C) CatalystG, 2012
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <assert.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <fcntl.h>
#include <screen/screen.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "plugin_lib.h"
#include "../plugins/gpulib/gpu.h"
#include "../plugins/gpulib/cspace.h"

#include "touchcontroloverlay.h"

#include "main.h"
#include "../libpcsxcore/psemu_plugin_defs.h"
#include "common/readpng.h"
#include "qnx_common.h"

#define X_RES           1024
#define Y_RES           600
#define D_WIDTH			800
#define D_HEIGHT		600

int g_layer_x = (X_RES - D_WIDTH) / 2;
int g_layer_y = (Y_RES - D_HEIGHT) / 2;
int g_layer_w = D_WIDTH, g_layer_h = D_HEIGHT;

screen_context_t screen_ctx;
screen_window_t screen_win;
screen_buffer_t screen_buf[2];
screen_pixmap_t screen_pix;
screen_buffer_t screen_pbuf;

int size[2] = {0,0};
int rect[4] = { 0, 0, 0, 0 };
int old_bpp = 0;
int stride = 0;
int handleKeyFunc(int sym, int mod, int scancode, uint16_t unicode, int event);
int handleDPadFunc(int angle, int event);

struct tco_callbacks cb = {handleKeyFunc, handleDPadFunc, NULL, NULL, NULL, NULL};
tco_context_t tco_ctx;

static bool shutdown;

static void
handle_screen_event(bps_event_t *event)
{
    //int screen_val;

    screen_event_t screen_event = screen_event_get_event(event);
    //screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

    tco_touch(tco_ctx, screen_event);
}

int handleKeyFunc(int sym, int mod, int scancode, uint16_t unicode, int event){

	//printf("sym: %d\n", sym);fflush(stdout);
	switch(sym){
	case DKEY_CIRCLE:
	case DKEY_SQUARE:
	case DKEY_CROSS:
	case DKEY_TRIANGLE:
	case DKEY_START:
	case DKEY_SELECT:
	case DKEY_R1:
	case DKEY_R2:
	case DKEY_L1:
	case DKEY_L2:
		switch(event){
		case TCO_KB_DOWN:
			in_keystate |= 1 << sym;
			break;
		case TCO_KB_UP:
			in_keystate &= ~(1<<sym);
			emu_set_action(SACTION_NONE);
			break;
		default:
			break;
		}
		break;
	case 16:
		if(event == TCO_KB_DOWN){
			state_slot = 1;
			emu_set_action(SACTION_SAVE_STATE);
			in_keystate |= 1 << DKEY_START;
			//emu_save_state(1);
		} else {
			in_keystate &= ~(1<<DKEY_START);
		}
		break;
	case 17:
		if(event == TCO_KB_DOWN){
			state_slot = 1;
			emu_set_action(SACTION_LOAD_STATE);
			in_keystate |= 1 << DKEY_START;
			//emu_load_state(1);
		} else {
			in_keystate &= ~(1<<DKEY_START);
		}
		break;
	default:
		break;
	}

	return 0;
}

int handleDPadFunc(int angle, int event){
	static int key1 = -1, key2 = -1, old_key1 = -1, old_key2 = -1;

	//printf("angle: %d, key1: %d, key2: %d\n", angle, key1, key2);fflush(stdout);

	switch(event){
	case TCO_KB_DOWN:
		if (angle <= -68 && angle >= -113){
			key1 = DKEY_UP;
			key2 = -1;
		}
		else if (angle >= -22 && angle <= 22){
			key1 = DKEY_RIGHT;
			key2 = -1;
		}
		else if (angle >= 67 && angle <= 113){
			key1 = DKEY_DOWN;
			key2 = -1;
		}
		else if ((angle <= -157 && angle >= -180) ||
				(angle >= 157 && angle <= 180)){
			key1 = DKEY_LEFT;
			key2 = -1;
		}
		else if ((angle < -113) && (angle > -157)){
			key1 = DKEY_UP;
			key2 = DKEY_LEFT;
		} else if ((angle < 157) && (angle > 113)){
			key1 = DKEY_DOWN;
			key2 = DKEY_LEFT;
		} else if ((angle < 67) && (angle > 22)){
			key1 = DKEY_DOWN;
			key2 = DKEY_RIGHT;
		} else if ((angle < -22) && (angle > -68)){
			key1 = DKEY_UP;
			key2 = DKEY_RIGHT;
		}

		if((key1 == old_key1)&&(key2 == old_key2))
			return 0;

		in_keystate &= ~((1<<DKEY_UP)|(1<<DKEY_DOWN)|(1<<DKEY_LEFT)|(1<<DKEY_RIGHT));
		if (key1 >= 0)
			in_keystate |= 1 << key1;
		if (key2 >= 0)
			in_keystate |= 1 << key2;

		old_key1 = key1;
		old_key2 = key2;
		break;
	case TCO_KB_UP:
		in_keystate &= ~((1<<DKEY_UP)|(1<<DKEY_DOWN)|(1<<DKEY_LEFT)|(1<<DKEY_RIGHT));
		old_key1 = -1;
		old_key2 = -1;
		emu_set_action(SACTION_NONE);
		break;
	default:
		break;
	}

	return 0;
}

static void
handle_navigator_event(bps_event_t *event) {
	navigator_window_state_t state;
	bps_event_t *event_pause = NULL;
	int rc;

    switch (bps_event_get_code(event)) {
    case NAVIGATOR_SWIPE_DOWN:
        fprintf(stderr,"Swipe down event");
        emu_set_action(SACTION_ENTER_MENU);
        break;
    case NAVIGATOR_EXIT:
        fprintf(stderr,"Exit event");
		shutdown = true;
        /* Clean up */
		screen_stop_events(screen_ctx);
		bps_shutdown();
		tco_shutdown(tco_ctx);
		screen_destroy_window(screen_win);
		screen_destroy_context(screen_ctx);
        break;
    case NAVIGATOR_WINDOW_STATE:
    	state = navigator_event_get_window_state(event);

    	switch(state){
    	case NAVIGATOR_WINDOW_THUMBNAIL:
    		for(;;){
    			rc = bps_get_event(&event_pause, -1);
    			assert(rc==BPS_SUCCESS);

    			if(bps_event_get_code(event_pause) == NAVIGATOR_WINDOW_STATE){
    				state = navigator_event_get_window_state(event_pause);
    				if(state == NAVIGATOR_WINDOW_FULLSCREEN){
    					break;
    				}
    			}
    		}
    		break;
    	case NAVIGATOR_WINDOW_FULLSCREEN:
			in_keystate &= ~(1<<DKEY_START);
    		break;
    	case NAVIGATOR_WINDOW_INVISIBLE:
    		break;
    	}
    	break;
    default:
        break;
    }
    fprintf(stderr,"\n");
}

static void
handle_event()
{
    int rc, domain;

    while(true){
    	bps_event_t *event = NULL;
		rc = bps_get_event(&event, 0);
		if(rc == BPS_SUCCESS)
		{
			if (event) {
				domain = bps_event_get_domain(event);
				if (domain == navigator_get_domain()) {
					handle_navigator_event(event);
				} else if (domain == screen_get_domain()) {
					handle_screen_event(event);
				}
			} else {
				break;
			}
		}
    }
}

void
qnx_init(int * argc, char ***argv)
{
    const int usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_WRITE | SCREEN_USAGE_READ;
    int rc;

    /* Setup the window */
    screen_create_context(&screen_ctx, 0);
    screen_create_window(&screen_win, screen_ctx);
    screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);
    screen_create_window_buffers(screen_win, 2);

    screen_create_window_group( screen_win, "pcsx-pb" );

    screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)screen_buf);
    screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect+2);

	int bg[] = { SCREEN_BLIT_COLOR, 0x00000000, SCREEN_BLIT_END };
	screen_fill(screen_ctx, screen_buf[0], bg);
	screen_fill(screen_ctx, screen_buf[1], bg);

    screen_create_pixmap( &screen_pix, screen_ctx );

    int format = SCREEN_FORMAT_RGB565;
    screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_FORMAT, &format);

    int pix_usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
    screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_USAGE, &pix_usage);

    rc = tco_initialize(&tco_ctx, screen_ctx, cb);
    if(rc != TCO_SUCCESS)
    	printf("TCO: Init error\n");
    if(access("shared/misc/pcsx-rearmed-pb/cfg/controls.xml", F_OK) == 0){
    	rc = tco_loadcontrols(tco_ctx, "shared/misc/pcsx-rearmed-pb/cfg/controls.xml");
    } else {
    	rc = tco_loadcontrols(tco_ctx, "app/native/controls.xml");
    }
    if (rc != TCO_SUCCESS)
    	printf("TCO: Load Controls Error\n");fflush(stdout);
    tco_showlabels(tco_ctx, screen_win);

    screen_post_window(screen_win, screen_buf[0], 1, rect, 0);

    /* Signal bps library that navigator and screen events will be requested */
    //bps_initialize();
    screen_request_events(screen_ctx);
    navigator_request_events(0);

    return;
}

void plat_finish()
{
	//Force quit?
}

void menu_loop(void)
{
}

void *hildon_set_mode(int w, int h, int bpp)
{
	//printf("Hildon Set Mode...\n");fflush(stdout);
	//screen_get_buffer_property_pv(screen_buf[0], SCREEN_PROPERTY_POINTER, &pl_vout_buf);

	if((w == size[0]) && (h == size[1]) && (old_bpp == bpp))
		return pl_vout_buf;
	else{
		screen_destroy_pixmap_buffer( screen_pix );
	}

	size[0] = w;
	size[1] = h;
	old_bpp = bpp;

	screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_BUFFER_SIZE, size);

	int format;

	//if(bpp == 16)
		format = SCREEN_FORMAT_RGB565;
	//else if(bpp == 24)
		//format = SCREEN_FORMAT_RGBX8888;
	screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_FORMAT, &format);
	screen_create_pixmap_buffer(screen_pix);
	screen_get_pixmap_property_pv(screen_pix, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_pbuf);

	screen_get_buffer_property_iv(screen_pbuf, SCREEN_PROPERTY_STRIDE, &stride);
	screen_get_buffer_property_pv(screen_pbuf, SCREEN_PROPERTY_POINTER, (void **)&pl_vout_buf);

	return pl_vout_buf;
}

void *hildon_flip(void)
{

	int x = gpu.screen.x & ~1; // alignment needed by blitter
	int y = gpu.screen.y;
	int w = gpu.screen.w;
	int h = gpu.screen.h;
	uint16_t *vram = gpu.vram;
	//int stride = gpu.screen.hres;
	int rgb24 = gpu.status.rgb24;
	int fb_offs;
	uint8_t *dest;
	int i = h;

	//printf("x:%d, y:%d, w:%d, h:%d, stride:%d\n", x, y, w, h, stride);fflush(stdout);

	fb_offs = y * 1024 + x;
	dest = (uint8_t *)pl_vout_buf;

	if(size[0] != w && size[1] != h){
		hildon_set_mode(w, h, 16);
	} else if(pl_vout_buf != NULL){
		if(rgb24 == 0){
			for (; i-- > 0; dest += stride, fb_offs += 1024)
			{
				bgr555_to_rgb565(dest, vram + fb_offs, w * 2);
			}
		} else if (rgb24 == 1){
			for (; i-- > 0; dest += stride, fb_offs += 1024)
			{
				bgr888_to_rgb565(dest, vram + fb_offs, w * 3);
			}
		}
	}

	int hg[] = {
			SCREEN_BLIT_SOURCE_X, 0,
			SCREEN_BLIT_SOURCE_Y, 0,
			SCREEN_BLIT_SOURCE_WIDTH, size[0],
			SCREEN_BLIT_SOURCE_HEIGHT, size[1],
			SCREEN_BLIT_DESTINATION_X, rect[0]+112,
			SCREEN_BLIT_DESTINATION_Y, rect[1],
			SCREEN_BLIT_DESTINATION_WIDTH, rect[2]-(112*2),
			SCREEN_BLIT_DESTINATION_HEIGHT, rect[3],
			//SCREEN_BLIT_SCALE_QUALITY,SCREEN_QUALITY_FASTEST,
			SCREEN_BLIT_END
		};

	screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)screen_buf);
	screen_blit(screen_ctx, screen_buf[0], screen_pbuf, hg);
	screen_post_window(screen_win, screen_buf[0], 1, rect, 0);

	handle_event();

	return pl_vout_buf;
}

int omap_enable_layer(int enabled)
{
	return 0;
}

void menu_notify_mode_change(int w, int h, int bpp)
{
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	return NULL;
}

void plat_step_volume(int is_up)
{
}

void plat_trigger_vibrate(void)
{
}

void plat_minimize(void)
{
}


