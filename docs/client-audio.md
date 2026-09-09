# Client Audio Controls

On `master`, the Sound Options page has independent Effects Volume and Music
Volume sliders. The stock-named `mss_volume` cvar is the effects/SFX control,
and the new `musicVolume` cvar is the music control. Both are archived in the
client profile. There is no separate `sfxVolume` cvar.

A profile's initial `musicVolume` inherits its existing `mss_volume`, so the
separate control does not change that profile's established mix. Profiles that
used the former `playMusic 0` setting migrate to zero Music Volume, and the
retired cvar is removed from later config writes.

Setting Music Volume to zero silences the background-music slot, aliases on the
`music` channel, and files under `sound/music/`, while leaving other sound
effects and ambient sounds enabled. Custom maps should use that channel or
directory for music played outside the background-music slot.
