# `mupen64plus-video-videocore`

`mupen64plus-video-videocore` is a Nintendo 64 graphics plugin for Mupen64Plus optimized for modern
mobile GPUs, especially the Broadcom VideoCore IV used in the Raspberry Pi (though it is not just
limited to the Pi and should improve speed on any mobile GPU). Using it, many games run at full
speed or nearly full speed on the Raspberry Pi 2. It's descended from the venerable
[glN64](http://gln64.emulation64.com) plugin, like `mupen64plus-video-gles2n64`. Unlike that
plugin, the rendering backend has been fully rewritten and features the following optimizations:

* An "ubershader" that replicates the functionality of the RDP color combiner without the need to
  generate shaders on the fly. This eliminates shader changes while rendering the scene.

* A texture atlas manager that reduces the number of texture state changes during scene rendering
  to zero.

* A dedicated rendering thread to work around driver stalls (which are very frequent using
  Broadcom's drivers).

* A modern batcher that tries to minimize the number of draw calls and make optimum use of vertex
  buffer objects.

* A renderer that renders at the N64 native resolution (320x240) and scales that picture up to the
  desired resolution. This reduces fragment shading and raster operation load (at the expense of
  picture quality).

This plugin is incomplete and has many bugs. Most games have varying degrees of graphical issues.

## Compatibility

A partial compatibility list:

* *Super Mario 64*: very good, a few minor graphical issues, full speed

* *Star Fox 64*: good, a few minor graphical issues, some slowdown in non-critical places

* *Mario Kart 64*: good, a few minor graphical issues, some slowdown in non-critical places

* *The Legend of Zelda: Ocarina of Time*: playable, major graphical issues, occasional slowdown

* *Super Smash Bros.*: playable, severe graphical issues

* *Banjo-Kazooie*: playable, severe graphical issues, some slowdown in non-critical places

* *GoldenEye 007*: not working, severe graphical issues, major slowdown

* *Mario Party*: not working due to incorrect VI emulation 

## Building and installing

First, ensure you have Mupen64Plus built and installed. Then, run `make` in the top-level
directory of this source tree, then `sudo make install` to install. You can then start Mupen64Plus
with `--gfx mupen64plus-video-videocore` to use the plugin.

The install process will place the plugin in
`/usr/local/lib/mupen64plus/mupen64plus-video-videocore`, a configuration file in
`/etc/xdg/mupen64plus/videocore.conf`, and shaders in `/usr/local/share/mupen64plus/videocore/`.
The shaders are required for the plugin to run.

Configuration is done via `videocore.conf`, located in `/etc/xdg/mupen64plus/videocore.conf`
(system-wide) or `~/.config/mupen64plus/videocore.conf` (per-user). The currently supported
settings are:

* `display.width`, `display.height`: Sets the display resolution that the picture will be scaled
  to. Note that the N64 native resolution is currently locked to 320x240 and cannot be changed.
  (Upscaling causes speed issues on all Raspberry Pi models.) The default is 1920x1080 (1080p).

* `debug.display`: Set to true to enable a debug display that displays moving averages of various
  statistics relevant to performance. The default is false.

## Requirements

SDL2 2.0.5 at least is necessary on Raspberry Pi 2/3.

## Contributing

Contributions to improve games are more than welcome! I likely won't have a huge amount of time to
devote to this project myself.

## Acknowledgements

This plugin exists thanks to:

* Orkin for the glN64 plugin.

* The Mupen64Plus project.

* Sean Barrett for `stb-image`: https://github.com/nothings/stb

* MAYAH for `tinytoml`: https://github.com/mayah/tinytoml 

* Troy D. Hanson for `uthash`: https://troydhanson.github.io/uthash/ 

* Yann Collet for xxHash: https://github.com/Cyan4973/xxHash 

* Ten by Twenty for the Munro font used in the debug overlay:
  http://tenbytwenty.com/?xxxx_posts=munro 

