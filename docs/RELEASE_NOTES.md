Umbrella Shell development build: Qt Quick/D3D11 Shell Bar with the supplied
Umbrella logo and settings, monitor/DPI event handling, Windows AppBar workspace
reservation, opt-in per-user startup and a taskbar recovery process.

Windows taskbar windows are additionally hidden with Win32 and observed by an
out-of-context event hook. Explorer window classes are not a public API contract;
asynchronous events cannot guarantee zero transient frames on every Windows build. Explorer remains the Windows shell. Automatic updates, Blur,
transactional installer rollback and a universal zero-flicker guarantee
are not implemented. See docs/SHELL_BAR.md for verification limits and the
Windows 11 VM matrix; compilation does not establish production readiness.
