# Umbrella Dock — сохранённая точка продолжения

Разработка приостановлена по просьбе владельца перед выключением ПК.
Ветка: `agents/umbrella-dock`, worktree: `umbrella-dock`.
Первый проверенный этап: `9296f5df03b2f56e1fef0280488e08c71bac1c45`.
Последующий WIP содержит расширения; не считать его полностью проверенным.

## Реализовано

Отдельные Qt Quick Dock-окна в существующем процессе, четвёртый режим и страница
настроек, логотип Umbrella, событийные WinEvent hooks, worker снимков/значков,
группы окон, активация/сворачивание/закрытие, закрепления и их порядок, частичный
импорт Windows, packaged AUMID/AppsFolder, настройки темы/геометрии/автоскрытия,
профили устойчивых ID мониторов, плавное скрытие, общий компонент живого превью.
Совместимый режим сохраняет системную панель и native/Reorder-конфигурацию.

## Что проверено и что ещё не проверено

Прямая MSVC 14.42 / Qt 6.8.3 сборка проходила. Dock policy/model/persistence/QML
fixtures, существующие offscreen Taskbar live/navigation тесты и визуальные
software-render снимки проходили до последнего изменения сопоставления AUMID/EXE.
Последняя адресная сборка завершилась с кодом 0 (`build-dock-verified.log`), но
после неё тесты ещё НЕ повторены: владелец запросил остановку.

Последние изменения требуют первой перепроверки: симметричное сопоставление
закреплённого EXE и окна с AUMID, очистка transient-модели при отключении Dock,
сообщения об отказе оконных действий. Дополнительно проверить обработку ошибки
создания графики и отчёт startup-status; эти ветви не испытаны на реальном GPU.

Development output: `build/dock-check-6`. windeployqt скопировал Qt; translations
выдали предупреждение. Добавлен SDK qoffscreen.dll для изолированного fixture.
Deployed diagnostics и offscreen fixture с системным PATH проходили до последних
изменений. Offscreen backend сообщал о каталоге fonts; реальный Windows font/DWM
backend этим не проверяется. Установленная Umbrella не заменялась.

Не выполнены CMake presets/CTest, реальный GUI/D3D11 smoke нового Dock,
desktop/mixed-DPI/hotplug/Explorer restart/login/installer проверки и измерения
CPU/RAM/GPU. DWM-миниатюры окон пока не реализованы. Точный системный pin-order и
полная синхронизация Windows pins публичным API не предоставляются.

## Следующий шаг

1. Прочитать этот файл и `docs/TASKBAR_DOCK.md`; проверить branch/status.
2. Повторить Dock fixtures после последней сборки. После deployment тестовой
   платформе нужен `platforms/qoffscreen.dll` либо QT_QPA_PLATFORM_PLUGIN_PATH из
   того же Qt SDK. Не запускать основной GUI рядом с установленным владельцем.
3. Для воспроизводимой свежей сборки использовать отслеживаемый скрипт:

   ```powershell
   ./scripts/Build-Local.ps1 -ToolsRoot '../Umbrella Shell/tmp/native-tools' -QtRoot '../Umbrella Shell/tmp/qt-sdk' -Output 'build/dock-resume-1'
   ```

   Output должен отсутствовать. Локальные адресные helper-скрипты находятся в
   игнорируемом `tmp/`, но не требуются для воспроизведения сборки.
4. Проверить оставшиеся требования, особенно миниатюры, надёжность автоскрытия и
   совместное поведение со штатным автоскрытием Taskbar. Реальные desktop/resource
   испытания выполнять только в согласованной интеграционной сессии: параллельные
   агенты не должны одновременно менять рабочий стол.
5. После проверок обновить ограничения и создать следующий обычный локальный
   коммит. Не делать push, merge, установку или релиз без нового указания владельца.

Изменяемые общие файлы: CMakeLists.txt, src/app/main.cpp, Settings.qml,
TaskbarPage.qml, scripts/Build-Local.ps1, tests/taskbar_live_ui_test.py и общая
документация. TaskbarSettings/TaskbarRuntime API не изменены. Возможны конфликты
интеграции в composition root, QML-навигации и сборке. Новых агентов не создавать.
