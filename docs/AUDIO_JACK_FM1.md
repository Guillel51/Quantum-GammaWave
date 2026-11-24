# FM-1: the headphone jack and the internal speaker

**Class of every statement below**: reading of this repository's sources and HAL. No FM-1 was
available, so nothing here is a measurement of a unit, and none of it is a claim about a circuit
nobody has documented in the repository.

## What the firmware does with audio out

- One stereo stream, one output: `audio.c` renders the master into the ALNK0 buffer and calls it
  "usb_fixed_dac" (`audio.c:122`); `click.c:9` names the same destination "headphones / line out, the
  speaker". There is no second bus, no separate speaker mix and no per-output gain anywhere in
  `firmware/src`.
- The transport is I2S out of ALNK0 into an **external codec** (`firmware/hal/fm1_audio.h`, header
  comment), so level, mute and routing live in that codec and in whatever amplifier follows it.
- The firmware's only output-side control is `fm1__pc_out(6, v)`: the codec's **output stage mute**,
  PC6, used to keep the output silent while the codec comes up and to unmute it 25 ms later, on a DAC
  that has been running on a zeroed buffer (`firmware/hal/fm1_audio.h:66-84`;
  `docs/BOOT_SPLASH_AND_POP.md`, `docs/AUDIT_BOOT_v1.4.11.md`). That mute is **downstream of both
  outputs**: muting it silences the headphones as well as the speaker, which is why it cannot serve
  as a speaker switch.
- There is **no jack-detect line** anywhere: `firmware/hal/fm1_gpio.h` names the ports (PA, PB, PC,
  PH) and `firmware/hal/fm1_input.h` documents every pin the firmware uses (matrix rows PA0, PA5..PA8,
  PB7; 595 lines PA4/PA3/PA1; LED lines PH6/PH9/PA9/PA10; codec control PC0/1/2/6; MIDI UART on PH8).
  No pin is read as a plug, no `fm1_jack_*` API exists, and nothing in `firmware/` mentions
  "headphone", "jack" or a detect contact.
- The JieLi SDK in this environment (`$AC79_SDK`) holds only `cpu/wl82/tools`; its audio headers are
  not here, so the SDK's own jack/HP-detect API (if it has one) could not be read either.

## What that means, without dressing it up

**Automatic switching cannot be implemented in software from this repository.** Two things are
missing and neither is inferable from the code:

1. **A detect source.** Either the 3.5 mm jack is a *switched* jack whose contact physically
   disconnects the internal speaker when a plug is in (then the hardware already does the job and the
   firmware must do nothing), or the contact is wired to a GPIO and the firmware has to read it. Which
   one the FM-1 has is a property of the board, not of the firmware.
2. **A speaker-only switch.** PC6 mutes the codec's output stage, i.e. both outputs. A speaker-only
   cut needs the amplifier's own enable line (a second GPIO, or the codec's speaker-amp register) —
   the audit in `docs/AUDIT_BOOT_v1.4.11.md` already notes the amplifier's own click was never
   measured, which is consistent with the amplifier not being under firmware control today.

Faking it is worse than not having it: muting PC6 on a jack event would silence the headphones the
player just plugged in, and a software "duplicate the stream" would be audio the hardware never has.

## How to settle it on a unit (10 minutes, no writes)

1. Play a steady tone. Plug headphones in. If the speaker goes quiet with the jack in and returns
   with it out, the jack is switched: **nothing to implement**, and the firmware's behaviour is already
   correct. Record it in `docs/VALIDATION_MATRIX.md` as a hardware property, not a firmware feature.
2. If the speaker keeps sounding with the plug in, the two outputs are wired in parallel and there is
   no detect contact in use. Take the unit's schematic (or trace the jack's third contact) and find
   which port pin, if any, it reaches; then the firmware needs that pin plus an amp enable.
3. Whichever the answer, confirm the level of both outputs with the same tone: the DAC is a single
   stream, so any difference in loudness between them is the hardware's, not the mixer's.

## What is implemented here

Nothing in the signal path changed: the working mute/unmute at boot, the 27 ms boot fade and the
master ramp are the only things that touch the output, and they were already correct (FASE 3, `audio.c`
and `fx.c`). Adding a switch on top of a detect that does not exist would have been a lie with a
speaker in it.

Open, and named so it is not forgotten: **FM6 voices are not always freed after note-off**
(`tests/regress` HEALTH FAIL on `FM6/00_SILT_KEYS` and `FM6/31_FERROUS_PAD`, and
`tests/fm6_test` "all free some seconds after the release"). That is a real audio defect and the next
piece of work; it is not a jack problem.
