"""Opt-in Windows desktop test. Changes real Taskbar; uses only its own windows.

Run with Python 3: taskbar_workarea_desktop_test.py --run-desktop HOST_PATH
Close any active Umbrella styling session first. Requires autohide off, no other
appbars, and the supported native layout build for the four-edge matrix.
Does not test the Snap Layouts flyout, mixed DPI, or changing display topology.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import queue
import subprocess
import threading
import time


class MonitorInfo(c.Structure):
    _fields_ = [('size', w.DWORD), ('monitor', w.RECT), ('work', w.RECT),
                ('flags', w.DWORD), ('device', w.WCHAR * 32)]


class Appbar(c.Structure):
    _fields_ = [('size', w.DWORD), ('hwnd', w.HWND), ('callback', w.UINT),
                ('edge', w.UINT), ('rect', w.RECT), ('param', w.LPARAM)]


def rectangle(r):
    return (r.left, r.top, r.right, r.bottom)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-desktop', action='store_true', required=True)
    parser.add_argument('host', type=Path)
    parser.add_argument('--owner-death', action='store_true', help='Terminate only the test host for recovery verification')
    parser.add_argument('--independent', action='store_true', help='Also test unequal thickness using the host per-monitor protocol')
    args = parser.parse_args()
    if not __debug__:
        raise RuntimeError('Run this verification without Python -O; assertions must remain enabled')
    host = args.host.resolve(strict=True)
    u, shell = c.WinDLL('user32', use_last_error=True), c.WinDLL('shell32')
    signatures = {
        'SetThreadDpiAwarenessContext': (w.HANDLE, [w.HANDLE]),
        'FindWindowExW': (w.HWND, [w.HWND, w.HWND, w.LPCWSTR, w.LPCWSTR]),
        'MonitorFromWindow': (w.HANDLE, [w.HWND, w.DWORD]),
        'GetMonitorInfoW': (w.BOOL, [w.HANDLE, c.POINTER(MonitorInfo)]),
        'GetWindowRect': (w.BOOL, [w.HWND, c.POINTER(w.RECT)]),
        'GetDpiForWindow': (w.UINT, [w.HWND]),
        'CreateWindowExW': (w.HWND, [w.DWORD, w.LPCWSTR, w.LPCWSTR, w.DWORD,
                                   c.c_int, c.c_int, c.c_int, c.c_int, w.HWND, w.HMENU, w.HINSTANCE, w.LPVOID]),
        'DestroyWindow': (w.BOOL, [w.HWND]),
        'ShowWindow': (w.BOOL, [w.HWND, c.c_int]),
        'IsZoomed': (w.BOOL, [w.HWND]),
        'DispatchMessageW': (w.LPARAM, [c.POINTER(w.MSG)]),
    }
    for name, (result, params) in signatures.items():
        getattr(u, name).restype, getattr(u, name).argtypes = result, params
    shell.SHAppBarMessage.argtypes = [w.DWORD, c.POINTER(Appbar)]
    shell.SHAppBarMessage.restype = c.c_size_t
    if not u.SetThreadDpiAwarenessContext(w.HANDLE(-4)):
        raise c.WinError(c.get_last_error())

    def pump():
        msg = w.MSG()
        while u.PeekMessageW(c.byref(msg), None, 0, 0, 1):
            u.TranslateMessage(c.byref(msg))
            u.DispatchMessageW(c.byref(msg))

    def eventually(check, seconds=6):
        deadline = time.monotonic() + seconds
        while True:
            pump()
            try:
                check()
                return
            except AssertionError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(.03)

    def window_rect(hwnd):
        r = w.RECT()
        if not u.GetWindowRect(hwnd, c.byref(r)):
            raise c.WinError(c.get_last_error())
        return rectangle(r)

    def monitor_info(hwnd):
        m = MonitorInfo(c.sizeof(MonitorInfo))
        if not u.GetMonitorInfoW(u.MonitorFromWindow(hwnd, 2), c.byref(m)):
            raise c.WinError(c.get_last_error())
        return m

    data = Appbar(c.sizeof(Appbar))
    if shell.SHAppBarMessage(4, c.byref(data)) & 1:
        raise RuntimeError('Turn autohide off before running the geometry matrix')
    panels = []
    for cls in ('Shell_TrayWnd', 'Shell_SecondaryTrayWnd'):
        hwnd = None
        while True:
            hwnd = u.FindWindowExW(None, hwnd, cls, None)
            if not hwnd:
                break
            panels.append(hwnd)
            if len(panels) > 16:
                raise RuntimeError('Too many taskbars')
    if not panels:
        raise RuntimeError('No taskbar')
    before = {h: (window_rect(h), rectangle(monitor_info(h).work)) for h in panels}
    windows, margins = {}, {}
    process = None
    try:
        for h in panels:
            m = monitor_info(h)
            test = u.CreateWindowExW(0, 'STATIC', 'Umbrella work area verification', 0x00CF0000,
                                    m.monitor.left + 150, m.monitor.top + 150, 600, 400,
                                    None, None, None, None)
            if not test:
                raise c.WinError(c.get_last_error())
            windows[h] = test
            u.ShowWindow(test, 3)  # Real maximize; subsequent changes must adapt it automatically.
            pump()
            margins[h] = tuple(a - b for a, b in zip(window_rect(test), before[h][1]))
            assert u.IsZoomed(test), 'Test window did not maximize'
        process = subprocess.Popen([str(host), '--serve'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, creationflags=0x08000000)
        lines = queue.Queue()

        def read_lines():
            for line in process.stdout:
                lines.put(line.strip())
            lines.put(None)

        threading.Thread(target=read_lines, daemon=True).start()

        def expect(text):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                pump()
                try:
                    line = lines.get(timeout=.03)
                except queue.Empty:
                    continue
                if line == text:
                    return
                if line is None or line.startswith(('error ', 'FAILED')):
                    raise RuntimeError(f'Host rejected request: {line}')
            raise RuntimeError(f'Host response timeout: {text}')

        expect('ready')
        configured_thickness = {}
        for edge in (3, 0, 1, 2, 3):
            for width, thickness, islands in ((100, 48, 0), (75, 96, 0), (75, 64, 1), (100, 96, 1)):
                command = f'{width} {edge} {thickness} {islands}'
                process.stdin.write(command + '\n')
                process.stdin.flush()
                expect('applied ' + command)

                def verify():
                    primary_work = None
                    for h, test in windows.items():
                        m = monitor_info(h)
                        expected = list(rectangle(m.monitor))
                        pixels = (configured_thickness.get(h, thickness) * u.GetDpiForWindow(h) + 48) // 96
                        expected[edge] += pixels if edge < 2 else -pixels
                        assert rectangle(m.work) == tuple(expected), (command, rectangle(m.work), expected)
                        panel = window_rect(h)
                        assert panel[2 if edge % 2 == 0 else 3] - panel[0 if edge % 2 == 0 else 1] == pixels, (command, panel)
                        assert u.IsZoomed(test), 'Window lost maximized state'
                        expected_window = tuple(a + b for a, b in zip(expected, margins[h]))
                        assert window_rect(test) == expected_window, (command, window_rect(test), expected_window)
                        if m.flags & 1:
                            primary_work = tuple(expected)
                    spi = w.RECT()
                    if not u.SystemParametersInfoW(48, 0, c.byref(spi), 0):
                        raise c.WinError(c.get_last_error())
                    assert rectangle(spi) == primary_work, 'SPI_GETWORKAREA differs from primary rcWork'
                    # ABM_GETTASKBARPOS describes the primary reserved taskbar,
                    # not the shortened HWND/clip and not secondary monitors.
                    assert shell.SHAppBarMessage(5, c.byref(data)), 'ABM_GETTASKBARPOS failed'
                    assert data.edge == edge, (data.edge, edge)

                eventually(verify)
                for test in windows.values():
                    u.ShowWindow(test, 9)  # Restore, then maximize again against the new work area.
                    pump()
                    assert not u.IsZoomed(test), 'Restore failed'
                    u.ShowWindow(test, 3)
                eventually(verify)
                print(f'PASS {command}: {len(windows)} monitors, live maximize + restore/maximize', flush=True)
        if args.independent:
            if len(panels) < 2:
                raise RuntimeError('Independent thickness verification requires two monitors')
            command = '100 3 48 0'
            process.stdin.write(command + '\n')
            process.stdin.flush()
            expect('applied ' + command)
            configured_thickness = {h: 48 for h in panels}
            eventually(verify)
            for target, requested in ((panels[0], 96), (panels[1], 64), (panels[0], 48)):
                command = f'100 3 {requested} 0 1 {monitor_info(target).device}'
                process.stdin.write(command + '\n')
                process.stdin.flush()
                expect('applied ' + command)
                configured_thickness[target] = requested
                eventually(verify)
                print(f'PASS independent thickness {list(configured_thickness.values())}: work areas and maximized windows', flush=True)
    finally:
        if process:
            if args.owner_death and process.poll() is None:
                process.terminate()  # Only the child created by this test.
            else:
                process.stdin.close()
            process.wait(timeout=12)
        try:
            def restored():
                for h, baseline in before.items():
                    assert (window_rect(h), rectangle(monitor_info(h).work)) == baseline, 'Original panel/work area not restored'
            eventually(restored)
            print('PASS original HWND geometry and per-monitor rcWork restored', flush=True)
        finally:
            for test in windows.values():
                u.DestroyWindow(test)
        if process and not args.owner_death and process.returncode:
            raise RuntimeError(f'Host exit: {process.returncode}')


if __name__ == '__main__':
    main()
