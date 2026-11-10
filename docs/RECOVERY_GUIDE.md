# RECOVERY GUIDE — FM-1 / AC791N

A device that will not boot, or that boots only into an update loader, is recoverable in most cases.
This page orders the known methods by risk and states what each one can and cannot do. **Nothing
here was executed on hardware during this work.** `[HARDWARE]` items are procedures, not results.

The golden rule: **do not write to the flash head, the bootloader or the ROM.** Every method below
works around them, never through them.

Which paths exist, what each can read and write, which of them a browser can reach at all, and how
big the unprotected window of a stock install is: `docs/FM1_MODES.md`. It separates what the
community projects have demonstrated on their own units from what has been demonstrated on this one.

---

## 0. Decide which state you are in

| Symptom | State | First action |
|---------|-------|--------------|
| Boots, plays, USB-MIDI answers | running | normal install, backup first |
| Appears as `ota-FM-1_900` | this project's update loader (an install was interrupted) | re-run the installer: it resumes. The loader reads the chip itself before erasing anything, so nothing has to be loaded first. |
| Appears as `ota-FM-1_015` | the unit's own update mode | do **not** feed it a Felucca image: power-cycle without holding keys and install only from a unit running Quantum GammaWave, or use M-Flasher |
| Appears as `FM-1_015` after a failed install | stock firmware, app broken | install a package again |
| No USB device, blank screen, no LEDs | boot chain | USB_KEY / Mask-ROM or an external programmer |
| Boots, then resets in a loop | record left behind | clear the update record (loader does this), then reinstall |

Recording the symptom precisely (LEDs, screen, what appears in `lsusb` / MIDI devices) is worth more
than any single tool.

## 1. Normal update (lowest risk)

`tools/fm1_install.py` or the web installer. The loader preserves the update record across power
loss, so an interrupted write is resumable and nothing else is touched. This is the only path this
project tests in software.

## 2. The loader's own escape hatch

Holding **UBOOT** (the soft key in `usb.uboot_req`) makes the firmware detach and jump to the mask
ROM's UBOOT. It is implemented in both the firmware and the loader and needs no cable tricks. If the
mask ROM answers, the device is not bricked.

## 3. Recovery references (read them, do not copy code blindly)

| Project | What it is for | Licence / note |
|---------|----------------|----------------|
| <https://sbegueria.github.io/fm1-dev-wiki/Recovery.html> | community recovery wiki: the ladder, what each method needs | documentation, read and follow |
| <https://github.com/jvitkauskas/m-bootloader> | bootloader research for this family | check the licence before reusing any code |
| <https://github.com/AL-255/FM-1-RE> | reverse engineering notes | reference only |
| <https://github.com/joakimeriksson/fm1-emu> | emulator: useful to understand the boot chain, **not** a recovery tool | GPL-3.0; a host emulator cannot unbrick a part |
| M-Flasher, MvaveFM1Unbricker, FM-1 Transporter, `fm1_rescue` | vendor/community flashing tools | verify the source and the licence before running anything |

**USB_KEY / Mask ROM**: the SoC's mask ROM has a USB download mode entered by holding the documented
key combination at power-on. It talks to the vendor's programming tool, not to this firmware. It is
the last software-only resort.

**External programmer**: an SPI clip or a soldered header with a 3.3 V programmer. Read the part
first and keep the dump; write only the regions you must. Electrical risks: level mismatch, hot-plug
with the device powered, and clip contact during a write. Use a bench supply with current limiting,
and never power the board from both USB and the programmer.

**RP2040 / XIAO RP2040, Raspberry Pi Debug Probe**: these can drive SPI or a debug interface, but
they are not certified for this board; wiring, voltage and pinout must be confirmed on the actual
unit before anything is connected. No procedure here prescribes a pinout that was not verified.

## 4. What must be true before a laboratory recovery attempt

1. A known-good full dump exists (from the same unit, before the failure) **or** the stock V15
   package (`FM-1.fwsc`, hash `db1642b2…`) is available.
2. The flash part is identified as one of the documented 1 MiB parts; an unknown part stops the
   procedure.
3. The board is powered from a current-limited supply and nothing is hot-plugged.
4. Everything is logged, and the log is kept with the unit.

## 5. What this repository will not do

* Modify the bootloader or the ROM to improvise a recovery.
* Add a "force" path that writes to the head or the bootloader.
* Copy recovery code from another project without checking its licence.
* Claim that a simulated power loss equals a physical one.
