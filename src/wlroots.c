#ifdef HAVE_WLROOTS
#define _POSIX_C_SOURCE 199309L
#include "lil.h"
#include <time.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wayland-server-core.h>

static struct wl_display *wdisplay;
static struct wlr_backend *wbackend;
static struct wlr_scene *wscene;
static struct wlr_scene_output_layout *wscene_layout;
static struct wlr_output_layout *woutput_layout;
static struct wlr_xdg_shell *wxdg_shell;
static struct wlr_renderer *wrenderer;
static struct wlr_allocator *wallocator;
static struct wlr_cursor *wcursor;
static struct wlr_xcursor_manager *wcursor_mgr;
static struct wlr_seat *wseat;
static struct wlr_output *woutput;
static struct wl_event_loop *wevent_loop;
static int wl_running;
static int wl_frame;
static int wl_quit_pending;
static int wl_mod_state;
static int wl_pointer_press_time;
static char wl_frame_var[64];
static char wl_toplevel_var[64];
static char wl_pointer_motion_var[64];
static char wl_pointer_button_var[64];
static char wl_key_var[64];
static char wl_key_buf[64];
static struct wl_listener new_output_listener;
static struct wl_listener new_toplevel_listener;
static struct wl_listener new_input_listener;

static void wl_toplevel_destroy_cb(struct wl_listener *listener, void *data);

#define MAX_WINDOWS 64
static struct {
    struct wlr_xdg_toplevel *toplevel;
    struct wlr_scene_node *node;
    char app_id[64];
    char title[256];
    struct wl_listener destroy;
} wl_windows[MAX_WINDOWS];
static int wl_num_windows;

static void wl_toplevel_destroy_cb(struct wl_listener *listener, void *data) {
    (void)data;
    for (int i = 0; i < wl_num_windows; i++) {
        if (&wl_windows[i].destroy == listener) {
            wl_windows[i].node = NULL;
            wl_windows[i].toplevel = NULL;
            break;
        }
    }
}

static void wl_output_frame(struct wl_listener *listener, void *data) {
    (void)listener; (void)data;
    wl_frame = 1;
}

static void wl_new_output(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_output *output = data;
    struct wl_listener *fl = calloc(1, sizeof(struct wl_listener));
    if (!fl) return;
    fl->notify = wl_output_frame;
    wl_signal_add(&output->events.frame, fl);
    wlr_output_init_render(output, wallocator, wrenderer);
    struct wlr_scene_output *so = wlr_scene_output_create(wscene, output);
    struct wlr_output_layout_output *lo = wlr_output_layout_add_auto(woutput_layout, output);
    wlr_scene_output_layout_add_output(wscene_layout, lo, so);
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, 1);
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
    woutput = output;
}

static void wl_new_toplevel_cb(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_xdg_toplevel *toplevel = data;
    struct wlr_scene_surface *ss = wlr_scene_surface_create(&wscene->tree, toplevel->base->surface);
    if (!ss) return;
    if (wl_num_windows < MAX_WINDOWS) {
        wl_windows[wl_num_windows].toplevel = toplevel;
        wl_windows[wl_num_windows].node = &ss->buffer->node;
        strncpy(wl_windows[wl_num_windows].app_id, toplevel->app_id ? toplevel->app_id : "", 63);
        wl_windows[wl_num_windows].app_id[63] = 0;
        strncpy(wl_windows[wl_num_windows].title, toplevel->title ? toplevel->title : "", 255);
        wl_windows[wl_num_windows].title[255] = 0;
        wl_windows[wl_num_windows].destroy.notify = wl_toplevel_destroy_cb;
        wl_signal_add(&toplevel->base->events.destroy, &wl_windows[wl_num_windows].destroy);
        wl_num_windows++;
    }
        if (wl_toplevel_var[0]) {
        char buf[512];
        snprintf(buf, sizeof(buf), "mapped:%s:%s", toplevel->app_id ? toplevel->app_id : "", toplevel->title ? toplevel->title : "");
        var_set(wl_toplevel_var, make_str(buf));
    }
}

