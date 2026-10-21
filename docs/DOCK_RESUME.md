# Umbrella Dock — точка продолжения

Разработка возобновлена 2026-10-21 с `f14f4b315846e3f15f69bb48d57bccd596237271`.
Ветка: `agents/umbrella-dock`, worktree: `umbrella-dock`. Полный перечень
возможностей и ограничений: [TASKBAR_DOCK.md](TASKBAR_DOCK.md).

## Подтверждённое состояние

- Сохранённые перед выключением policy/model/QML fixtures повторно прошли.
- Добавлены включаемые DWM-миниатюры с выбором/закрытием окна и освобождением
  ресурсов. Mock DWM API проверяет lifecycle и ошибки без desktop HWND.
- Исправлены стабильный порядок нескольких окон, повторное создание COM apartment
  и лишние model notifications. Добавлены соответствующие регрессионные проверки.
- Выполнена свежая прямая сборка MSVC 14.42 / Qt 6.8.3 через Build-Local, затем
  адресная пересборка. Output: `build/dock-resume-1`.
- Повторная свежая сборка: `build/dock-resume-2`. Три Dock test executables
  (policy, mock DWM lifecycle/error paths, isolated model/persistence/QML) прошли
  из этого output. Дополнительно прошли update policy и mock menu patch tests.
- Offscreen Taskbar live/navigation проверки повторно прошли. Foundation прошёл;
  staged audit проверяется перед коммитом. Измерения синтетической модели записаны отдельно
  в TASKBAR_DOCK.md; не считать их CPU/RAM/GPU показателями настоящего Dock.

Для свежей сборки (Output должен отсутствовать):

```powershell
./scripts/Build-Local.ps1 -ToolsRoot '../Umbrella Shell/tmp/native-tools' -QtRoot '../Umbrella Shell/tmp/qt-sdk' -Output 'build/dock-resume-3'
```

Для fixtures добавить Qt SDK bin в PATH; QT_QPA_FONTDIR можно направить на
системный Fonts. После windeployqt нужен SDK platforms/qoffscreen.dll либо
QT_QPA_PLATFORM_PLUGIN_PATH. Запускать `umbrella_dock_policy_tests.exe`,
`umbrella_dock_thumbnail_tests.exe`, `umbrella_dock_fixture_tests.exe`.
Отдельный `--benchmark` запускает только синтетическую модель без desktop/RHI.

## Далее

1. Проверить фактические DWM preview и graphics failure paths в контролируемой
   интеграционной среде. Mock fixtures не доказывают отображение native thumbnail.
2. Проверить совместное автоскрытие со штатным Taskbar, два монитора 2560×1440,
   mixed DPI, hotplug/primary switch, packaged apps, Explorer restart и login.
3. Измерить реальные CPU/RAM/GPU в простое/наведении/анимациях и при множестве окон;
   проверить освобождение ресурсов и latency Shell API при завершении.
4. Выполнить CMake presets/CTest и installer/VM проверки там, где доступны инструменты.

Во время параллельной разработки реальные desktop-тесты остаются запрещёнными.
Установленная Umbrella, пользовательские приложения и Explorer не изменялись.
Push, merge, установка и релиз не выполнялись.

Общие точки интеграции: CMakeLists.txt, src/app/main.cpp, Settings.qml,
TaskbarPage.qml, scripts/Build-Local.ps1, tests/taskbar_live_ui_test.py и документация.
TaskbarSettings/TaskbarRuntime API не изменены. `dockSystem.enabled` обозначает
совместимый Dock, а не эксклюзивное управление системной панелью.
