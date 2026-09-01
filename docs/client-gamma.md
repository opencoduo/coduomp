# Client Gamma Control

The default client exposes three gamma policies through the archived, latched
`r_gammaMode` cvar:

| Value | Mode | Behavior |
| --- | --- | --- |
| `0` | Off | The game does not apply gamma correction. Native display ramps, the full-frame software lookup table, the original texture-upload fallback, and gamma-dependent overbright are all disabled. Use this when an NVIDIA, AMD, monitor, or other external control owns gamma. |
| `1` | Automatic (default) | Prefer gamma on the display containing the game window, then use the full-frame software lookup table if native gamma explicitly fails. If neither final-output provider is available, retain the original limited texture-upload fallback. |
| `2` | Software | Skip native display gamma and force the full-frame software lookup table. If the required OpenGL feature is unavailable, the client warns and leaves gamma correction disabled rather than silently selecting native or limited texture gamma. |

Because `r_gammaMode` is latched, apply a change with `vid_restart` or restart
the client. The default is:

```text
seta r_gammaMode "1"
seta r_ignorehwgamma "0"
```

## Existing gamma cvars

`r_ignorehwgamma` remains permanently supported for compatibility with old
configs and documentation. It is a hardware veto, not a master gamma switch:

- `r_ignorehwgamma 0` allows native display gamma in Automatic mode.
- `r_ignorehwgamma 1` never permits the game to change an OS/native hardware
  gamma ramp. Automatic mode selects software gamma instead.
- Off and Software modes already skip hardware, so `r_ignorehwgamma` has no
  additional effect in those modes.

`r_ignorehwgamma` is also archived and latched. The complete precedence is:

| `r_gammaMode` | `r_ignorehwgamma` | Effective policy |
| --- | --- | --- |
| `0` | either | Gamma off |
| `1` | `0` | Native display gamma, then software fallback |
| `1` | nonzero | Software gamma; original texture fallback only if the full-frame pass is unavailable |
| `2` | either | Forced full-frame software gamma |

`r_gamma` controls the curve applied by whichever provider is selected. Its
valid range is `0.5` through `3.0`; `1.0` is the neutral identity curve. Values
above `1.0` brighten midtones, and values below `1.0` darken them.

`r_overBrightBits` is the stock fixed-function overbright mechanism, not HDR.
The recovered renderer enables one effective overbright bit only in fullscreen
when a final-output gamma provider is active. Off mode therefore makes the
effective overbright value zero even if the archived cvar remains `1`.

## What each provider does

A lookup table (LUT) is a fixed array that maps each input channel value to an
output value. The client computes a 256-entry red, green, and blue mapping from
`r_gamma` and overbright state, then gives that mapping to one of these paths:

1. **Native display gamma** changes the monitor/display pipeline. On Windows,
   the client uses the GDI device for the monitor containing the game window;
   on SDL platforms it first uses the window-associated display. Linux can
   additionally target the window's XRandR CRTC. This is the closest behavior
   to the original game.
2. **Full-frame software gamma** copies the completed framebuffer and maps all
   three channels through the exact renderer LUT in an OpenGL fragment-program
   presentation pass. It is window-scoped and does not alter another monitor
   or desktop application, but it is a rendering pass rather than hardware
   display gamma.
3. **Original texture-upload fallback** applies the mapping on the CPU while
   eligible textures are loaded. It is retained only as the last Automatic
   fallback. It is limited because it cannot correct every contribution to the
   completed frame and can therefore look different from native or full-frame
   gamma.

Automatic provider order is:

| Platform | Provider order |
| --- | --- |
| Windows | Game-monitor GDI ramp, full-frame software LUT, original texture fallback |
| Linux/X11 | SDL display ramp, game-window XRandR CRTC, full-frame software LUT, original texture fallback |
| macOS | SDL display ramp, full-frame software LUT, original texture fallback |

On Windows, `SetDeviceGammaRamp` can report success even when the driver or
display path does not visibly apply the ramp. The client can automatically
fall back after an explicit API failure or a broken captured ramp, but it
cannot reliably detect that silent-success case. Use `r_gammaMode 2` to force
the full-frame software path, or `r_gammaMode 0` when external controls should
own gamma.

This guide describes the `master` branch. The `stock` branch retains the
recovered original gamma selection and fallback behavior and does not register
`r_gammaMode`.
