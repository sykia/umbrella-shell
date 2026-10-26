# Проверки текущей версии

Дата подготовки релиза: 2026-10-26. Этот файл относится к текущей реализации
системной панели и Clock Widgets, а не к удалённым Shell Bar, Dock или Music.
Версия CMake: 1.0.0.

## Подтверждённые результаты

26 октября, подготовка 1.0.0:

- Выполнена свежая полная прямая сборка MSVC 14.42 и Qt 6.8.3 из текущих
  исходников в отдельном каталоге. Собраны пять поставляемых EXE/DLL, GUI и
  все локальные test targets; старые бинарники не переиспользовались.
- Прошли policy, placement, snapshot protocol, location override, reorder,
  session, menu patch, settings lifetime и recovery guard tests, а также
  geometry/observer host fixtures и полный private-process Windows XAML fixture.
- `UmbrellaShell.exe --diagnostics` подтвердил Qt 6.8.3, версию 1.0.0 и
  отключённые обновления. `--ui-smoke` завершился с D3D11, blur и widgets,
  312 кадрами, без QML warnings и graphics errors.
- Исправлена устаревшая snapshot-проверка: одиночное размещение и zone 3
  являются действующими операциями, тогда как zone 4 явно отвергается.
- CMake preset, CTest, Qt deployment, Inno installer, staged public-index audit,
  установка/удаление и GitHub CI будут записаны отдельно только после их
  фактического выполнения. Чистая VM и Authenticode этим локальным прогоном не
  подтверждены.

23 октября, откат синхронизации сторон мониторов:
автоматическое изменение сторон всех активных профилей удалено. Несовместимое
сочетание сторон с вертикальной панелью отклоняется до изменений Windows и
сброса компонентов. В ручной resume desktop test добавлена проверка сохранения
геометрии и профилей при отказе. Независимый вертикальный перенос не подтверждён.
Локальная MSVC/Qt-сборка приложения, deployment, UI smoke (код 0), native
fixture, core policy/placement/reorder и `--exercise` resume desktop test
прошли. Desktop report подтвердил independentEdgesRestored,
mixedSideRefusedWithoutMovingPeers и passed; отдельный снимок после выхода
подтвердил исходные панели 2560×48 снизу на обоих мониторах. Foundation и
аудит публичного индекса также прошли; публикация не выполнялась.

Локальный эксперимент с per-window native setters: геометрия вторичного экрана
менялась отдельно на left/right/bottom, но проверки приложения/трея на толщине
72 DIP не прошли. Расширение с фиксированным размером XAML вызвало зависание
Explorer; затем Explorer самостоятельно перезапустился (завершение процесса
командой не выполнялось). Экспериментальные исходники удалены, изменения native
модуля и visual layout откачены. Этот вариант не поставляется пользователю.
Python QML-проверка недоступна: в runtime нет PySide6. Другие DLL/DPI не проверены.
22 октября, расширение трея до четырёх оригинальных групп:

- Полная прямая сборка MSVC 14.42/Qt 6.8.3 через Build-Local, deployment Qt и
  релевантные placement/reorder/snapshot/session/XAML/host fixtures прошли.
  CMake presets, installer и D3D smoke во время игры не запускались.
- Core проверяет все 24 кодировки/обратное преобразование, недопустимые порядки
  и совместимость прежнего кода обмена часов. Собственный XAML fixture проверяет
  все 24 перестановки исходных объектов разной ширины, сохранение фиксированного
  служебного слота при его появлении/скрытии и reset. Прежние Click/TabIndex,
  stale-model и rollback проверки сохранены.
- Offscreen QML suite прошёл: перенос input/overflow через соседние группы,
  связанный network/volume drag, запрет межзонного drop, отказ native, Escape,
  coalescing, переключение монитора и автопрокрутка; warnings отсутствуют.
- Явный quiet `--tray-only` на настоящем Explorer прошёл для всех 24 порядков
  через TaskbarSettings и действующий appearance-host: проверены настоящие XAML
  координаты четырёх групп, sandboxed persist, отказ разделить network/volume,
  повторный запуск нового владельца, reset и восстановление при shutdown.
  Сохранились bounds/workarea/DPI/regions/styles двух панелей, идентичность
  Explorer, foreground и действительные Run/TaskbarLocation. Тестовые INI и
  HKCU изолированы; физический ввод и flyout не вызывались.