static void wl_pointer_motion_cb(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_pointer_motion_event *ev = data;
    wlr_cursor_move(wcursor, &ev->pointer->base, ev->delta_x, ev->delta_y);
    if (wl_pointer_motion_var[0]) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f %.0f", wcursor->x, wcursor->y);
        var_set(wl_pointer_motion_var, make_str(buf));
    }
}

static void wl_pointer_button_cb(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_pointer_button_event *ev = data;
    if (ev->button == 273 && ev->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        wl_quit_pending = 2;
        return;
    }
    if (wl_pointer_button_var[0]) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u:%s", ev->button, ev->state == WL_POINTER_BUTTON_STATE_PRESSED ? "pressed" : "released");
        var_set(wl_pointer_button_var, make_str(buf));
    }
}

static void wl_keyboard_key_cb(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_keyboard_key_event *ev = data;
    snprintf(wl_key_buf, sizeof(wl_key_buf), "%u:%s", ev->keycode,
        ev->state == WL_KEYBOARD_KEY_STATE_PRESSED ? "pressed" : "released");
    if (ev->keycode == 24 && ev->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        wl_quit_pending = 1;
    }
}

struct wl_input_ctx { struct wl_listener destroy; struct wl_listener *ml, *bl, *kl; };

static void wl_input_destroy_cb(struct wl_listener *listener, void *data) {
    (void)data;
    struct wl_input_ctx *ctx = (struct wl_input_ctx *)listener;
    if (ctx->ml && ctx->ml->link.prev) { wl_list_remove(&ctx->ml->link); free(ctx->ml); }
    if (ctx->bl && ctx->bl->link.prev) { wl_list_remove(&ctx->bl->link); free(ctx->bl); }
    if (ctx->kl && ctx->kl->link.prev) { wl_list_remove(&ctx->kl->link); free(ctx->kl); }
    free(ctx);
}

