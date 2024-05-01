# twm — Directional focus switcher for Windows &nbsp;![](https://github.com/tom94/twm/workflows/CI/badge.svg)

<img align="right" src="resources/icon-256.png"/> You've perfectly arranged your windows.
Code in the middle, terminal to the left, browser to the right.
You're **flying** through your task, deep in the zone.
But then it happens: you press `alt-tab` and it focuses the wrong window.
No matter, `alt-shift-tab`, `alt-tab-tab`.
Dammit, wrong window again.
Why is this so hard!?

No more!
With **twm**, you can move focus by pressing direction keys.
Duh!
`alt-right` to move focus to the right.
`alt-left` to go to the left.
Impossible to get wrong.
Vim shortcuts?
`alt-h/j/k/l` also work.

You can also swap adjacent windows with `alt-shift-<dir key>`.

## Installation

Download the **twm** installer (.msi) or portable executable (.exe) from the [releases page](https://github.com/Tom94/twm/releases).
The installer makes **twm** autostart with Windows (can be disabled in in the system tray later) and adds it to `PATH`.

## Configuration

**twm** can be configured by a [TOML file](https://toml.io/en/) that must be placed at `%APPDATA%\twm\twm.toml`.
You can also use another path by setting the `TWM_CONFIG_PATH` environment variable.

If the config file does not exist, **twm** uses the (self-explanatory) default config:

```toml
disable_drop_shadows = false
disable_rounded_corners = false
draw_focus_border = false

[hotkeys]
alt-left = "focus window left"
alt-down = "focus window down"
alt-up = "focus window up"
alt-right = "focus window right"

alt-shift-left = "swap window left"
alt-shift-down = "swap window down"
alt-shift-up = "swap window up"
alt-shift-right = "swap window right"

alt-h = "focus window left"
alt-j = "focus window down"
alt-k = "focus window up"
alt-l = "focus window right"

alt-shift-h = "swap window left"
alt-shift-j = "swap window down"
alt-shift-k = "swap window up"
alt-shift-l = "swap window right"

alt-1 = "focus desktop left"
alt-2 = "focus desktop right"

alt-shift-q = "close window"
ctrl-alt-shift-q = "terminate window"

alt-shift-r = "reload"
```

## Styling

**twm** can add styling to make navigation easier.
Simply enable any of the following config values:

```toml
disable_drop_shadows = true
disable_rounded_corners = true
draw_focus_border = true

# RRGGBB border colors
focused_border_color = "#999999" # light gray
unfocused_border_color = "#333333" # dark gray
```

## Tiling window manager

Maybe you guessed that **twm** stands for **t**iling **w**indow **m**anager... and that would be correct!
I would like to add BSP-tree (binary space partitioning) tiling in the future.
Until then, I recommend using [FancyZones](https://learn.microsoft.com/en-us/windows/powertoys/fancyzones) (part of [PowerToys](https://learn.microsoft.com/en-us/windows/powertoys/)) to tile your windows.

Alternatively, check out [komorebi](https://github.com/LGUG2Z/komorebi), an almost fully fledged tiling window manager for Windows.
(I say "almost" because [there is no tree-based tiling](https://github.com/LGUG2Z/komorebi/issues/59) like you might be used to from [i3](https://i3wm.org/) or [yabai](https://github.com/koekeishiya/yabai).)

## Building twm

All that is required for building **twm** is [CMake](https://cmake.org/) and Visual Studio 2022 or newer.
Once both are installed, clone this repository and all its submodules using the following command:

```sh
> git clone --recursive https://github.com/Tom94/twm
```

Then, use [CMake](https://cmake.org/) as follows:

```sh
> cmake . -B build
> cmake --build build --config Release -j
```

Afterwards, you can either run `twm.exe` or you can create an installer with

```sh
> cpack --config build/CPackConfig.cmake
```

## License

GPL 3.0