- 24 apply/verify/persist вместе с диагностическими IPC заняли 1187 мс;
  полный внешний quiet-прогон — 12166 мс. Это не измерение чистого native apply
  или задержки drag/drop. За прогон Explorer private bytes изменились на
  -60682240, working set на -12824576, handles на -46, threads на -2;
  CPU delta 296.875 мс. Фоновая активность не изолирована, поэтому эти значения
  не доказывают оптимизацию или отсутствие утечки. Сопоставимого baseline нет.

Первая desktop-попытка корректно отказала при появлении неизвестного видимого
MainStack; состояние было восстановлено. Проверка была излишне строгой:
условные соседи теперь сохраняют исходные collection slots и участвуют в общей
проверке геометрии, а не запрещают остальные переносы. Последующий явный
прогон всех 24 порядков прошёл. Меню/клики/flyout/keyboard navigation настоящего
трея, смешанный DPI и native reorder дополнительной панели не проверялись.
Start/Search/Widgets, модель приложений и переносы трея между зонами не готовы.
Наблюдаемый тип центрального ItemsSource — INotifyCollectionChanged; это не
подтверждённый интерфейс изменения модели. Снимок v4 не даёт разрешения записи.

21 октября, рабочий Clock/Quick Settings reorder: собственный Windows XAML fixture
проверил logical child order, реальные slots, исходный Click/TabIndex/parent,
reset, отказ при новом peer и запрет затирания чужой коллекции. QML offscreen suite
проверил drag часов и связанной пары network/volume, ошибочный межзонный drop,
native refusal и прежние Task View/queue/monitor/Escape сценарии без warnings.

На настоящем Explorer подтверждены оба порядка: Clock x=2448 → 2388 DIP,
Quick Settings x=2388 → 2488 DIP при неизменных размерах. Проверены явный reset,
совместный Task View после Widgets и возврат при EOF/завершении своего host.
Измеренные round trips command → ack: 1,76–2,92 мс, включая IPC и native apply;
изолированное native-время и полная drag/drop latency не измерялись.

Quiet `umbrella_configuration_desktop_test --tray-only` прошёл настоящий backend
при запущенном обычном host оформления: apply → сохранение → новый владелец →
appearance + tray replay → selected reset. INI и Run/Advanced keys изолированы
также в собственном recovery-worker. До исправления выявлены отсутствующий
Advanced key в тестовом sandbox и лишняя отправка текущей стороны при restore;
последняя нарушала проверку foreground/состояния. После исправлений итоговый
прогон завершился за 1080 мс: совпали HWND/rect/workarea/DPI/регионы/extended styles/
alpha двух панелей, PID/start time Explorer, foreground игры и реальные Run/
TaskbarLocation. SendInput/SetFocus/flyout не вызывались. Наблюдаемые дельты Explorer:
private +532 КиБ, working set +52 КиБ, handles/threads 0; фоновые изменения не
изолированы, это не доказательство утечки или её отсутствия.

Полная MSVC/Qt 6.8.3 сборка `build/taskbar-reorder` успешна, после найденного
restore-дефекта затронутые GUI/host/test targets пересобраны точечно. Core reorder,
placement, XAML, IPC/session, snapshot protocol, geometry и изолированные fixtures
прошли. Qt runtime/QML plugins развёрнуты локально, diagnostics этой сборки прошёл.
Первоначально отсутствовавшие Qt6Test/qoffscreen для quiet-fixture добавлены из того
же Qt SDK. Установка, release, вход в Windows, реальные popup/контекстные меню,
keyboard navigation и D3D smoke во время игры не проверялись. Старый установленный
runtime автоматически не заменялся.

21 октября, первая разрешённая read-only проверка реального Explorer: новый
observer прошёл две панели, 17 записей на каждой, 306/224 посещённых узла.
Подтверждены TaskbarCollapsibleLayout, обёртка Search на глубине 2 и StackPanel
у известных tray-групп. Обнаружены recycled task-list объекты вне viewport;
после исправления повторный снимок помечает их invisible. Весь native-host запуск
чтения двух панелей занял 32 мс, повторный по тому же пути — 31 мс. Это не
измерение drag/drop или native apply. До/после совпали HWND/геометрия панелей,
рабочие области, DPI, регионы, PID/start time Explorer и foreground HWND.
Физический ввод, flyout, перемещения и restart Explorer не использовались.

