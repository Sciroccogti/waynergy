#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>
#include <sys/mman.h>
#include "os.h"
#include "xmem.h"
#include "config.h"
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-virtual-pointer-unstable-v1.prot.h"
#include "virtual-keyboard-unstable-v1.prot.h"
#include "xdg-output-unstable-v1.prot.h"
#include "wlr-data-control-unstable-v1.prot.h"
#include "uSynergy.h"

struct wlOutput
{
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	int32_t x;
	int32_t y;
	int width;
	int height;
	int32_t scale;
	bool complete;
	bool have_log_size;
	bool have_log_pos;
	char *name;
	char *desc;
	struct wlOutput *next;
};

struct wlContext {
	struct wl_registry *registry;
	struct wl_display *display;
	struct wl_seat *seat;
	struct zwlr_data_control_manager *data_manager;
	struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
	struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
	struct zxdg_output_manager_v1 *output_manager;
	struct zwlr_virtual_pointer_v1 *pointer;
	struct zwp_virtual_keyboard_v1 *keyboard;
	struct wlOutput *outputs;
	//state
	int width;
	int height;
	time_t epoch;
	//callbacks
	void (*on_output_update)(struct wlContext *ctx);
};

extern int wlLoadConfLayout(struct wlContext *ctx);
extern int wlSetup(struct wlContext *context, int width, int height);
extern uint32_t wlTS(struct wlContext *context);
extern void wlResUpdate(struct wlContext *context, int width, int height);
extern void wlClose(struct wlContext *context);
extern int wlPrepareFd(struct wlContext *context);
extern void wlPollProc(struct wlContext *context, short revents);

extern void wlMouseRelativeMotion(struct wlContext *context, int dx, int dy);
extern void wlMouseMotion(struct wlContext *context, int x, int y);
extern void wlMouseButtonDown(struct wlContext *context, int button);
extern void wlMouseButtonUp(struct wlContext *context, int button);
extern void wlMouseWheel(struct wlContext *context, signed short dx, signed short dy);
extern void wlKey(struct wlContext *context, int key, int state, uint32_t mask);
extern void wlIdleInhibit(struct wlContext *context, bool on);
