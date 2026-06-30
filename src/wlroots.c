#ifdef HAVE_WLROOTS
#define _POSIX_C_SOURCE 199309L
#include "lil.h"
#include <time.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/pass.h>
#include <wlr/render/color.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>
#include <wayland-server-core.h>
#include <wayland-server.h>

static struct wl_display *wdisplay;
static struct wlr_backend *wbackend;
static struct wlr_renderer *wrenderer;
static struct wlr_allocator *wallocator;
static struct wlr_output *woutput;
static struct wl_event_loop *wevent_loop;
static int wl_running;
static int wl_frame;
static char wl_frame_var[64];
static struct wl_listener new_output_listener;

static void wl_output_frame(struct wl_listener *listener, void *data) {
    (void)listener; (void)data;
    wl_frame = 1;
}

static void wl_new_output(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_output *output = data;
    wlr_output_init_render(output, wallocator, wrenderer);
    struct wl_listener *fl = calloc(1, sizeof(struct wl_listener));
    fl->notify = wl_output_frame;
    wl_signal_add(&output->events.frame, fl);
    woutput = output;
}

Value wlroots_dispatch(const char *fn, int argc, char **args, int line) {
    if (!strcmp(fn, "init")) {
        wlr_log_init(WLR_DEBUG, NULL);
        wdisplay = wl_display_create();
        if (!wdisplay) fatal("line %d: failed to create display", line);
        wevent_loop = wl_display_get_event_loop(wdisplay);
        wbackend = wlr_backend_autocreate(wevent_loop, NULL);
        if (!wbackend) fatal("line %d: failed to create backend", line);
        wrenderer = wlr_renderer_autocreate(wbackend);
        if (!wrenderer) fatal("line %d: failed to create renderer", line);
        wallocator = wlr_allocator_autocreate(wbackend, wrenderer);
        if (!wallocator) fatal("line %d: failed to create allocator", line);
        new_output_listener.notify = wl_new_output;
        wl_signal_add(&wbackend->events.new_output, &new_output_listener);
        return make_str("");
    }
    if (!strcmp(fn, "set_mode")) {
        if (argc < 4) fatal("line %d: set_mode expects width, height, refresh", line);
        if (!woutput) fatal("line %d: no output", line);
        char *r1 = resolve_arg(args[1]), *r2 = resolve_arg(args[2]), *r3 = resolve_arg(args[3]);
        int w = (int)strtod(r1, NULL); free(r1);
        int h = (int)strtod(r2, NULL); free(r2);
        int rr = (int)strtod(r3, NULL); free(r3);
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_mode(&state, WLR_OUTPUT_STATE_MODE_FIXED);
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
        free(signal); free(var);
        return make_str("");
    }
    if (!strcmp(fn, "frame")) {
        if (wl_frame) {
            wl_frame = 0;
            if (wl_frame_var[0]) var_set(wl_frame_var, make_str("1"));
        }
        wl_display_flush_clients(wdisplay);
        wl_event_loop_dispatch(wevent_loop, 0);
        return make_str("");
    }
    if (!strcmp(fn, "run")) {
        wl_running = 1;
        if (!wlr_backend_start(wbackend))
            fatal("line %d: failed to start backend", line);
        wl_display_init_shm(wdisplay);
        while (wl_running) {
            wl_display_flush_clients(wdisplay);
            if (wl_frame) {
                wl_frame = 0;
                if (wl_frame_var[0]) var_set(wl_frame_var, make_str("1"));
                return make_str("frame");
            }
            wl_event_loop_dispatch(wevent_loop, -1);
        }
        return make_str("");
    }
    if (!strcmp(fn, "begin_frame")) {
        if (!woutput) fatal("line %d: no output", line);
        return make_str("");
    }
    if (!strcmp(fn, "clear")) {
        if (argc < 4) fatal("line %d: clear expects r g b", line);
        char *r1 = resolve_arg(args[1]), *r2 = resolve_arg(args[2]), *r3 = resolve_arg(args[3]);
        float r = (float)strtod(r1, NULL); free(r1);
        float g = (float)strtod(r2, NULL); free(r2);
        float b = (float)strtod(r3, NULL); free(r3);
        struct wlr_buffer *buf = wlr_allocator_create_buffer(wallocator, woutput->width, woutput->height, NULL);
        if (buf) {
            struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(wrenderer, buf, NULL);
            if (pass) {
                struct wlr_render_rect_options opts = {
                    .box = {0, 0, woutput->width, woutput->height},
                    .color = {r, g, b, 1.0f},
                    .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
                };
                wlr_render_pass_add_rect(pass, &opts);
                wlr_render_pass_submit(pass);
            }
            wlr_buffer_drop(buf);
        }
        return make_str("");
    }
    if (!strcmp(fn, "end_frame")) {
        if (!woutput) fatal("line %d: no output", line);
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_damage(&state, NULL);
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