Наблюдаемые дельты Explorer при загрузке финального diagnostic image: private
+144 КиБ, working set +232 КиБ, handles/threads без изменения; при повторе по тому
же пути все четыре дельты нулевые. Это разовые процессные наблюдения, не оценка
изолированной стоимости или долгосрочного расхода. Image удерживается один раз
на путь до выхода Explorer; первый исследовательский image другого пути также
остаётся загруженным. XAML-ссылки, подписки и recovery-workers не удерживаются.
Точечная MSVC x64 `/MT` сборка host/module через `-NativeOnly`, observer fixture
(с запретом attach/configure/restore) и обычный geometry fixture на собственном
невидимом HWND прошли. Полная GUI-сборка/установка в этом этапе не выполнялась.

21 октября, отмена очереди конструктора: regression-тест воспроизвёл применение
после скрытия страницы до исправления. После исправления offscreen Qt UI suite
прошёл уход со страницы, hidden/minimized window, disabled editor и Escape при
ожидающей команде; неактивный редактор отвергает новую команду. Использованы
mock-backend и software renderer, без Explorer, системного ввода и GPU-рендеринга.
Для этого QML-изменения тяжёлая сборка/desktop-проверка во время игры не запускалась.

Продолжение 21 октября, обнаружение владельцев: системный WinUI fixture прошёл
прямой и вложенный model item (Button + индикатор внутри StackPanel/Border),
перестановку целой исходной обёртки с актуальным model index, ближайшую границу
вложенного ItemsRepeater, отказ для detached/null и более восьми родителей.
Protocol tests проверили отказ v1/v2 у host v3, неверные owner IDs/depth/index и
отсутствие пригодной привязки после отказа. Полная MSVC/Qt 6.8.3 сборка
`Build-Local.ps1 -Output build/reorder-owner`, core reorder, placement, session,
Host `--fixture` прошли. Foundation и public index audit
успешны.
Это не desktop-проверка: новые снимки/переносы в Explorer не запускались;
задержка native apply и ресурсы Explorer не измерялись.

21 октября 2026, локальный этап логического Reorder: полная MSVC/Qt 6.8.3 сборка
`Build-Local.ps1 -Output build/reorder-model` успешна. Core model-транзакция прошла
отказы до/после всех шести операций перестановки четырёх объектов, восстановление
удалённого объекта, native-verification refusal, no-op и защиту reentrant-данных.
Системный WinUI 2 CBS ItemsRepeater в собственном offscreen HWND прошёл тот же
native-адаптер: порядок/восстановление, исходные Click через automation Invoke,
добавление/закрытие элемента и отказ от перезаписи reentrant-вставки. Snapshot
protocol, placement, session, Host `--fixture` также прошли.
Core planner: 1000 планов по 64 объекта — 20–24 мс в двух запусках; это не время
модельной транзакции или drag/drop. Explorer не подключался, его ресурсы/задержки
не измерялись, дополнительные компоненты настоящей панели не объявлены готовыми.

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

## Восстановление оформления при входе (2026-10-20)

Найдены уже извлечённые MSVC 14.42.34433, SDK 10.0.26100.0 и Qt 6.8.3.
Прямая Release-компиляция пяти продуктов и C++ fixtures прошла через
Build-Local.ps1. Переустановка инструментов не выполнялась. CMake presets/CTest
и Inno для этого изменения локально не запускались; новой CI-проверки нет.

На двух настоящих мониторах 2560×1440, DPI 100%, проверены:

- Разные профили: отдельная панель 80%/72 DIP/alpha 204 и островки
  90%/64 DIP/alpha 153. После уничтожения модели и новой загрузки совпали
  фактические размеры, позиции, прозрачность и регионы обоих окон.
- Независимая верхняя панель дополнительного экрана при нижней основной,
  включая повторное восстановление; общее автоскрытие сохранено/восстановлено
  при совместимой полной нижней панели 48 DIP. Явный сброс отключает resume.
- Настоящий UmbrellaShell.exe --resume-settings восстановил пользовательский
  INI (островки 100%/64 DIP на обоих экранах) без показа окна. Повторный resume
  завершился без второго владельца. После штатного quit новый процесс
  восстановил те же размеры и alpha. SHA-256 пользовательского INI не изменился.
