#ifdef HAVE_WLROOTS
#include "lil.h"
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <wayland-server-core.h>

static struct wl_display *wdisplay;
static struct wlr_backend *wbackend;
static struct wlr_renderer *wrenderer;
static struct wlr_allocator *wallocator;
static struct wlr_output *woutput;
static int wl_running;
static int wl_frame;
static char wl_frame_var[64];

static void wl_output_frame(struct wl_listener *listener, void *data) {
    (void)listener; (void)data;
    wl_frame = 1;
}

Value wlroots_dispatch(const char *fn, int argc, char **args, int line) {
    if (!strcmp(fn, "init")) {
        wlr_log_init(WLR_DEBUG, NULL);
        wdisplay = wl_display_create();
        if (!wdisplay) fatal("line %d: failed to create display", line);
        wbackend = wlr_backend_autocreate(wdisplay, NULL);
        if (!wbackend) fatal("line %d: failed to create backend", line);
        wrenderer = wlr_renderer_autocreate(wbackend);
        if (!wrenderer) fatal("line %d: failed to create renderer", line);
        wallocator = wlr_allocator_autocreate(wbackend, wrenderer);
        if (!wallocator) fatal("line %d: failed to create allocator", line);
        return make_str("");
    }
    if (!strcmp(fn, "create_output")) {
        if (argc < 2) fatal("line %d: expected output name", line);
        char *name = resolve_arg(args[1]);
        (void)name;
        struct wlr_output *output = wlr_output_create(wdisplay);
        if (!output) fatal("line %d: failed to create output", line);
        static struct wl_listener frame_listener;
        frame_listener.notify = wl_output_frame;
        wl_signal_add(&output->events.frame, &frame_listener);
        woutput = output;
        return make_str("");
    }
    if (!strcmp(fn, "set_mode")) {
        if (argc < 4) fatal("line %d: set_mode expects width, height, refresh", line);
        if (!woutput) fatal("line %d: no output created", line);
        int w = (int)strtod(resolve_arg(args[1]), NULL);
        int h = (int)strtod(resolve_arg(args[2]), NULL);
        int rr = (int)strtod(resolve_arg(args[3]), NULL);
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_mode(&state, WLR_OUTPUT_STATE_MODE_CUSTOM);
        wlr_output_state_set_custom_mode(&state, w, h, rr * 1000);
        wlr_output_state_set_enabled(&state, 1);
        wlr_output_commit_state(woutput, &state);
        wlr_output_state_finish(&state);
        return make_str("");
    }
    if (!strcmp(fn, "on")) {
        if (argc < 3) fatal("line %d: on expects signal and variable", line);
        char *signal = resolve_arg(args[1]);
        char *var = resolve_arg(args[2]);
        if (!strcmp(signal, "frame")) {
            strncpy(wl_frame_var, var, 63);
            wl_frame_var[63] = 0;
            var_set(var, make_str(""));
        }
        return make_str("");
    }
    if (!strcmp(fn, "frame")) {
        if (wl_frame) {
            wl_frame = 0;
            if (wl_frame_var[0]) var_set(wl_frame_var, make_str("1"));
        }
        wl_display_flush_clients(wdisplay);
        wl_display_dispatch(wdisplay);
        return make_str("");
    }
    if (!strcmp(fn, "run")) {
        wl_running = 1;
        while (wl_running) {
            wl_display_flush_clients(wdisplay);
            if (wl_frame) {
                wl_frame = 0;
                if (wl_frame_var[0]) var_set(wl_frame_var, make_str("1"));
                return make_str("frame");
            }
            int r = wl_display_dispatch(wdisplay);
            if (r == -1) break;
        }
        return make_str("");
    }
    if (!strcmp(fn, "begin_frame")) {
        if (!woutput) fatal("line %d: no output", line);
        int ok = wlr_renderer_begin(wrenderer, woutput->width, woutput->height);
        if (!ok) return make_str("");
        return make_str("");
    }
    if (!strcmp(fn, "clear")) {
        if (argc < 4) fatal("line %d: clear expects r g b", line);
        float r = (float)strtod(resolve_arg(args[1]), NULL);
        float g = (float)strtod(resolve_arg(args[2]), NULL);
        float b = (float)strtod(resolve_arg(args[3]), NULL);
        wlr_renderer_clear(wrenderer, (float[]){r, g, b, 1.0f});
        return make_str("");
    }
    if (!strcmp(fn, "end_frame")) {
        wlr_output_set_commit_data(woutput, NULL, NULL, 0);
        wlr_renderer_end(wrenderer);
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_damage_whole(&state);
        wlr_output_commit_state(woutput, &state);
        wlr_output_state_finish(&state);
        return make_str("");
    }
    if (!strcmp(fn, "quit")) {
        wl_running = 0;
        return make_str("");
    }
    fatal("line %d: unknown wlroots function '%s'", line, fn);
    return make_num(0);
}
#endif
