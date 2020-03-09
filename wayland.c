#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <sys/mman.h>
#include "config.h"
#include "os.h"
#include "xmem.h"
#include "wayland.h"
#include <stdbool.h>
#include "log.h"




void wlOutputAppend(struct wlOutput **outputs, struct wl_output *output, struct zxdg_output_v1 *xdg_output)
{
	struct wlOutput *l;
	struct wlOutput *n = xmalloc(sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->wl_output = output;
	n->xdg_output = xdg_output;
	if (!*outputs) {
		*outputs = n;
	} else {
		for (l = *outputs; l->next; l = l->next);
		l->next = n;
	}
}
struct wlOutput *wlOutputGet(struct wlOutput *outputs, struct wl_output *wl_output)
{
	struct wlOutput *l;
	for (l = outputs; l; l = l->next) {
		if (l->wl_output == wl_output) {
			break;
		}
	}
	return l;
}
struct wlOutput *wlOutputGetXdg(struct wlOutput *outputs, struct zxdg_output_v1 *xdg_output)
{
	struct wlOutput *l;
	for (l = outputs; l; l = l->next) {
		if (l->xdg_output == xdg_output)
			break;
	}
	return l;
}

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	if (!output) {
		logErr("Output not found");
		return;
	}
	logDbg("Mutating output...");
	if (output->have_log_pos) {
		logDbg("Except not really, because the logical position outweighs this");
		return;
	}	
	output->complete = false;
	output->x = x;
	output->y = y;
	logDbg("Got output at position %d,%d", x, y);
}
static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	bool preferred = flags & WL_OUTPUT_MODE_PREFERRED;
	bool current = flags & WL_OUTPUT_MODE_CURRENT;
	logDbg("Got %smode: %dx%d@%d%s", current ? "current " : "", width, height, refresh, preferred ? "*" : "");
	if (!output) {
		logErr("Output not found in list");
		return;
	}
	if (current) {
		if (!preferred) {
			logInfo("Not using preferred mode on output -- check config");
		}
		logDbg("Mutating output...");
		if (output->have_log_size) {
			logDbg("Except not really, because logical size outweighs this");
			return;
		}
		output->complete = false;
		output->width = width;
		output->height = height;
	}
}
static void output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	logDbg("Got scale factor for output: %d", factor);
	if (!output) {
		logErr("Output not found in list");
		return;
	}
	logDbg("Mutating output...");
	output->complete = false;
	output->scale = factor;
}
void (*wlOnOutputsUpdated)(struct wlOutput *outputs) = NULL;
static void output_done(void *data, struct wl_output *wl_output)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	if (!output) {
		logErr("Output not found in list");
		return;
	}
	output->complete = true;
	if (output->name) {
		logInfo("Output name: %s", output->name);
	}
	if (output->desc) {
		logInfo("Output description: %s", output->desc);
	}
	logInfo("Output updated: %dx%d at %d, %d (scale: %d)",
			output->width,
			output->height,
			output->x,
			output->y,
			output->scale);

	/* fire event if all outputs are complete. */
	bool complete = true;
	for (output = ctx->outputs; output; output = output->next) {
		complete = complete && output->complete;
	}
	if (complete) {
		logDbg("All outputs updated, triggering event");
		if (ctx->on_output_update)
			ctx->on_output_update(ctx);
	}
}

static void xdg_output_pos(void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y)
{
	logDbg("Got xdg output position: %d, %d", x, y);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	output->have_log_pos = true;
	output->x = x;
	output->y = y;
}
static void xdg_output_size(void *data, struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height)
{
	logDbg("Got xdg output size: %dx%d", width, height);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	output->have_log_size = true;
	output->width = width;
	output->height = height;
}
static void xdg_output_name(void *data, struct zxdg_output_v1 *xdg_output, const char *name)
{
	logDbg("Got xdg output name: %s", name);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	if (output->name) {
		free(output->name);
	}
	output->name = xstrdup(name);
}
static void xdg_output_desc(void *data, struct zxdg_output_v1 *xdg_output, const char *desc)
{
	logDbg("Got xdg output desc: %s", desc);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	if (output->desc) {
		free(output->desc);
	}
	output->desc = xstrdup(desc);
}




static struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_output_pos,
	.logical_size = xdg_output_size,
	.name = xdg_output_name,
	.description = xdg_output_desc,
};

static struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale
};