- Доставка настоящего сообщения TaskbarCreated скрытому окну вызвала повторное
  применение с тем же результатом. Explorer при этой проверке не перезапускался.
- Task View: реальный drag в конструкторе, автоматическое native-применение и
  сохранение, совпадение оригинального UIA-элемента, штатный клик Task View,
  сброс, затем восстановление подтверждённой конфигурации новым владельцем.
  Сброс подтверждённого размещения удаляет resume; предупреждений QML нет.
- Матрица рабочей области максимизированных тестовых окон прошла на двух
  экранах: четыре стороны, 48/64/96 DIP, отдельная панель и островки, живые
  изменения, штатное завершение и смерть только тестового host. Исходные HWND
  geometry и rcWork восстановлены. Snap Layouts и смешанный DPI не проверялись.
- Policy, menu patch, native geometry fixture и lifecycle C++ прошли. Diagnostics
  и deployed QML/D3D11 smoke прошли (Qt 6.8.3, blur=1, d3d11=1). PySide6-тесты
  Taskbar и конструктора прошли, включая coalescing, отказ без сохранения,
  изоляцию мониторов и Escape. Использована уже имеющаяся PySide6 6.8.3.

Короткий замер скрытого владельца: около 74 МБ working set, 715 handles
без роста за 1,5 s; 0 ms CPU за выборку. Это не длительный ресурсный тест
и не измерение GPU. Постоянного таймера восстановления нет.

Foundation и аудит текущего индекса пройдены; индекс ещё не содержит эти
незакоммиченные изменения. Исходники отдельно просмотрены. Установленная версия
не заменялась. Подготовка локального runtime не является выпуском или подписью.

**Настоящий повторный вход в Windows не проверен.** Не проверены фактическая
смена PID Explorer, обновление Windows, отказ записи HKCU Run, hotplug/mixed-DPI,
RDP и холодная загрузка с медленно готовящимися драйверами. Повторный запуск
процесса и TaskbarCreated не заменяют эти проверки. Порядок принятия обновления
и ручной проверки входа: [STARTUP_RECOVERY.md](STARTUP_RECOVERY.md).

Промежуточные проверки выявили зависание ожидания сразу после закрытия Task View:
одних событий готовности оказалось недостаточно. Добавлены ограниченная перепроверка
готовности (не более 16 в течение 30 секунд) и защита от повторного входа в обработчик
во время native-применения. Причина ожидания записывается в startup-status.
В прямой Release-сборке явно отключены Qt debug assertions; пути include относительные,
опциональные C++/WinRT source locations отключены. HRESULT/error checks сохранены.

Уточнена причина ожидания: после Task View Windows могла убрать дочерний bridge HWND,
хотя настоящий XamlRoot панели оставался рабочим. Внешний proxy-признак исключён;
готовность XAML подтверждает сам native-host. Последовательность «native-клик Task View,
закрытие, подтверждённое восстановление конструктора, немедленный скрытый запуск GUI»
затем прошла вместе с повторным запуском процесса, двумя панелями и проверкой INI.
Тест ограниченных попыток намеренно отклоняет первые две readiness-callback и
подтверждает ровно третью; после этого новых попыток нет.

## Рабочая область Taskbar — 2026-10-20

Причина рассинхронизации воспроизведена на двух мониторах 2560×1440, 96 DPI,
Windows 11 Explorer 10.0.26100.9711: HWND панели имел толщину 64 px, но rcWork
обоих мониторов и основной ABM_GETTASKBARPOS сохраняли резерв 48 px.
SetWindowPos и масштабирование подложки не обновляли нативный кэш Explorer.
Изменение поперечного Width/Height настоящего TaskbarFrame вызывает штатное
уведомление о размере: Explorer сам пересчитывает резерв и рабочую область.
SPI_SETWORKAREA, регистрация AppBar и изменение пользовательских окон не нужны.

Нативные DLL/host собраны непосредственно MSVC 14.42 / C++20 со статической CRT,
Windows SDK 10.0.26100; повторный прогон выполнен с /O2 /DNDEBUG.
Это сборка проверяемых native-компонентов, не новая полная CMake/Qt/installer-проверка.

Воспроизводимый opt-in тест (изменяет настоящую панель; закрыть другие сеансы
оформления; автоскрытие должно быть выключено, других AppBar быть не должно):

