# Background tasks in EmulationStation-X

ES-X keeps its normal RetroPie/EmulationStation interface usable while certain
long-running jobs continue. A compact global notification reports the current
job, its context and its status using the bundled Font Awesome font. This is
part of ES-X; it does not require installing Batocera or a separate task manager.

## Supported operations and controls

| Component | How to return to the carousel | How to revisit the job | Notes |
| --- | --- | --- | --- |
| Native scraper | Choose **BACKGROUND**, or press **B** while auto-acceptance is enabled | Main menu → Scraper → Scrape Now → **Resume** | Background mode needs automatic result acceptance. Manual review stays visible because it requires decisions. The original **Stop** control remains available. |
| Skyscraper | Press **B** in the running progress dialog | Open the Skyscraper menu for available foreground controls | **Y** opens the stop confirmation while its progress dialog is displayed. Gathering and gamelist generation are distinct operations. |
| Theme Browser | Press **B** while downloading/updating a theme or catalog | Main menu → UI Settings → Theme Downloader | Uses an indeterminate bar; Git clone's percentage is not reported as a reliable total. |
| X-TRAS free-game downloads | Press **B** during a download | Main menu → X-TRAS | The ROM is downloaded to a temporary file and promoted only after success. |
| X-TRAS ports | Press **B** during install/update via RetroPie-Setup | Main menu → X-TRAS | Installs use noninteractive `sudo -n`. **Removal stays foreground-only.** |

**Important:** B hides a working dialog; it does not terminate the underlying
job. In X-TRAS, do not quit ES-X while RetroPie-Setup is installing/building a
port. A port may need system resources and exclusive access to its own files.

## Status and progress

- **Running:** the blue floating panel shows the action, game/theme/port,
  and a stage or other relevant status.
- **Completed / failed / stopped:** the terminal icon and message distinguish
  outcomes. A completed background task can be dismissed with the next input.
- **Scraper:** uses its actual completed/total count.
- **Theme downloads:** uses animated *indeterminate* progress.
- **X-TRAS ports:** parses RetroPie-Setup's log for stages such as dependency
  installation, fetching sources, compilation and installing files.
  Any displayed numeric percentage is for the **current stage only**, never
  a fabricated percentage for the overall installation.
- The bundled Font Awesome glyphs are shared with Theme Browser and X-TRAS;
  no separate icon PNG files are required for the task statuses.

If multiple operations are running, the common overlay cycles through their
statuses. Individual background GUIs remain owned by the main Window and are
updated on the UI thread to avoid dangling GUI references. Operations started
from Theme Browser, X-TRAS, and the native scraper have separate lifetimes.

## Update safety

Theme Browser now downloads replacement themes to a staging directory and
promotes the new version after a successful clone. It preserves the previously
installed theme if cloning or promotion fails. Catalog ZIP updates similarly
extract before replacing an existing catalog.

An interrupted operation may leave a `.esx-update-backup` directory; when
present, Theme Browser refuses to overwrite that backup automatically so it
can be inspected or restored manually.

## Language behavior

ES-X's selected language comes from the `Language` setting. Existing menu
windows may retain labels created *before* the language was changed, even
though a newly displayed notification uses the new translation. Restart ES-X
after switching languages for a fully consistent interface.

Port descriptions are obtained from the upstream RetroPie-Setup module and
are not necessarily translated. Background X-TRAS notifications display a
short port name (for example, **GZDoom**) instead of the full English module
description.

## Test record

The following are **manual observations** reported while testing the
experimental branches, not automated regression-suite results:

| Scenario | Observation |
| --- | --- |
| Native scraper continues while browsing another system | Observed working |
| Native scraper status persists after launching a game and returning | Observed working; the counter subsequently advanced |
| Skyscraper background activity notification | Observed working during resource gathering |
| Theme Browser download/update while browsing the frontend | Observed working |
| Downloaded and updated themes change correctly | Confirmed |
| X-TRAS install/update in the background | GZDoom installation observed |
| X-TRAS completion notification | GZDoom success notification observed |
| X-TRAS port stage identification and glyph | Observed in screenshots |

### Additional smoke tests recommended

1. Confirm a fresh build on desktop Linux and an ARM64 RetroPie machine.
2. Verify native scraper's **Stop**, completed/failed states and automatic
   conflict-handling requirement.
3. Verify Theme Browser **resume**, failed-network rollback and catalog refresh.
4. Verify X-TRAS free-game download, failed download recovery, failed port
   install and log link/path; ensure no duplicate installation can be started.
5. Verify multiple jobs running together (including starting/returning from a
   game), navigation responsiveness and theme/font compatibility.
6. Verify language switching and a clean restart in both Spanish and English.
7. Confirm no background installation is abruptly interrupted by an ES-X quit.

This feature is intentionally lightweight: it adds no global destructive
cancel shortcut, no invented percentages, and no new standalone task manager.
