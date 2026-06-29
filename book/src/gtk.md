# GTK GUI Library

lil has a built-in GTK+3 library for graphical user interfaces. Widgets are referenced by string names, and events set lil variables that you poll in a loop. No callback functions needed.

## Building with GTK

The default `make` builds lil without GTK support. Use `make gtk` to include it:

```sh
make gtk
```

This requires `libgtk-3-dev` (or your distro's GTK+3 development package).

## Widgets

Create widgets with `function gtk` commands. Every widget gets a string name you choose:

```lil
function gtk window "main" "hello" 400 300    # name, title, width, height
function gtk vbox "box" 10                     # vertical box with 10px spacing
function gtk hbox "row" 5                      # horizontal box with 5px spacing
function gtk button "btn" "click me"           # button with label
function gtk label "lbl" "hello world"         # static text label
function gtk entry "input" "type here..."      # text entry with placeholder
```

## Layout

Build a hierarchy by adding children to containers:

```lil
function gtk add "box" "btn" "lbl" "input"    # add multiple children at once
function gtk show "box"                        # show the container and all children
```

## Properties

Set and get widget properties at runtime:

```lil
function gtk set "lbl" "label" "new text"      # change button/label text
function gtk set "input" "text" "hello"        # change entry text
function gtk set "main" "title" "my app"       # change window title
function gtk set "btn" "sensitive" "false"     # disable a widget
function gtk set "btn" "visible" "true"        # show/hide
function gtk set "btn" "width" 100             # set size
function gtk set "input" "placeholder" "name"  # change placeholder text
```

Get values back:

```lil
function gtk get "input" "text"                # returns entry text
function gtk get "lbl" "text"                  # returns label text
function gtk get "main" "title"                # returns window title
```

## Events

Connect signals to lil variables. When the signal fires, the variable is set:

```lil
function gtk on "btn" "clicked" "clicked_var"
function gtk on "input" "changed" "input_text"
```

The variable starts as `""` and gets set to `"1"` for most signals. For entry `changed` signals, the variable gets the current text of the entry.

## Event Loop

Poll for events in a loop using `function gtk wait`:

```lil
loop {
    function gtk wait
    if clicked_var != "" {
        print "button was clicked"
        clicked_var = ""
    }
    if input_text != "" {
        print "input changed to: " input_text
        input_text = ""
    }
    if __gtk_quit != "" {
        print "window was closed"
        stop
    }
}
```

`function gtk wait` blocks until a signal fires (or the window is closed). Each signal callback sets its variable then returns control to your script.

The special variable `__gtk_quit` is set to `"1"` when the window is destroyed. Check it to exit your loop cleanly.

After a signal fires, reset the variable back to `""` so you can detect the next event.

## Delaying Between Checks

If you need a small delay between event loop iterations (e.g. for smooth animations or to reduce CPU usage):

```lil
loop {
    function gtk wait
    if clicked_var == "1" {
        print "clicked"
        clicked_var = ""
    }
    function math sleep 0.05
}
```

`function math sleep` takes seconds (including fractional), so `0.05` is 50ms. It works in both interpreted and compiled mode.

## Complete Example

A window with a button and a counter:

```lil
function gtk window "win" "counter" 300 150
function gtk vbox "box" 10
function gtk label "lbl" "0"
function gtk button "btn" "increment"
function gtk add "box" "lbl" "btn"
function gtk show "win"

function gtk on "btn" "clicked" "click"

n = 0

loop {
    function gtk wait
    if click != "" {
        n = n + 1
        function gtk set "lbl" "label" n
        click = ""
    }
    if __gtk_quit != "" {
        stop
    }
}
```

## All Functions

| Function | Arguments | Description |
|----------|-----------|-------------|
| `window` | name, title, w, h | Create a window |
| `vbox` | name [, spacing] | Vertical box layout |
| `hbox` | name [, spacing] | Horizontal box layout |
| `button` | name, label | Clickable button |
| `label` | name, text | Static text label |
| `entry` | name [, placeholder] | Text input field |
| `add` | parent, child1 [, child2, ...] | Add children to container |
| `show` | name | Show a widget (calls show_all) |
| `set` | name, property, value | Set a widget property |
| `get` | name, property | Get a widget property (returns string) |
| `on` | name, signal, variable | Connect signal to variable |
| `wait` | | Wait for a signal to fire |
| `run` | | Start the GTK main loop (blocks until quit) |
| `timeout` | ms, signal, variable | Fire a signal after N milliseconds |
| `quit` | | Quit the GTK main loop |
| `destroy` | name | Destroy a widget |
| `init` | | Explicitly initialize GTK (auto-called on first window) |

## Signal Reference

Common signals you can connect with `function gtk on`:

| Signal | Fires When | Variable Value |
|--------|------------|----------------|
| `clicked` | Button is clicked | `"1"` |
| `changed` | Entry text changes | The current entry text |
| `activate` | Enter pressed in entry | `"1"` |
| `enter-notify-event` | Mouse enters widget | `"1"` |
| `leave-notify-event` | Mouse leaves widget | `"1"` |
| `focus-in-event` | Widget gains focus | `"1"` |
| `focus-out-event` | Widget loses focus | `"1"` |
| `destroy` | Widget is being destroyed | `"1"` |

## Notes

- GTK functions only work in interpreted (VM) mode. Compiled mode silently returns 0.
- Widgets are automatically removed from the internal registry when destroyed.
- All event variables are initialized to `""` when connected, so you can check `if varname != ""` before any signal fires.
- Use `function gtk run` instead of a polling loop if you only need to handle a single event (it blocks until the window is closed).