```powershell
python tests/taskbar_workarea_desktop_test.py --run-desktop <output>/UmbrellaTaskbarHost.exe
python tests/taskbar_workarea_desktop_test.py --run-desktop --owner-death <output>/UmbrellaTaskbarHost.exe
python tests/taskbar_workarea_desktop_test.py --run-desktop --independent <output>/UmbrellaTaskbarHost.exe
```

- Для каждой панели создаётся собственное настоящее максимизированное окно.
  Проверены четыре стороны и возврат вниз, 48/64/96 DIP, 75/100% длины,
  отдельная панель и островки: 20 конфигураций на двух мониторах.
  Тест сверяет rcWork каждого монитора, SPI_GETWORKAREA основного, толщину HWND,
  изменение уже максимизированных окон, восстановление и повторную максимизацию.
  После штатного выхода и завершения только тестового host проверяется исходная
  геометрия HWND и rcWork обоих мониторов. Прогоны прошли.
- Опция --independent использует существующий per-monitor протокол host:
  проверены пары толщин 96/48, 96/64, 48/64 DIP. rcWork и уже максимизированное
  окно неизменяемого монитора сохраняют свою геометрию. Прогон прошёл.
- UI Automation в taskbar_native_alignment.ps1 подтвердила центровку кнопок
  приложений/трея при 48/72/96 DIP на четырёх сторонах и восстановление.
- Отдельный эксперимент подтвердил нативное автоскрытие снизу при 100%/48 DIP
  после перехода с левой панели 96 DIP. Исходный флаг автоскрытия и обе рабочие
  области возвращены. Автоскрытие нестандартной толщины по-прежнему не поддерживается.
- Через computer-use в отдельном стандартном Win32-окне выполнен настоящий Snap
  перетаскиванием к левому краю основного монитора. Видимая область окна стала
  1280×1344 при островках 96 DIP, автоматически изменилась на 1280×1376 при
  64 DIP и на 1280×1392 при возврате к 48 DIP. Затем штатная кнопка максимизации
  развернула окно до 2560×1392. Размеры подтверждены снимками и GetWindowRect
  с учётом невидимых рамок; Umbrella не меняла размеры этого окна.
- Flyout Snap Layouts открывался над кнопкой максимизации при панели 96 DIP.
  Выбор плитки не проверен: computer-use отклонил нажатие по Explorer PopupHost,
  который не был доступен как отдельная цель. Это ограничение проверки,
  а не подтверждённая несовместимость Snap Layouts.

Во время одного раннего повторного прогона Explorer аварийно завершился.
WER указал UmbrellaTaskbarModule.dll_unloaded с timestamp 0x6ad61bc9 — прежнюю
DLL из configuration-prototype/taskview-prototype, отличную от проверяемого
бинарника. После автоматического перезапуска Explorer повторные прогоны прошли;
первый сбой не считается успешной проверкой или доказательством исправления
проблемы времени жизни старой DLL.

Не проверены: выбор всех плиток Snap Layouts, Snap на вторичном мониторе,
смешанный DPI, горячая смена топологии, RDP, сторонние AppBar и другие сборки
Windows. Для этих конфигураций гарантия не заявляется. Проверки Foundation,
diff --check и аудит существующего Git index выполняются отдельно; аудит index
не покрывает ещё не добавленные изменения. Установка и публикация не выполнялись.


## Конструктор с автоматическим применением — 2026-10-20

Свежая прямая сборка Build-Local.ps1 (MSVC 14.42 / Qt 6.8.3), fixture с временным
INI и перенаправленным HKCU: Qt drop автоматически применил оригинальную Task View
на x=224 (исходно 1239), сохранил после подтверждения, UIA/WindowFromPoint подтвердили
тот же элемент и штатный физический клик. Restore вернул исходный RECT. Проверены
два мониторных ключа, отказ неверных конфигураций, новый владелец с подтверждённой
схемой и отключение её replay после Restore. Настоящий HKCU Run не переписывался.

Отдельный тест завершил только собственный native-host: исходные координаты
и native input восстановились. PySide6-тесты проверили debounce, очередь pending,
отказ без сохранения, изоляцию мониторов, Escape и автопрокрутку при высоте 560.
Settings.qml проверен Qt-событиями при 1000×700 и 800×560; в видимом тестовом окне
также проверены читаемость и drag. Windows backend этого визуального окна заменён
тестовым; реальное применение подтверждает отдельный C++ fixture. QML warnings нет.

