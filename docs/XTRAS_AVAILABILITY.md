# X-TRAS platform availability

Ports unavailable according to the installed RetroPie-Setup rules have a salmon
name and warning triangle, plus a textual availability label. Accept opens package
flags instead of starting installation/update. Free games are unaffected.

One asynchronous query discovers the platform/OS/compiler and calls the installed
`rp_registerAllModules`; X-TRAS consumes its `enabled` values. Rules are not
reimplemented in C++. The query runs as the current user, without sudo, dependency
installation, GPG imports, or framebuffer changes. It does not call `setup_env`.
The dependency request in OS discovery is suppressed, as is legacy Pi pkgconfig
cleanup. Existing local platform configuration and module registration scripts are
loaded, just as in Setup.

A failed/incomplete query leaves ports as **Compatibility unverified**, never as
confirmed compatible. There is a 20-second timeout. Refresh/close cancels the query
process group; result publication and UI changes happen on the main thread.
The query is repeated when refreshing the list. Built-in modules take precedence
over duplicate external IDs, matching Setup.

Availability means the module is enabled by Setup flags, not that compilation,
graphics drivers, game data or runtime performance have been tested.

## Validation

- `python3 tests/test_xtras_compatibility.py`: isolated query success/failure tests.
- Worker extracted and compiled with C++11/pthreads: available/unavailable states
  and cancellation of a sleeping query verified.
- Seven ordered flag/version cases exercised against upstream `packages.sh`.
- `git diff --check` passed.
- Full application build and on-device visual validation remain pending: this
  environment has no CMake or SDL development headers.

On a RetroPie device, compare ports with Setup's red entries. Verify an unavailable
entry opens details, a supported entry installs normally, refresh/close stays
responsive during discovery, and an absent/failed query shows the unverified label.
