# Partitura del Juego — macOS test package

This package is for direct testing on an Apple Silicon Mac. It contains the
visual application, its shaders and video library, plus the SuperCollider
scripts. openFrameworks is not required on the test Mac.

## Requirements

- Apple Silicon Mac (M1 or newer)
- macOS 11 or newer
- SuperCollider, only if audio testing is required

The package is ad-hoc signed but not Apple-notarized. It is intended for
private testing, not public distribution.

## First launch

1. Copy the ZIP to the test Mac and unzip it.
2. Move the extracted folder somewhere writable, such as the Desktop.
3. Control-click `Partitura_del_Juego.app`, choose **Open**, then confirm
   **Open** in the macOS dialog.
4. Allow any requested camera or microphone permissions.

If macOS still blocks the app, open Terminal and run:

```sh
xattr -dr com.apple.quarantine "/path/to/Partitura_del_Juego.app"
```

Then Control-click the app and choose **Open** again.

The test package starts in a 1920 x 1080 single-window layout. It displays the
four channels side by side, allowing the program to be tested without the full
multi-monitor installation.

Display configuration is saved outside the signed application bundle at:

`~/Library/Application Support/PartituraDelJuego/settings.json`

This file is created from the packaged defaults on first launch. To reset all
settings, quit the application, delete that file, and launch the application
again.

## Audio with SuperCollider

1. Install and launch SuperCollider.
2. Open `SuperCollider/pdj_datamatics.scd` from this package.
3. Place the cursor inside the outer parentheses and evaluate the block with
   Cmd+Enter. Wait for the server to boot.
4. Launch `Partitura_del_Juego.app`.

The visual app sends OSC to `localhost:9001`, and the included SuperCollider
engine listens on UDP port 9001. Stop the audio engine with Cmd+Period.

## Smoke test

Confirm the following:

- The app opens without requiring openFrameworks or the source repository.
- One presentation window and one control window appear.
- All four channel areas play video.
- The control panel reports 48 available clips.
- B&W shaders and visual modes render without shader errors.
- Modes rotate automatically without showing the temporarily disabled
  SlitScan mode.
- If SuperCollider is running, OSC activity produces audio on all four
  channels.

## Performance test

Open the **Performance** page in the control window and select **Start
10-minute stress test**. Keep the presentation windows visible and avoid
moving windows or launching other applications during the run.

The test warms up, then exercises normal video, the expensive line and number
modes, slit-scan with clip changes, and procedural generators. A production
pass requires stable 30 FPS pacing with no frame longer than 100 ms. CPU,
memory, GPU draw time, and per-channel subsystem costs remain visible during
the run.

At completion the page shows PASS or FAIL and the specific threshold failures.
JSON and CSV files are saved under
`~/Documents/PartituraDelJuego/performance_reports`. Press **Stop test** to
finish early and save a partial report; closing the application during a run
does the same. Reset is disabled while a test is active so the original visual
state is always restored. Use the JSON report when comparing computers because
it contains the settings snapshot, frame percentiles, phase results, memory
growth, and the slowest subsystem.

The packaged four-channel layout is useful as a smoke test. The installation
computer must also be tested in `dualWindow8` with both ICUIXIAN outputs
connected; only that run represents the production load.

## Troubleshooting

### No videos

The package must retain this internal folder:

`Partitura_del_Juego.app/Contents/Resources/data/cortos`

Do not move files out of the `.app` bundle.

### No audio

- Start SuperCollider before the visual app.
- Confirm `pdj_datamatics.scd` reports UDP port 9001.
- Ensure no other process is already using port 9001.
- Check the selected SuperCollider audio output device.

### Full installation layout

This test build intentionally uses a laptop-safe single window. Installation
display geometry and remote OSC hosts should be configured in the project
settings or with the control panel. Runtime changes are stored in the user's
Application Support folder and do not modify the signed `.app`.