static void on_data_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *id)
{
	logDbg("Got data offer");
	struct wlContext *ctx = data;
	if (ctx->data_offer) {
		/* we destroy the old offer */
		zwlr_data_control_offer_v1_destroy(ctx->data_offer);
	}
	ctx->data_offer = id;
	memset(ctx->data_offer_formats, 0, sizeof(ctx->data_offer_formats));
	zwlr_data_control_offer_v1_add_listener(ctx->data_offer, &data_off_listener, ctx);
}
static void on_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *id)
{
	int sel, format, fds[2];
	struct wlContext *ctx = data;
	struct wlSelectionBuffer *buf;
	if (!id) {
		logErr("Got NULL selection");
		return;
	}
	if (id != data->data_offer) {
		logErr("Got selection with unknown offer");
		return;
	}
	for (format = 0; format < WL_SELECTION_FORMAT_MAX; ++format) {
		if (ctx->data_offer_formats[format]) {
			buf = &(ctx->data_buffer[WL_SELECTION_CLIPBOARD][format]);
			buf->pos = 0;
			buf->complete = false;
			buf->id = id;
			if (pipe(fds) == -1) {
				logErr("Selection pipe() failed: %s", strerror(errno));
				return;
			}
			if (
			buf->offer_fd = fds[0];
			zwlr_data_control-offer_v1_receive(id, wlSelectionFormatMimes[format], fds[1]);
			wl_display_roundtrip(ctx->display);



static struct zwlr_data_control_device_v1_listener data_control_listener = {
	on_data_offer,
	on_selection,
	on_finished,
	on_primary_selection
};
static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	struct wlContext *ctx = data;
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		ctx->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
	} else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
		ctx->pointer_manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
	} else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		ctx->keyboard_manager = wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) ==0) {
		ctx->output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3);
		if (ctx->outputs) {
			for (struct wlOutput *output = ctx->outputs; output; output = output->next) {
				if (!output->xdg_output) {
					output->xdg_output = zxdg_output_manager_v1_get_xdg_output(ctx->output_manager, output->wl_output);
					zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, ctx);
				}
			}
		}
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		wl_output = wl_registry_bind(registry, name, &wl_output_interface, version);
		wl_output_add_listener(wl_output, &output_listener, ctx);
		if (ctx->output_manager) {
			xdg_output = zxdg_output_manager_v1_get_xdg_output(ctx->output_manager, wl_output);
			zxdg_output_v1_add_listener(xdg_output, &xdg_output_listener, ctx);
		} else {
			xdg_output = NULL;
		}
		wlOutputAppend(&ctx->outputs, wl_output, xdg_output);
	} else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0) {
		ctx->data_manager = wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
	}

}
static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	//nothing here mf
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};


static int button_map[] = {
	0,
	0x110,
	0x112,
	0x111,
	0x150,
	0x151,
	-1
};
uint32_t wlTS(struct wlContext *ctx)
{
	struct timespec ts;
	if (ctx->epoch == -1) {
		ctx->epoch = time(NULL);
	}
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec -= ctx->epoch;
	return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

void wlClose(struct wlContext *ctx)
{
	return;
}

int wlSetup(struct wlContext *ctx, int width, int height)
{
	ctx->width = width;
	ctx->height = height;
	ctx->display = wl_display_connect(NULL);
	if (!ctx->display) {
		printf("Couldn't connect, yo\n");
		return 1;
	}
	ctx->registry = wl_display_get_registry(ctx->display);
	wl_registry_add_listener(ctx->registry, &registry_listener, ctx);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);

	ctx->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(ctx->pointer_manager, ctx->seat);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	ctx->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(ctx->keyboard_manager, ctx->seat);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	if(wlLoadConfLayout(ctx)) {
		return 1;
	}
	return 0;
}
void wlResUpdate(struct wlContext *ctx, int width, int height)
{
	ctx->width = width;
	ctx->height = height;
}

int wlPrepareFd(struct wlContext *ctx)
{
	int fd;

	fd = wl_display_get_fd(ctx->display);
//	while (wl_display_prepare_read(display) != 0) {
//		wl_display_dispatch(display);
//	}
//	wl_display_flush(display);
	return fd;
}

void wlPollProc(struct wlContext *ctx, short revents)
{
	if (revents & POLLIN) {
//		wl_display_cancel_read(display);
		wl_display_dispatch(ctx->display);
	}
}


/* FIXME XXX: hacky as fuck way to inhibit idle -- we just execute some commands */
void wlIdleInhibit(struct wlContext *ctx, bool on)
{
	char *cmd = configTryString(on ? "idle-inhibit/cmd-on" : "idle-inhibit/cmd-off", NULL);
	if (cmd)
		system(cmd);
	free(cmd);
}
