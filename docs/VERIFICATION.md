# Проверки текущей версии

Дата подготовки: 2026-10-19. Этот файл относится к текущей реализации системной
панели, а не к удалённой Shell Bar. Версия CMake: 0.1.0.

## Подтверждённые результаты

[CI 0a0dfff](https://github.com/sykia/umbrella-shell/actions/runs/39247191348)
успешно выполнил Windows MSVC/Qt 6.8.3 сборку, **8/8 CTest**, deployment,
Inno-компиляцию и smoke с системным PATH. Linux core также прошёл.

Локально на Windows 11 скачанный runtime этой сборки выполнил diagnostics и
QML/D3D11 smoke (`blur=1`, `d3d11=1`); installer SHA-256 совпал с CI checksum,
Authenticode показал NotSigned. Установщик не запускался. QML/PySide6-проверка
без изменения панели, Foundation, actionlint и public index audit прошли.

При подготовке CI выявил и помог исправить отсутствующий COM include при
WIN32_LEAN_AND_MEAN и блокировку явного quit фильтром hide-on-close. Неудачные
прогоны сохранены в Actions; проверки не отключались.

Финальный [CI 4397f49](https://github.com/sykia/umbrella-shell/actions/runs/39248428289),
[CI тега](https://github.com/sykia/umbrella-shell/actions/runs/39248989375) и
[release workflow](https://github.com/sykia/umbrella-shell/actions/runs/39248989460)
завершились успешно. Последний действительно опубликовал unsigned prerelease
[v0.1.0](https://github.com/sykia/umbrella-shell/releases/tag/v0.1.0) от sykia.

Локально проверены все 89 SHA-256 runtime-inventory и наличие 156 файлов
уведомлений; opengl32sw.dll и локальные инструкции в deployment отсутствуют.
Финальный runtime выполнил diagnostics и D3D11 smoke. Отдельная проверка реального
Qt GUI подтвердила сохранение процесса после Close, возврат через MenuHost и
нормальный полный выход через --quit-settings без изменения оформления панели.

Опубликованный установщик скачан с GitHub; SHA-256 совпал с checksum и GitHub digest:
`47efa5fd4653b90e01c9d8ef056ed64f806f2f3a85dd71c87e600315e19c4722`.
Размер — 19 001 151 байт; Authenticode — NotSigned. Это не испытание установки.

## Что проверяют автоматические проверки

- Foundation: необходимые файлы, JSON, синтаксис PowerShell и ограничения Inno.
- PublicRepository: staged index, запрещённые файлы, шаблоны секретов/домашних путей.
- CTest Windows: политика обновлений, patch fixture, lifecycle Qt-окна,
  передача show/quit через MenuHost fixture, геометрия модуля, QML/D3D smoke,
  diagnostics и отказ неизвестного аргумента.
- Core Linux: независимая от Qt/Windows политика, без проверки криптографии.
- Deployed smoke: запуск diagnostics/UI с системным PATH без development Qt.
- QML/PySide6: отсутствие применения при загрузке, debounce и последнее изменение
  при busy, несовместимые сочетания режимов; системная панель не меняется.

Native `--fixture` и menu fixture не требуют изменения настоящей панели.
`tests/taskbar_native_alignment.ps1` меняет настоящую панель и предназначен для
отдельного ручного запуска с восстановлением через завершение host.

## Прежние локальные наблюдения текущего native-прототипа

В документации разработки записаны проверки двух мониторов 2560×1440 / 96 DPI,
четырёх сторон и восстановления после выхода/смерти host. Это ограниченные
наблюдения текущей машины, не переносимые на другие Windows DLL или все сценарии.
Точные границы: [TASKBAR_NATIVE_LAYOUT.md](TASKBAR_NATIVE_LAYOUT.md) и
[TASKBAR_CONTEXT_MENU.md](TASKBAR_CONTEXT_MENU.md).

## Локальный прототип перемещения Task View

Отдельный opt-in прототип Task View проверен локально: физический левый drag
через SendInput сдвинул тот же UIA-элемент с x=1239 в x=250, y=1392, 44×48 px.
UIA hit-test подтвердил новую область и отсутствие кнопки на старом месте.
Левый клик вызвал исходный Click и открыл XamlExplorerHostIslandWindow;
после Esc UIA SetFocus вернул S_OK и HasKeyboardFocus=true. Reset вернул
исходный RECT. Отдельно принудительно завершён только свежий тестовый host
с оставленной сдвинутой кнопкой и edit-вводом: через 1,5 с читающий inspect
подтвердил x=1239, нулевое смещение, IsHitTestVisible=true и прежний Background.
Explorer не перезапускался. Host/DLL собраны точечно
MSVC 14.42; полной CMake/Qt/installer/VM-проверки прототипа нет.
Подробности и воспроизведение: [TASKBAR_BUTTON_EDIT_PROTOTYPE.md](TASKBAR_BUTTON_EDIT_PROTOTYPE.md).

## Не подтверждено

Установка/upgrade/uninstall в чистой Windows 11 VM, подпись релиза, длительная
стабильность и утечки, все DPI/RDP/топологии, все flyout/drag-and-drop, потеря
питания и совместный сбой восстановительных процессов. Освобождение GPU при
скрытии запрашивается через Qt, но экономия памяти/энергии не измерена.
Автообновления не реализованы и не проверены.

## Локальное исправление подключения после установки (2026-10-19)

Установленный 0.1.0 воспроизвёл отказ handshake TaskbarHost. Локальный A/B
MSVC 14.42: /MT с /guard:cf отказывает, /MT без /guard:cf подключается;
остальные проверенные флаги /sdl, /permissive-, WIN32_LEAN_AND_MEAN сохранены.
Исключение CFG ограничено TaskbarModule; точная внутренняя причина не установлена.
Ранняя гипотеза о поиске динамической CRT отдельно не подтверждена.
Исправленная DLL собрана вручную и установлена локально с резервной копией исходной.
Подтверждены ready, applied для top/left и detached 75%/72 DIP, EOF restore/exit=0,
--fixture, diagnostics, QML/D3D11 smoke, Foundation и public index audit.
Старый тестовый MenuHost штатно остановлен; установленный --open-settings вернул 0.
Полная CMake/CTest сборка, новый installer и VM-проверка этой правки не выполнялись.

## Конструктор внутри настроек — локальная проверка

`tests/taskbar_configuration_ui_test.py` проверил Qt mouse drag: три области,
вставку/порядок, каталог, отмену, два независимых черновика, неподдерживаемые
сценарии и автопрокрутку страницы. `taskbar_live_ui_test.py` прошёл без регрессии.
Точечно собраны GUI и native host/DLL MSVC 14.42, Qt 6.8.3. UI smoke: D3D11,
blur, кадры; QML warnings отсутствовали. Полная CMake/deployment сборка не запускалась.

Desktop C++ fixture прошёл всю цепочку GUI drag → отдельное применение →
оригинальная Task View x=224 → UIA point-hit/CompareElements → настоящий клик/
Task View → Reset. Подтверждено раздельное сохранение/перечитывание для двух
реальных QScreen и отклонение неверного черновика; INI теста временный.
Отдельно прошли native position/click/focus/reset и owner-death нового
--configure-taskview: x=1239 восстановлен через 1,5 s после смерти только host.
Explorer не перезапускался. Проверка занятых x=176 вернула ERROR_BUSY и откат,
а не фиктивный успех. Дополнительная панель и остальные компоненты в Windows
не переставлялись. [Границы и команды](TASKBAR_CONFIGURATION.md).

Закрытие этапа: обычный exe без --ui-smoke/fixture открыт через штатную навигацию
Taskbar. «Конфигурация панели» видна в верхней части страницы. Повторная ручная
MSVC/Qt-сборка и локальное развёртывание runtime выполнены; установленная копия
не заменялась, native-применение не запускалось.