Это не проверка настоящего входа в Windows, установки, второй native-панели,
других DPI/сборок или совместной смерти всех восстановительных процессов.
Подробнее и команды: [TASKBAR_CONFIGURATION.md](TASKBAR_CONFIGURATION.md).

Финальный GUI после последних QML-правок пересобран отдельно. `--ui-smoke` завершился
с кодом 0: frames=975, blur=1, d3d11=1, без QML/graphics errors. C++ policy,
menu-patch и settings-lifetime проверки прошли; Foundation и diff --check прошли.
Аудит существующего индекса прошёл для 256 файлов; он не покрывает незастейдженные
изменения. Публикация/CI и установка не выполнялись.

### Итоговое локальное обновление 2026-10-20

После описанных выше прогонов текущая объединённая версия всех трёх направлений
полностью пересобрана scripts/Build-Local.ps1 (MSVC 14.42, Qt 6.8.3).
Последние QML-изменения проверены configuration_ui и navigation_ui: успешно.
Обновление установленной копии выполнено с предварительной резервной копией
программы, INI и двух записей Run. Все 318 файлов установки совпали с manifest;
пользовательский INI побайтно сохранён. Run указывает установленный exe с
--resume-settings. Один GUI-владелец, отдельный watcher и native host работают.
Новый startup-status подтвердил успешное применение на двух мониторах; независимое
чтение Win32 показало панели 2560x64 в (0,1376) и (-2560,1376).
Foundation и аудит текущего индекса (267 файлов) прошли. Реальный следующий вход
в Windows ещё не проверен; перезагрузка, выход из Windows и перезапуск Explorer
при установке не выполнялись. Эти результаты уточняют предыдущие записи
«установка не выполнялась». Нового релиза и публикации нет.


## Оформление настроек — 23 октября 2026

Новая палитра и стартовая страница проверены четырьмя QML-тестами:
`taskbar_navigation_ui_test.py`, `settings_theme_ui_test.py`,
`taskbar_live_ui_test.py`, `taskbar_configuration_ui_test.py` (PySide6 6.8.3).
Проверены уведомления смены светлой/тёмной/контрастной палитры и акцента
с fake backend, навигация 800×560, отсутствие записей при навигации,
существующие drag/drop, coalescing и rollback. Предупреждений QML нет.
Локально собранный MSVC/Qt 6.8.3 GUI прошёл `--ui-smoke`: D3D11,
системный blur, первый кадр и загрузка ресурсов; exit 0.
Test-Foundation и аудит существующего индекса Test-PublicRepository прошли.
Реальная смена темы Windows, новый installer, VM и CI этой правки не проверены.


## Удаление виджетов рабочего стола — 24 октября 2026

Удалены редактор, часы, календарь, медиасессия, WASAPI/FFT, ресурсы и отдельные
проверки виджетов, подключения в CMake и Build-Local. Системная кнопка Widgets
по-прежнему распознаётся конструктором Taskbar.

Прямая сборка Build-Local.ps1 завершилась с MSVC и Qt 6.8.3. Прошли Foundation,
проверка публичного Git-индекса, policy, mock DWM ownership/error paths
и QML navigation (загрузка Settings, страницы, навигация, размер 800×560).
Это не проверка установки или рабочего стола; CMake presets/ctest не запускались.

## Настройки Fluent 2 — 24 октября 2026

- MSVC 2022 / Qt 6.8.3: `scripts/Build-Local.ps1`, каталог
  `build/settings-fluent-final-20261024`; после финальной правки QML ресурсы
  пересобраны и приложение перелинковано тем же компилятором.
- `--ui-smoke`: код 0, D3D11, native backdrop включён, снимок окна проверен.
  Короткий запуск: sampled peak working set 144.7 MiB, CPU 1.25 s.
  Это не длительный idle/GPU benchmark.
- `taskbar_navigation_ui_test.py`: старт, переходы, навигация, 800×560,
  отсутствие QML warnings и запросов изменения Taskbar.
- `settings_theme_ui_test.py`: light/dark/accent/high contrast, reduced motion,
  прозрачность при native backdrop и непрозрачные fallback-состояния.


Физическое переключение системной темы/прозрачности, отказ DWM и резервный Acrylic
на другой сборке Windows не воспроизводились: fallback-политика проверена на уровне
QML, а успешный native material — на текущей Windows. CMake presets, installer,
VM, подпись, публикация и CI в рамках изменения интерфейса не выполнялись.


