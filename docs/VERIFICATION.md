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

## Не подтверждено

Установка/upgrade/uninstall в чистой Windows 11 VM, подпись релиза, длительная
стабильность и утечки, все DPI/RDP/топологии, все flyout/drag-and-drop, потеря
питания и совместный сбой восстановительных процессов. Освобождение GPU при
скрытии запрашивается через Qt, но экономия памяти/энергии не измерена.
Автообновления не реализованы и не проверены.
