# GTK GUI Library

lil has a built-in GTK+3 library for graphical user interfaces.

Widgets are referenced by string names, and events set lil variables that you poll in a loop. No callback functions needed.

## Building with GTK

The default `make` builds lil without GTK support. Use `make gtk` to include it:

```sh
make gtk
make install-gtk    : install the gtk-enabled binary to PATH
```

This requires `libgtk-3-dev` (or your distro's GTK+3 development package).

## Important: String Literals Required

All GTK function arguments (widget names, property names, signal names, variable names) **must be quoted strings**. Bare identifiers are evaluated as variables, which will cause errors.

```lil
window@gtk("win", "hello", 400, 300)    : correct
window@gtk(win, "hello", 400, 300)      : WRONG - win evaluated as variable
```

## Widgets

Create widgets with `func@gtk` commands. Every widget gets a string name you choose:

```lil
window@gtk("main", "hello", 400, 300)    : name, title, width, height
vbox@gtk("box", 10)                     : vertical box with 10px spacing
hbox@gtk("row", 5)                      : horizontal box with 5px spacing
button@gtk("btn", "click me")           : button with label
label@gtk("lbl", "hello world")         : static text label
entry@gtk("input", "type here...")      : text entry with placeholder
```

Widgets are referred to by string names, not objects.

## Layout

Build a hierarchy by adding children to containers:

```lil
add@gtk("box", "btn", "lbl", "input")    : add multiple children at once
show@gtk("box")                        : show the container and all children
```

## Properties

Set and get widget properties at runtime:

```lil
set@gtk("lbl", "label", "new text")      : change button/label text
set@gtk("input", "text", "hello")        : change entry text
set@gtk("main", "title", "my app")       : change window title
set@gtk("btn", "sensitive", "false")     : disable a widget
```

Get current property values:

```lil
current = get@gtk("lbl", "label")
write(current)
```

Available properties: `label`, `text`, `title`, `placeholder`, `sensitive`, `visible`, `width`, `height`.

## Events (no callbacks)

Instead of callback functions, GTK signals set lil variables. You poll them in a loop.

**Critical**: The third argument to `on@gtk` must be a **quoted string** containing the variable name. Bare identifiers are evaluated as variables (their value is used, not their name).

```lil
on@gtk("btn", "clicked", "ev")
on@gtk("main", "destroy", "closed")
show@gtk("main")

loop {
  wait@gtk          : blocks until any registered signal fires
  if closed == 1 { break }
  if ev == 1 {
    set@gtk("btn", "label", "clicked!")
    ev = 0
  }
}
```

`wait@gtk` blocks until a signal fires. Then you check which variable was set and handle it. Set the variable back to 0 after handling it.

### Timeout events

Fire a signal after a delay:

```lil
timeout@gtk(1000, "tick", "t")    : fire 'tick' after 1000ms, sets t
```

## Complete Example

A window with a button that counts clicks:

```lil
include gtk

window@gtk("win", "Counter", 300, 200)
vbox@gtk("box", 10)
button@gtk("btn", "clicks: 0")
add@gtk("box", "btn")
add@gtk("win", "box")
on@gtk("btn", "clicked", "ev")
on@gtk("win", "destroy", "closed")
show@gtk("win")

count = 0
loop {
  wait@gtk
  if closed == 1 { break }
  if ev == 1 {
    count = count + 1
    set@gtk("btn", "label", "clicks: {count}")
    ev = 0
  }
}
```

## Functions Reference

| Function | Description |
|----------|-------------|
| `window@gtk(name, title, w, h)` | Create window |
| `vbox@gtk(name, spacing)` | Vertical box |
| `hbox@gtk(name, spacing)` | Horizontal box |
| `button@gtk(name, label)` | Button |
| `label@gtk(name, text)` | Label |
| `entry@gtk(name, [placeholder])` | Text entry |
| `add@gtk(parent, child, ...)` | Add children to container |
| `set@gtk(widget, prop, val)` | Set property |
| `get@gtk(widget, prop)` | Get property (returns string) |
| `on@gtk(widget, signal, var)` | Register event, sets var on trigger |
| `wait@gtk` | Block until next signal |
| `run@gtk` | Non-blocking signal check |
| `timeout@gtk(ms, signal, var)` | Fire signal after delay |
| `show@gtk(name)` | Show widget |
| `quit@gtk` | Quit GTK main loop |
| `destroy@gtk(name)` | Destroy widget |

All arguments must be quoted strings. Signals: `clicked`, `changed`, `activate`, `enter-notify-event`, `leave-notify-event`, `focus-in-event`, `focus-out-event`, `destroy`