## Удаление отдельной панели приложений — 24 октября 2026

Удалены собственные окна панели, модель приложений/закреплений, preview/DWM
thumbnail, страница и навигация, восстановление режима при входе, сборочные
цели и отдельные fixtures. Системная тема выделена в SystemTheme с событийным
обновлением; Taskbar и Widgets используют прежние общие настройки и контролы.

Четыре PySide6 6.8.3 проверки (navigation, settings theme, live UI,
configuration UI) прошли без QML warnings и запросов изменения Taskbar.
Полная прямая сборка scripts/Build-Local.ps1 завершилась успешно (MSVC, Qt 6.8.3),
каталог build/without-panel-01. Свежий GUI прошёл --ui-smoke: exit 0, frames=265,
blur=1, d3d11=1. Пройдены policy, placement, location override, reorder,
private-process XAML, mock session, menu patch, recovery guard и settings lifetime fixtures.
Foundation, аудит существующего индекса и diff --check прошли. Индекс не
обновлялся; аудит индекса не проверяет незакоммиченные изменения.

Snapshot protocol fixture завершился с «Malformed/legacy placement command
accepted»: его исходник, TaskbarPlacementCommand.h и TaskbarModuleProtocol.h
не отличаются от HEAD. Это существующее отдельное падение; протокол не менялся.
CMake presets/CTest, installer, VM, физическая смена темы и CI не запускались.
Установленная копия и рабочая сессия оформления не обновлялись.

## Widgets desktop layers — 24 October 2026

Local MSVC 2022 / Qt 6.8.3 build and isolated D3D11/QML smoke passed. Existing
smoke additionally verifies unchanged layout across editor toggles and persisted
resize dimensions with rejection of non-finite input. Read-only inspection on
two 2560×1440 / 100% DPI screens confirmed six saved widget HWNDs parented to
Progman below SHELLDLL_DefView and above its wallpaper WorkerW. Explorer was
not restarted. Legacy hosts, mixed DPI, virtual desktops, Show Desktop and
Explorer restart/hot-plug recovery remained manual checks. This section records
the removed implementation and is not a claim about the current build.

Follow-up desktop check: reopening monitor 2 showed the three current saved
Clock/Music visuals above Acrylic; Done restored visible HWNDs below icons
with original coordinates. Widgets.ini hash stayed unchanged. The separate
Music control window showed the active session. Final local rebuild and
D3D11/QML smoke passed again after native style read-back validation was fixed.
Five-second whole-GUI/live-media sample: 218.75 ms CPU, 167.4 MiB working set,
1884 to 1885 handles; this does not measure adapter-only cost or GPU use.

## Widgets reset and editor refresh — 25 October 2026

Clock and Music implementations were removed. The current editor creates no
widget windows, media sessions or network requests. Its smoke coverage checks
the nine-item concept catalog, monitor-specific opening, selection, closing and
reopening. Local MSVC 2022 / Qt 6.8.3 compilation passed. UI smoke passed with
317 frames, D3D11, native editor blur, no QML warnings and no scene-graph error.
The captured 2560×1440 editor was visually inspected. Foundation and public-index
audits passed; installer, VM, mixed-DPI desktop and CI checks were not run.

## Clock rebuild — 25 October 2026

The new Clock implementation compiled with local MSVC 2022 and Qt 6.8.3. The
isolated UI smoke created inline and stacked blur clocks, opened the selected
instance settings and passed with 313 frames, D3D11, native editor blur, no QML
warnings and no scene-graph error. The 2560×1440 editor capture was inspected:
both layouts, color swatches, style controls and interval sliders were visible.
The smoke deliberately does not attach windows to Explorer. Live icon-layer,
mixed-DPI, virtual-desktop, long idle CPU/GPU and resume behavior remain manual
checks; installer, VM and CI were not run.

Follow-up style split: `empty`, `blur-card` and `blur-digits` passed the isolated
smoke with 314 frames, D3D11, no QML warnings and no scene-graph error. The
capture confirmed the full-card and glyph-only masks plus the three-option
settings row. Existing `white`/`blur` layoutV2 values migrate to `empty`/
`blur-card`. Explorer attachment and sustained resource use remain manual checks.
