# fm1-chord

A custom firmware that turns the M-VAVE FM-1 pocket synth into a chord-machine style instrument.
Built on [Felucca](https://github.com/hugelton/Felucca). Clean-room: nothing from any other instrument's firmware.

**White keys play chords, then strum them.** The seven white keys at the left are the scale degrees
in the current key and scale, so every chord fits: the first is I, the next ii, up to vii. The nine
white keys to their right are a strumplate, as on an Omnichord: each plays one note of the chord
last played, rising from key to key, so a swipe across them strums it. In LEAD every white key is
a note of the scale instead, up from the tonic, as many at once as you hold. The screen names the chord (or the note) and shows
it on a piano; at rest it shows what every key does.

**Black keys change them.** The eight black keys of the middle octaves are the chord machine's joystick
directions (7ths, 9ths, sus, dim, aug, borrowed chords, in four tables); F#3 cycles the inversion
(C, C/E, C/G), G#3 locks a change to a key, A#3 holds. With BASS on SLASH (the default), the key held
first is the bass and the next one the chord: Em/C.

**Everything else a chord machine does:** Play, Strum, Lead, Drone, Arp, Repeat, a 16-step chord
sequencer, drum pads, 56 drum loops, a 4-layer looper, a mixer, Chord Hiro and the Ear Trainer; 37 sounds
(36 classic ones, plus an acoustic guitar for leads),
7 envelope presets, reverb, delay, chorus, flanger, tremolo, vibrato, glide, drive, tape, stereo, a filter
wheel; key, scale, bass, voices, voice leading; four presets; MIDI in and out. Only the mic features and
Bluetooth are missing.

**Controls.** Four layers, as in SLOOP and JIANT: **ALGORITHM** picks layer 1 to 4, each with its own
sound and play style, and the looper records into the selected one. The menu buttons, colour-coded, open
their menu when tapped and are a layer when held: the white keys and KNOB 1 to 4 change job while the
button is down, their map on the screen. **SCL** key (any key sets the key; knobs scale, bass, voices,
joystick), **FX** sound (a key a sound, 16 a page; knobs page, envelope, reverb, delay), **EDIT** mode
(keys 1 to 11 the play style: PLAY, STRUM, LEAD, DRONE, ARP, REPEAT, SEQ, DRUM, LOOPS, HIRO, EAR; knobs
strum, arp, rate, kit), **GLO** mix (keys 1 to 4 mute the layers, 7 the metronome, 16 tap tempo; knobs the
levels; tapped: the MIX screen), **SAVE** presets (keys 1 to 4 load P1 to P4, 5 to 8 save them, each with its loop). PRESETS
picks the sound from a list, ENV and LFO open theirs, KNOB 1 to 4 are filter, resonance, attack and
release with the value shown as you turn (KNOB 4 sets an effect's amount on its row in the sound menu);
tempo is on the mode menu and EDIT tapped three times. REC on a layer that already plays arms the next
empty one; REC held clears the layer, held on to 2 s everything. Your settings and the loop come back after
power-off (the loop paused: PLAY). The presets' loops live in Felucca's third user sample slot, as JIANT keeps its
songs there: two user sample slots remain (USR1, USR2). HOME held gives you Felucca's full synth underneath; hold HOME and press SAVE to come back to the chord UI.

| | | |
|---|---|---|
| ![home](screenshots/home_c7.png) | ![slash chord](screenshots/home_slash.png) | ![inversion](screenshots/home_inverted.png) |
| ![effects](screenshots/home_fx.png) | ![sound list](screenshots/pick_sound.png) | ![envelopes](screenshots/pick_env.png) |
| ![key menu](screenshots/menu_key.png) | ![sound menu](screenshots/menu_sound_amount.png) | ![mode menu](screenshots/menu_mode.png) |
| ![looper](screenshots/menu_looper.png) | ![sequencer](screenshots/mode_sequencer.png) | ![drum loop recording](screenshots/mode_drumloop_rec.png) |
| ![drums](screenshots/mode_drum.png) | ![at rest: the key legend](screenshots/home_idle.png) | ![chord hiro](screenshots/mode_hiro.png) |
| ![ALGORITHM: the four layers](screenshots/pick_layers.png) | ![FX held: a sound on each key](screenshots/layer_sound.png) | ![EDIT held: the play styles](screenshots/layer_mode.png) |

![controls](screenshots/controls.png)

**Themes.** The screen takes every colour from a palette: SOUND > THEME switches between them (LAPIS, the
default, plus Felucca's eight). A palette is one row in `tools/gen_ui_palettes.py`: background, surface, text,
theme and accent, from which the tints are derived, plus the chord UI's role colours (KEY, SOUND, MODE, PRESET,
LOOP, OK) and the seven degree colours in `CHORD`. A palette without its own role colours gets the classic ones.
The tool checks the contrast of every pairing and fails the build on a miss. The type is Chakra Petch (SIL OFL),
as in [Tessera](https://github.com/Rastapopolix/tessera), whose palette LAPIS follows.

Status: running on an FM-1 (installed over USB-MIDI with `tools/fm1_install.py`, 28 % CPU at idle)
and tested on the host simulator. It also runs in the
[FM-1 emulator](https://github.com/simonjohansson/fm1-emulator) with a patch set on its released
build that adds the knobs and WAV recording: `tools/emulator/run.sh` fetches, patches, builds and
launches it. Factory reset: hold HOME + SAVE while powering on, or `factory yes` on the USB console.
Spec and notes in [spec/docs](spec/docs); build, flash and the findings on the unit in
[spec/docs/05-dev-setup.md](spec/docs/05-dev-setup.md) and [spec/docs/11-hardware-findings.md](spec/docs/11-hardware-findings.md).
GPL-3.0, as Felucca.