static void wl_new_input_cb(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_input_device *dev = data;
    struct wl_input_ctx *ctx = calloc(1, sizeof(struct wl_input_ctx));
    if (!ctx) return;
    ctx->destroy.notify = wl_input_destroy_cb;
    if (dev->type == WLR_INPUT_DEVICE_POINTER) {
        struct wlr_pointer *ptr = wlr_pointer_from_input_device(dev);
        ctx->ml = calloc(1, sizeof(struct wl_listener));
        ctx->bl = calloc(1, sizeof(struct wl_listener));
        if (ctx->ml) { ctx->ml->notify = wl_pointer_motion_cb; wl_signal_add(&ptr->events.motion, ctx->ml); }
        if (ctx->bl) { ctx->bl->notify = wl_pointer_button_cb; wl_signal_add(&ptr->events.button, ctx->bl); }
        wlr_cursor_attach_input_device(wcursor, dev);
    } else if (dev->type == WLR_INPUT_DEVICE_KEYBOARD) {
        struct wlr_keyboard *kbd = wlr_keyboard_from_input_device(dev);
        ctx->kl = calloc(1, sizeof(struct wl_listener));
        if (ctx->kl) { ctx->kl->notify = wl_keyboard_key_cb; wl_signal_add(&kbd->events.key, ctx->kl); }
        wlr_seat_set_keyboard(wseat, kbd);
    }
    wl_signal_add(&dev->events.destroy, &ctx->destroy);
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
        wallocator = wlr_allocator_autocreate(wbackend, wrenderer);
        wscene = wlr_scene_create();
        woutput_layout = wlr_output_layout_create(wdisplay);
        wscene_layout = wlr_scene_attach_output_layout(wscene, woutput_layout);
        wxdg_shell = wlr_xdg_shell_create(wdisplay, 1);
        if (!wxdg_shell) fatal("line %d: failed to create xdg shell", line);
        wcursor = wlr_cursor_create();
        wcursor_mgr = wlr_xcursor_manager_create("default", 24);
        wlr_xcursor_manager_load(wcursor_mgr, 1);
        wseat = wlr_seat_create(wdisplay, "seat0");
        wlr_cursor_attach_output_layout(wcursor, woutput_layout);
        new_output_listener.notify = wl_new_output;
        wl_signal_add(&wbackend->events.new_output, &new_output_listener);
        new_toplevel_listener.notify = wl_new_toplevel_cb;
        wl_signal_add(&wxdg_shell->events.new_toplevel, &new_toplevel_listener);
        new_input_listener.notify = wl_new_input_cb;
        wl_signal_add(&wbackend->events.new_input, &new_input_listener);
        if (!wlr_backend_start(wbackend))
            fatal("line %d: failed to start backend", line);
        wl_display_init_shm(wdisplay);
        for (int _wtries = 0; !woutput && _wtries < 200; _wtries++)
            wl_event_loop_dispatch(wevent_loop, 50);
        if (!woutput)
            fatal("line %d: no output available", line);
        wl_running = 1;
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
    if (!strcmp(fn, "render")) {
        if (!woutput) fatal("line %d: no output", line);
        struct wlr_scene_output *so = wlr_scene_get_scene_output(wscene, woutput);
        if (so) wlr_scene_output_commit(so, NULL);
        return make_str("");
    }
    if (!strcmp(fn, "move_window")) {
        if (argc < 4) fatal("line %d: move_window expects app_id x y", line);
        char *rid = resolve_arg(args[1]);
        char *r2 = resolve_arg(args[2]), *r3 = resolve_arg(args[3]);
        int nx = (int)strtod(r2, NULL); free(r2);
        int ny = (int)strtod(r3, NULL); free(r3);
        for (int i = 0; i < wl_num_windows; i++) {
            if (wl_windows[i].node && (!strcmp(wl_windows[i].app_id, rid) || !strcmp(wl_windows[i].title, rid))) {
                wlr_scene_node_set_position(wl_windows[i].node, nx, ny);
                break;
            }
        }
        free(rid);
        return make_str("");
    }
    if (!strcmp(fn, "window_at")) {
        if (argc < 3) fatal("line %d: window_at expects x y", line);
        char *r1 = resolve_arg(args[1]), *r2 = resolve_arg(args[2]);
        double wx = strtod(r1, NULL); free(r1);
        double wy = strtod(r2, NULL); free(r2);
        struct wlr_scene_node *node = wlr_scene_node_at(&wscene->tree.node, (int)wx, (int)wy, NULL, NULL);
        char result[256] = "";
        if (node) {
            for (int i = 0; i < wl_num_windows; i++) {
                if (wl_windows[i].node && wl_windows[i].node == node) {
                    snprintf(result, sizeof(result), "%s:%s", wl_windows[i].app_id, wl_windows[i].title);
                    break;
                }
            }
        }
        return make_str(strlen(result) > 0 ? result : "none");
    }
    if (!strcmp(fn, "on")) {
        if (argc < 3) fatal("line %d: on expects signal and variable", line);
        char *signal = resolve_arg(args[1]);
        char *var = resolve_arg(args[2]);
        if (!strcmp(signal, "frame")) {
            strncpy(wl_frame_var, var, 63); wl_frame_var[63] = 0;
            var_set(var, make_str(""));
        } else if (!strcmp(signal, "new_toplevel")) {
            strncpy(wl_toplevel_var, var, 63); wl_toplevel_var[63] = 0;
            var_set(var, make_str(""));
        } else if (!strcmp(signal, "pointer_motion")) {
            strncpy(wl_pointer_motion_var, var, 63); wl_pointer_motion_var[63] = 0;
            var_set(var, make_str(""));
        } else if (!strcmp(signal, "pointer_button")) {
            strncpy(wl_pointer_button_var, var, 63); wl_pointer_button_var[63] = 0;
            var_set(var, make_str(""));
        } else if (!strcmp(signal, "key")) {
            strncpy(wl_key_var, var, 63); wl_key_var[63] = 0;
            var_set(var, make_str(""));
        }
        free(signal); free(var);
        return make_str("");
    }
    if (!strcmp(fn, "frame")) {
        if (wl_frame) { wl_frame = 0; if (wl_frame_var[0]) var_set(wl_frame_var, make_str("1")); }
        if (wl_key_buf[0] && wl_key_var[0]) {
            var_set(wl_key_var, make_str(wl_key_buf));
            wl_key_buf[0] = 0;
        }
        wl_display_flush_clients(wdisplay);
        wl_event_loop_dispatch(wevent_loop, 0);
        return make_str("");
    }
    if (!strcmp(fn, "run")) {
        while (wl_running) {
            wl_display_flush_clients(wdisplay);
            if (wl_key_buf[0] && wl_key_var[0]) {
                if (wl_key_buf[0] == '2' && wl_key_buf[1] == '4' && wl_key_buf[2] == ':') {
                    wl_running = 0; break;
                }
                var_set(wl_key_var, make_str(wl_key_buf));
                wl_key_buf[0] = 0;
            }
            if (wl_quit_pending) { wl_quit_pending = 0; wl_running = 0; break; }

            if (wl_frame) {
                wl_frame = 0;
                if (wl_frame_var[0]) var_set(wl_frame_var, make_str("1"));
                return make_str("frame");
            }
            wl_event_loop_dispatch(wevent_loop, -1);
        }
        if (wdisplay) { wl_display_destroy(wdisplay); wdisplay = NULL; }
        return make_str("");
    }
    if (!strcmp(fn, "set_offset")) {
        if (argc < 3) fatal("line %d: set_offset expects x y", line);
        char *r1 = resolve_arg(args[1]), *r2 = resolve_arg(args[2]);
        double ox = strtod(r1, NULL); free(r1);
        double oy = strtod(r2, NULL); free(r2);
        struct wlr_scene_output *so = wlr_scene_get_scene_output(wscene, woutput);
        if (so) wlr_scene_output_set_position(so, (int)ox, (int)oy);
        return make_str("");
    }
    if (!strcmp(fn, "cursor_show")) {
        wlr_cursor_set_xcursor(wcursor, wcursor_mgr, "default");
        if (woutput) wlr_cursor_warp_closest(wcursor, NULL, woutput->width / 2, woutput->height / 2);
        return make_str("");
    }
    if (!strcmp(fn, "cursor_hide")) {
        wlr_cursor_set_xcursor(wcursor, wcursor_mgr, "none");
        return make_str("");
    }
    if (!strcmp(fn, "window_pos")) {
        if (argc < 2) fatal("line %d: window_pos expects app_id", line);
        char *rid = resolve_arg(args[1]);
        char result[64] = "";
        for (int i = 0; i < wl_num_windows; i++) {
            if (wl_windows[i].node && (!strcmp(wl_windows[i].app_id, rid) || !strcmp(wl_windows[i].title, rid))) {
                snprintf(result, sizeof(result), "%.0f %.0f", wl_windows[i].node->x, wl_windows[i].node->y);
                break;
            }
        }
        free(rid);
        return make_str(strlen(result) > 0 ? result : "none");
    }
    if (!strcmp(fn, "set_bg")) {
        if (argc < 4) fatal("line %d: set_bg expects r g b", line);
        char *r1 = resolve_arg(args[1]), *r2 = resolve_arg(args[2]), *r3 = resolve_arg(args[3]);
        float r = (float)strtod(r1, NULL); free(r1);
        float g = (float)strtod(r2, NULL); free(r2);
        float b = (float)strtod(r3, NULL); free(r3);
        float color[4] = {r, g, b, 1.0f};
        struct wlr_scene_rect *bg = wlr_scene_rect_create(&wscene->tree, 32768, 32768, color);
        wlr_scene_node_lower_to_bottom(&bg->node);
        return make_str("");
    }
    if (!strcmp(fn, "quit")) {
        wl_running = 0;
        if (wdisplay) {
            wl_list_remove(&new_output_listener.link);
            wl_list_remove(&new_toplevel_listener.link);
            wl_list_remove(&new_input_listener.link);
            wl_display_destroy(wdisplay);
            wdisplay = NULL;
        }
        return make_str("");
    }
    fatal("line %d: unknown wlroots function '%s'", line, fn);
    return make_num(0);
}
#endif
