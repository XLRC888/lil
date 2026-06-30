# WLROOTS Compositor Library

Build a Wayland compositor using wlroots. Requires `make wlroots` (depends on `wlroots`, `wayland-server`, `xkbcommon`).

## Functions

| Function | Arguments | Description |
|----------|-----------|-------------|
| `init` | | Create display, backend, scene, xdg_shell, cursor, seat |
| `set_mode` | width, height, refresh | Set output mode |
| `render` | | Commit the scene graph to output |
| `on` | signal, variable | Connect event to variable |
| `frame` | | Dispatch events, poll for signals |
| `run` | | Start the compositor event loop |
| `move_window` | app_id, x, y | Position a window on the grid |
| `window_at` | x, y | Get app_id:title at coordinates |
| `set_offset` | x, y | Scroll the canvas/scene offset |
| `cursor_show` | | Show cursor with default theme |
| `cursor_hide` | | Hide cursor |
| `quit` | | Quit the event loop |

## Signals

| Signal | Variable Set To | Description |
|--------|-----------------|-------------|
| `frame` | `"1"` | Vblank signal, ready to render |
| `new_toplevel` | `"mapped:app_id:title"` | New app window opened |
| `key` | `"keycode:pressed/released"` | Keyboard event |
| `pointer_motion` | `"x y"` | Cursor moved |
| `pointer_button` | `"button:pressed/released"` | Mouse click |

## Example

```lil
include wlroots

&wlroots|init
&wlroots|set_mode 1920 1080 60
&wlroots|on "frame" frame
&wlroots|on "key" key
&wlroots|on "new_toplevel" win
&wlroots|on "pointer_motion" ptr
&wlroots|cursor_show

loop {
    &wlroots|frame
    if frame != "" {
        &wlroots|render
        frame = ""
    }
    if key != "" {
        print "key:", key
        key = ""
    }
    if win != "" {
        print "window:", win
        &wlroots|move_window "firefox" 100 100
        win = ""
    }
}
```
