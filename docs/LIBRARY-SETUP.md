# Supply your own compatible library

The public ZIP contains no sample banks, factory patches, wavetables, activation keys or original plugin executables. The separate private backup made for the developer must NOT be uploaded.

## Prepare
Use an existing authorized content folder containing `Patches/`, `Samples/` and, where applicable, `Wavetables/`. An installer or ISO is not accepted by these tools. No automatic vendor installation, extraction or protection bypass is provided.

Build `hyperion-catalog` using CMake (see README). On Windows the executable is under `build/native/Release/`; on Linux it is under `build/native/`. Run it with the content directory as its sole argument. For example:

```powershell
.\hyperion-catalog.exe "D:\My Content"
```

It tests ordinary presets and writes catalog.tsv at that folder's root. Unsupported/wrapped data is skipped with a reason. It keeps the prior catalog as catalog.tsv.backup and refuses to replace an existing backup. The scan can take several minutes. This makes a list, not copies or derivative sound content. It doesn't guarantee full original synthesis/FX/modulation matching.

Copy the prepared folder to your own MPC drive, for example `/media/MYDRIVE/SGK_Hyperion_Library`. Drive names differ; use your actual mounted path.

## Select the folder
After installing the plugin, run its helper on the MPC:

```sh
sh '/sdcard/Synths/SGK - VST - SGK Hyperion/Tools/configure-library.sh' '/media/MYDRIVE/SGK_Hyperion_Library'
```

If you installed elsewhere, change the plugin path too. The helper validates the folder and writes `User/library-path.txt` beside the plugin. Reload the plugin instance afterward. No system-wide mount, firmware edit or MPC restart is performed by this helper. User configuration is preserved across standard upgrades/uninstalls.

Alternatively, place your prepared content at `Library/` inside the plugin folder. Do not do both unless you intend the configured external path to take priority. The environment variable SGK_HYPERION_LIBRARY is an optional developer override; a normal customer need not set it.

Without compatible content the player remains silent and shows 'Configure library first'. This is expected. Reopen the instance after configuring; a background rescan while that same instance remains open is not implemented.

## Private backup
Extract SGK_Hyperion_Library from your private backup onto your own drive, then select that directory using the helper. It already contains catalog.tsv. Do not upload that archive, catalog or its vendor banks to the public GitHub repository or Open MPC release.
