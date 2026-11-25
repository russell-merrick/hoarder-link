# Hoarder Link

A VST3 and Audio Unit plugin that connects Hoarder to the open DAW. Previews play through the session, on the host clock. Pitch and the half-time or double-time choice stay in Hoarder.

The plugin is the piece a desktop app cannot do by itself: read the live tempo and playhead, and make sound inside the session.

Hoarder itself stays in [hoarder](https://github.com/russell-merrick/hoarder). This repo is the plugin, the localhost protocol, and the install step.

## Behavior

The plugin sits on a MIDI track. Hoarder must be open. A link control turns blue when the plugin is connected. Clicking it disconnects, and previews go back to the browser.

| Feature | Behavior |
|---|---|
| Tempo | Reads the project tempo from the host. Previews play at that tempo. |
| Transport | If the DAW is playing or recording, the preview follows the metronome. |
| Key | You pick the project key in the app. Samples that have a key are transposed. The plugin does not detect the key. |
| Pitch | Manual semitone shift, as well as the key match. |
| Time | Matched to the project tempo. |
| Loops | Repeat until you stop them. |
| One-shots | Play once. Note repeat retriggers them from every 16th note out to every 4 bars. |
| Drag | The original sample. Pitch and time changes do not ride along. |
| Copy | "Copy modified sample" puts the pitched and stretched file on the clipboard. Paste that into the DAW. |
| Audition | Audio comes out of the plugin track, so the DAW's monitoring and effects are in the path. |

Targets Windows 10+ and macOS 10.15+, as VST3 and AU, in Ableton Live 10+, Logic Pro X 10.5+, Studio One 5+, FL Studio 20.8+, and GarageBand 10.4+. No AAX plugin, so Pro Tools is out.

## What Hoarder already does

Hoarder previews in the browser and can bake pitch and tempo into the file you drag. Tempo today comes from the BPM saved in the Ableton set on disk, and it updates after a save. The browser plays on its own clock, so a preview does not sit on the downbeat while Live is running.

Link replaces that tempo source while it is connected. The saved-set BPM remains the fallback when no plugin is loaded.

## Target behavior

Load Hoarder Link on a MIDI track. Hoarder shows a link mark. Green means a plugin is connected.

- The header tempo is the host tempo, including changes that are not saved yet.
- Play still uses Hoarder's To key. Relative keys do not move, same as today. There is no audio key detector.
- With the transport stopped, a click still starts the preview. Stopping the transport stops it.
- With the transport running, a loop locks to the beat grid.
- Loops repeat until you stop them or pick another sample.
- One-shots play once unless note repeat is on. Repeat rates are 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars, and 4 bars.
- Drag from the list inserts the original file.
- Copy modified writes the pitched and stretched WAV and puts that file on the clipboard.
- Disconnecting the link returns preview to the browser. The browser drag can keep today's baked-in pitch and tempo.

One plugin instance is the active link. A newly connected instance replaces the previous one.

## Design

```
DAW
  Hoarder Link (VST3 / AU instrument)
    reads tempo, PPQ, play state, meter, sample rate
    plays the current preview on the host clock
        |  localhost WebSocket, 127.0.0.1 only
Hoarder (existing app, port 8770 for the UI)
  link service on 127.0.0.1:8771
    renders pitch and tempo with the ffmpeg rubberband path already in stretch.py
    hands the plugin a WAV path
    link state, host tempo, stretch, and note repeat live in the page
```

The plugin does not stretch. Hoarder renders a WAV at the chosen pitch and at the project tempo. The plugin loads that file on the message thread and plays it from the audio callback. Loop position while the transport is running comes from the host PPQ, not from a free-running playhead.

The audio callback never opens a socket and never reads the file. Host state is stored in atomics and posted to Hoarder about 20 times a second.

### Messages

Plugin to Hoarder:

- `hello` with plugin format, host name, and a session id
- `host` with bpm, ppq, playing, recording, meter numerator and denominator, sample rate
- `bye` on unload

Hoarder to plugin:

- `preview` with the rendered WAV path, whether it is a loop, its length in beats, and the one-shot repeat rate
- `stop`
- `clear` when the link is disconnected from the page

Paths stay on the same machine. The socket accepts only `127.0.0.1`.

`python standin.py` is the stand-in for that socket. Hoarder must already be running. It sends `hello` and `host` about 20 times a second, prints each host line, and checks that a `preview` path is a WAV. `--until-preview` exits after the first good preview. It does not play audio.

## Platforms

| | Windows 10+ | macOS 10.15+ |
|---|---|---|
| Format | VST3 | VST3 and AU |
| Needed for | Ableton, FL, Studio One | Logic and GarageBand need the AU. Ableton can use the VST3. |
| Install | User VST3 folder, and a copy Hoarder can place for hosts that only scan the common VST3 folder | `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components` |

No AAX. Pro Tools is out of scope.

The plugin is built with JUCE and CMake so one codebase produces VST3 and AU. GitHub Actions builds Windows and macOS. An unsigned VST3 loads in Ableton on Windows for development. A Mac build that other people can open needs Developer ID signing and notarization. That is part of the install phase, not the first audible version.

## Work split

In this repo:

- The JUCE instrument
- The WebSocket client
- PPQ scheduling, loop wrap, and one-shot repeat
- CMake, the two platform builds, and the installer copy step

In hoarder:

- The link service on port 8771
- Route preview to the plugin while the link is on
- Host tempo on the existing project readout
- Stretch control and note repeat
- Copy modified, and original-file drag while linked. Windows clipboard is in. The macOS clipboard is still to do.

## Build the plugin

The VST3 connects to Hoarder, reports the host tempo, and plays the preview Hoarder renders. Loops follow the host beat grid while the transport is running. One-shots play once, or retrigger at the repeat rate. Repeat and key are chosen in Hoarder.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The build copies `Hoarder Link.vst3` to `%LOCALAPPDATA%\Programs\Common\VST3` on Windows, and to `~/Library/Audio/Plug-Ins/VST3` on macOS. In Ableton, Preferences, Plug-Ins, enable the VST3 system folders and rescan. Load Hoarder Link on a MIDI track while Hoarder is open. The link button turns blue, and the header tempo follows the open set.

## Build order

1. **Protocol.** A Python stand-in for the plugin connects to 8771, prints `host` messages, and checks that a `preview` path is a WAV. No DAW yet.
2. **Silent plugin.** VST3 instrument loads in Ableton on Windows, connects, and Hoarder shows the live tempo.
3. **Playback, transport stopped.** Clicking a sample plays the rendered WAV through the plugin at the project tempo.
4. **Grid lock.** With the transport running, a loop starts on the beat and holds tempo changes.
5. **Repeats and stretch.** One-shot repeat rates, and 1/2x and 2x.
6. **Clipboard and drag.** Windows puts the modified WAV on the clipboard, and a linked drag passes the original file. The macOS clipboard is still open.
7. **Mac and install.** AU plus VST3 from CI, user-folder install, then signing and notarization.

## Out of scope

- Detecting key by listening, or by reading a Live scale. To key stays the control.
- Playing Hoarder's audio through the plugin while the link is disconnected.
- AAX, cloud sample streaming, and an account system.
