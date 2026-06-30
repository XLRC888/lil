# WLROOTS Compositor Library

Build a Wayland compositor using wlroots. Requires `make wlroots` (depends on `wlroots`, `wayland-server`, `xkbcommon`).

## Functions

| Function | Arguments | Description |
|----------|-----------|-------------|
| `init` | | Create display, backend, renderer, allocator |
| `set_mode` | width, height, refresh | Set output mode |
| `on` | signal, variable | Connect event to variable |
| `frame` | | Dispatch events, check for frame signal |
| `run` | | Start the compositor event loop |
| `begin_frame` | | Begin rendering a frame |
| `clear` | r, g, b | Clear the framebuffer to a color |
| `end_frame` | | Commit the rendered frame |
| `quit` | | Quit the event loop |

## Example

```lil
include wlroots

&wlroots|init
&wlroots|set_mode 1920 1080 60
&wlroots|on "frame" frame

loop {
    &wlroots|frame
    if frame != "" {
        &wlroots|begin_frame
        &wlroots|clear 0.1 0.1 0.2
        &wlroots|end_frame
        frame = ""
    }
}
```
