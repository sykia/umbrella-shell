# Каталог системных контролов Taskbar Windows 11

Снимок: **19 октября 2026**, текущий интерактивный сеанс. Только чтение Win32 и UI Automation (UIA). Настройки, реестр, Explorer и расположение элементов не изменялись. Меню, hover-панели и flyout не открывались. Это каталог доступного снимка с отдельным перечнем условных компонентов, а не полный перечень внутренних объектов всех версий Windows.

## Среда, метод и достоверность

- ОС сообщает DisplayVersion `26H2`, build `26300.9711`; legacy ProductName — `Windows 10 Pro` (сам по себе не определяет современный бренд). Версия файла Explorer: `10.0.26100.8875`. Причина различия версий не исследовалась.
- Две панели принадлежат одному процессу Explorer. Обе видимы, снизу, 96 DPI (100%). Основной DISPLAY1: экран `(0,0,2560,1440)`, рабочая область `(0,0,2560,1392)`; дополнительный DISPLAY2 слева: экран `(-2560,0,2560,1440)`, рабочая область `(-2560,0,2560,1392)`.
- Win32: `EnumWindows`, `EnumChildWindows`, `GetParent`, `GetClassName`, `GetWindowRect`, `IsWindowVisible`, `GetDpiForWindow`, `GetWindowThreadProcessId`, `EnumDisplayMonitors`/`GetMonitorInfo`. DPI awareness задавался только диагностическому процессу. UIA: `AutomationElement.FromHandle` и `TreeWalker.RawViewWalker`, только чтение Current properties. Лимиты 1800 узлов/40 уровней не достигнуты; ошибок обхода нет.
- Получено 115 UIA-узлов выбранных корней: 110 относятся к двум панелям и скрытому `SystemTray_Main`; 5 посторонних служебных корней исключены. Включены доступные offscreen, пустые и дублированные узлы.
- Изучены README/архитектура, `TaskbarIslands.h`, `TaskbarVisualLayout.h`, `TaskbarPopupPlacement.h`. Обход проекта собирает геометрию групп, ограничен 512 узлами и не сохраняет имена/AppID; полного CLI-каталога контролов нет. Приложение, host и DLL не запускались, сборки не выполнялись. Использован временный внешний читающий диагностический скрипт, не часть продукта.
- Уже работающий `UmbrellaMenuHost` и его скрытое окно обнаружены, но не запускались и не останавливались. Это наблюдение стандартных классов Explorer в существующем окружении, а не доказательство чистой системы без расширений.
- Далее RECT — **экранные физические px `(x,y,w,h)`**. Отрицательные x допустимы. Порядок строк UIA — preorder провайдера, не z-order; визуальный порядок определяется x. HWND действительны только в этом сеансе. UIA ClassName/AutomationPeer не обязательно равен XAML-типу; UIA не раскрывает полный visual tree.
- `IsOffscreen=false` не доказывает фактическую отрисовку: скрытый Win32 `SystemTray_Main` показывает расхождение. Бесконечный UIA RECT означает отсутствие геометрии. Наличие `ProgressIndicator` не доказывает активную загрузку; декоративные дубли не являются дополнительными кнопками.
- Имена сетей/устройств, точная погода, время и содержимое уведомлений не перенесены. Имена приложений оставлены для различения кнопок; пути профиля отсутствуют. Сырой снимок находится только в игнорируемом `tmp/` и не входит в публикуемый документ.

## Иерархия доступности и соответствие XAML

```text
P Shell_TrayWnd [0]
├─ DesktopWindowContentBridge [1]
│  └─ InputSite.WindowClass [2]
│     ├─ TaskbarFrame / Taskbar.TaskbarFrameAutomationPeer [3]
│     │  └─ TaskbarFrameRepeater [4]
│     │     ├─ WidgetsButton → 2 одинаковых NamedContainer → image + 2 text
│     │     ├─ StartButton → Icon
│     │     ├─ SearchButton → Icon + SearchBoxTextBlock
│     │     ├─ TaskViewButton → Icon
│     │     └─ 8 TaskListButtonAutomationPeer → Icon / ProgressIndicator / OverlayIcon
│     └─ 6 SystemTray.* кнопок [40,44,46,50,54,57] → text/glyph потомки
├─ TrayNotifyWnd [61]
└─ ReBarWindow32 [62] → MSTaskSwWClass [63]

SystemTray_Main [64] (отдельный скрытый HWND)

S Shell_SecondaryTrayWnd [65]
├─ DesktopWindowContentBridge [66]
│  └─ InputSite.WindowClass [67]
│     ├─ TaskbarFrame [68] → TaskbarFrameRepeater [69] → те же 12 кнопок
│     └─ SystemTray.OmniButton, часы [105] → время + дата
└─ WorkerW [108] → MSTaskListWClass [109]
```

P — основная, S — дополнительная. Числа в скобках — номера узлов полного каталога ниже. UIA parent — наблюдаемый RawView parent, а не предполагаемый visual parent. Системные кнопки здесь находятся под InputSite, **не** под `TrayNotifyWnd`. `SystemTray.SystemTrayFrame`, `Taskbar.TaskbarBackground`/`BackgroundControl` известны из исходников проекта, но в UIA-снимке не раскрыты; их RECT и полный набор потомков неизвестны. Внедрение для получения XAML-дерева не выполнялось.

## Функциональные кнопки и визуальный порядок

Центральная последовательность обеих панелей: Пуск → Поиск → Представление задач → Проводник → Firefox → Steam (закреплённый путь) → SoundCloud Desktop → Discord → Steam (запущенный AppID) → Codex (UIA name ChatGPT) → Telegram. Это 11 кнопок плюс Widgets у левого края: **12 на каждой панели**. Две Steam-кнопки имеют разные AutomationId и реально присутствуют отдельно.

| Компонент / назначение | AutomationId / класс | P: x,y,w,h | S: x,y,w,h | Условие/состояние |
|---|---|---|---|---|
| Widgets / открытие виджетов | WidgetsButton / ToggleButton | 6,1392,152,48 | -2554,1392,152,48 | Видим погодный ticker; зависит от настройки/доступности Widgets |
| Пуск / запуск приложений | StartButton / ToggleButton | 948,1392,45,48 | -1612,1392,45,48 | Видим на обеих |
| Поиск | SearchButton / ToggleButton | 995,1400,220,32 | -1565,1400,220,32 | Режим поля; другие режимы не проверялись |
| Task View / окна и рабочие столы | TaskViewButton / ToggleButton | 1217,1392,44,48 | -1343,1392,44,48 | Видим; необязательная кнопка |
| 8 кнопок приложений / запуск и переключение | Taskbar.TaskListButtonAutomationPeer; Appid:… | x=1261…1569, шаг 44; y=1392; 44×48 | x=-1299…-991, шаг 44; y=1392; 44×48 | Закреплённые и запущенные; точные ID ниже |
| Скрытые значки / открытие overflow | SystemTrayIcon / SystemTray.NormalButton | 2340,1392,32,48 | Не обнаружен | Первая кнопка трея; содержимое закрытого overflow неизвестно |
| Индикатор ввода / язык и раскладка | SystemTrayIcon / SystemTray.NormalButton | 2372,1392,44,48 | Не обнаружен | Видимый текст РУС; установленные языки не исследовались |
| Сеть / состояние подключения | SystemTrayIcon / SystemTray.AccentButton | 2420,1392,24,48 | Не обнаружен | Состояние подключения доступно через UIA |
| Громкость / звук | SystemTrayIcon / SystemTray.OmniButtonRight | 2448,1392,24,48 | Не обнаружен | Отдельный peer; общий flyout не открывался |
| Часы / дата, доступ к календарю/уведомлениям | SystemTrayIcon / SystemTray.OmniButton | 2480,1392,64,48 | -72,1392,64,48 | На обеих; действие не вызывалось |
| Показать рабочий стол / свернуть окна | SystemTrayIcon / SystemTray.ShowDesktopButton | 2548,1392,12,48 | Не обнаружен | Последний элемент основной панели |

Родитель первых 12 кнопок P — узел 4, системных — 2; для S — 69 и 67 соответственно. `SystemTrayIcon` не уникален: различать по классу, роли, родителю и RECT. Свободные 8 px после часов S не объявляются кнопкой Show Desktop.

## Контейнеры Win32

Размер служебного HWND не подменяет геометрию XAML/UIA-кнопки. Visible — результат Win32, а не UIA. Корни имеют parent=0.

| Панель | HWND decimal | Класс | Parent HWND | x,y,w,h | Visible |
|---|---:|---|---:|---|---|
| P | 65740 | Shell_TrayWnd | 0 | 0,1392,2560,48 | True |
| P | 131548 | Windows.UI.Composition.DesktopWindowContentBridge | 65740 | 0,1392,2560,48 | True |
| P | 66052 | Windows.UI.Input.InputSite.WindowClass | 131548 | 0,1392,0,0 | True |
| P | 66002 | Windows.UI.Core.CoreWindow | 65740 | 0,1392,65535,65535 | False |
| P | 65744 | Start | 65740 | 947,1392,45,48 | False |
| P | 65746 | TrayDummySearchControl | 65740 | 0,1392,0,0 | True |
| P | 65748 | TrayNotifyWnd | 65740 | 2340,1392,220,48 | True |
| P | 65756 | ReBarWindow32 | 65740 | 947,1392,489,48 | True |
| P | 65758 | MSTaskSwWClass | 65756 | 947,1392,489,48 | True |
| P | 65760 | MSTaskListWClass | 65758 | 947,1392,489,48 | False |
| — | 65998 | SystemTray_Main | 0 | 0,0,14,14 | False |
| S | 65764 | Shell_SecondaryTrayWnd | 0 | -2560,1392,2560,48 | True |
| S | 66064 | Windows.UI.Composition.DesktopWindowContentBridge | 65764 | -2560,1392,2560,48 | True |
| S | 66066 | Windows.UI.Input.InputSite.WindowClass | 66064 | -2560,1392,0,0 | True |
| S | 65766 | Start | 65764 | -1613,1392,45,48 | False |
| S | 65768 | WorkerW | 65764 | -1613,1392,489,48 | True |
| S | 65770 | MSTaskListWClass | 65768 | -1613,1392,489,48 | True |

Скрытые HWND `Start` — совместимость, а не видимые кнопки Пуск. `TrayDummySearchControl` пустой и не равен SearchButton. `CoreWindow` имеет служебный огромный RECT, который не является видимым экраном. `ReBarWindow32`/`MSTask*` и `WorkerW` — legacy-контейнеры: их прямоугольники не покрывают все современные кнопки. `InputSite` имеет пустой Win32 RECT, но полный RECT панели в UIA. `SystemTray_Main` — скрытое окно с UIA name «Индикатор батарей», не подтверждение видимой батареи.

## Полный UIA-каталог обнаруженных узлов

Включает контейнеры и декоративные потомки. `—` — пустое свойство, `нет RECT` — нечисловая/бесконечная геометрия, `off` — IsOffscreen. Все 110 узлов сообщали IsEnabled=true. Framework — свойство FrameworkId провайдера. Названия погодных/сетевых/часовых данных заменены ролями; AppID сохранены без локальных путей профиля.

| № | Parent № | Тип | ClassName | AutomationId | Имя/роль | x,y,w,h | off | Framework |
|---:|---:|---|---|---|---|---|---|---|
| 0 | -1 | Pane | Shell_TrayWnd | — | — | 0,1392,2560,48 | False | Win32 |
| 1 | 0 | Pane | Windows.UI.Composition.DesktopWindowContentBridge | — | DesktopWindowXamlSource | 0,1392,2560,48 | False | Win32 |
| 2 | 1 | Pane | Windows.UI.Input.InputSite.WindowClass | — | — | 0,1392,2560,48 | False | XAML |
| 3 | 2 | Pane | Taskbar.TaskbarFrameAutomationPeer | TaskbarFrame | — | 0,1392,2560,48 | False | XAML |
| 4 | 3 | Group | — | TaskbarFrameRepeater | — | 6,1392,1607,48 | False | XAML |
| 5 | 4 | Button | ToggleButton | WidgetsButton | Widgets / погодный ticker | 6,1392,152,48 | False | XAML |
| 6 | 5 | Group | NamedContainerAutomationPeer | — | Widgets / погодный ticker | 16,1396,136,40 | False | XAML |
| 7 | 6 | Image | Image | BadgeAnchorLargeTicker | — | 19,1403,24,24 | False | XAML |
| 8 | 6 | Text | TextBlock | — | Температура | 51,1399,23,16 | False | XAML |
| 9 | 6 | Text | TextBlock | — | Описание погоды | 51,1415,73,16 | False | XAML |
| 10 | 5 | Group | NamedContainerAutomationPeer | — | Widgets / погодный ticker | 16,1396,136,40 | False | XAML |
| 11 | 10 | Image | Image | BadgeAnchorLargeTicker | — | 19,1403,24,24 | False | XAML |
| 12 | 10 | Text | TextBlock | — | Температура | 51,1399,23,16 | False | XAML |
| 13 | 10 | Text | TextBlock | — | Описание погоды | 51,1415,73,16 | False | XAML |
| 14 | 4 | Button | ToggleButton | StartButton | Пуск | 948,1392,45,48 | False | XAML |
| 15 | 14 | Image | Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer | Icon | — | 959,1404,24,24 | False | XAML |
| 16 | 4 | Button | ToggleButton | SearchButton | Поиск | 995,1400,220,32 | False | XAML |
| 17 | 16 | Image | Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer | Icon | — | 1006,1408,16,16 | False | XAML |
| 18 | 16 | Text | TextBlock | SearchBoxTextBlock | Поиск | 1032,1406,40,19 | False | XAML |
| 19 | 4 | Button | ToggleButton | TaskViewButton | Представление задач | 1217,1392,44,48 | False | XAML |
| 20 | 19 | Image | Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer | Icon | — | 1227,1404,24,24 | False | XAML |
| 21 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: Microsoft.Windows.Explorer | Приложение Проводник закреплено | 1261,1392,44,48 | False | XAML |
| 22 | 21 | Image | Image | Icon | — | 1271,1404,24,24 | False | XAML |
| 23 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: 308046B0AF4A39CB | Приложение Firefox —1 запущенное окно закреплено | 1305,1392,44,48 | False | XAML |
| 24 | 23 | ProgressBar | Microsoft.UI.Xaml.Controls.ProgressBar | ProgressIndicator | — | 1315,1432,24,3 | False | XAML |
| 25 | 23 | Image | Image | Icon | — | 1315,1404,24,24 | False | XAML |
| 26 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: {6D809377-6AF0-444B-8957-A3773F02200E}\Steam\Steam.exe | Приложение Steam закреплено | 1349,1392,44,48 | False | XAML |
| 27 | 26 | Image | Image | Icon | — | 1359,1404,24,24 | False | XAML |
| 28 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: io.github.sykia.soundcloud-desktop | Приложение SoundCloud Desktop закреплено | 1393,1392,44,48 | False | XAML |
| 29 | 28 | Image | Image | Icon | — | 1403,1404,24,24 | False | XAML |
| 30 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: com.squirrel.Discord.Discord | Приложение Discord —1 запущенное окно закреплено | 1437,1392,44,48 | False | XAML |
| 31 | 30 | Image | Image | Icon | — | 1447,1404,24,24 | False | XAML |
| 32 | 30 | Image | Image | OverlayIcon | — | 1461,1397,16,16 | False | XAML |
| 33 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: Valve.Steam.Client | Steam —1 запущенное окно | 1481,1392,44,48 | False | XAML |
| 34 | 33 | Image | Image | Icon | — | 1491,1404,24,24 | False | XAML |
| 35 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: OpenAI.Codex_2p2nqsd0c76g0!App | ChatGPT —1 запущенное окно | 1525,1392,44,48 | False | XAML |
| 36 | 35 | Image | Image | Icon | — | 1535,1404,24,24 | False | XAML |
| 37 | 4 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: Telegram.TelegramDesktop | Telegram —1 запущенное окно | 1569,1392,44,48 | False | XAML |
| 38 | 37 | Image | Image | Icon | — | 1579,1404,24,24 | False | XAML |
| 39 | 37 | Image | Image | OverlayIcon | — | 1593,1397,16,16 | False | XAML |
| 40 | 2 | Button | SystemTray.NormalButton | SystemTrayIcon | Показать скрытые значки | 2340,1392,32,48 | False | XAML |
| 41 | 40 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 42 | 40 | Text | TextBlock | InnerTextBlock | Системный glyph | 2348,1408,16,16 | False | XAML |
| 43 | 40 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 44 | 2 | Button | SystemTray.NormalButton | SystemTrayIcon | Индикатор ввода / Русский | 2372,1392,44,48 | False | XAML |
| 45 | 44 | Text | TextBlock | InnerTextBlock | РУС | 2380,1407,29,16 | False | XAML |
| 46 | 2 | Button | SystemTray.AccentButton | SystemTrayIcon | Сеть / подключение | 2420,1392,24,48 | False | XAML |
| 47 | 46 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 48 | 46 | Text | TextBlock | InnerTextBlock | Системный glyph | 2424,1408,16,16 | False | XAML |
| 49 | 46 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 50 | 2 | Button | SystemTray.OmniButtonRight | SystemTrayIcon | Громкость / устройство вывода | 2448,1392,24,48 | False | XAML |
| 51 | 50 | Text | TextBlock | InnerTextBlock | Системный glyph | 2452,1408,16,16 | False | XAML |
| 52 | 50 | Text | TextBlock | InnerTextBlock | Системный glyph | 2452,1408,16,16 | False | XAML |
| 53 | 50 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 54 | 2 | Button | SystemTray.OmniButton | SystemTrayIcon | Часы | 2480,1392,64,48 | False | XAML |
| 55 | 54 | Text | TextBlock | TimeInnerTextBlock | Время | 2484,1399,57,16 | False | XAML |
| 56 | 54 | Text | TextBlock | DateInnerTextBlock | Дата | 2484,1415,57,16 | False | XAML |
| 57 | 2 | Button | SystemTray.ShowDesktopButton | SystemTrayIcon | Показать рабочий стол Свернуть все окна | 2548,1392,12,48 | False | XAML |
| 58 | 57 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 59 | 57 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 60 | 57 | Text | TextBlock | InnerTextBlock | — | нет RECT | True | XAML |
| 61 | 0 | Pane | TrayNotifyWnd | — | — | 2340,1392,220,48 | False | Win32 |
| 62 | 0 | Pane | ReBarWindow32 | 40965 | — | 947,1392,489,48 | False | Win32 |
| 63 | 62 | Pane | MSTaskSwWClass | — | Работающие приложения | 947,1392,489,48 | False | Win32 |
| 64 | -1 | Pane | SystemTray_Main | — | Индикатор батарей | 0,0,14,14 | False | Win32 |
| 65 | -1 | Pane | Shell_SecondaryTrayWnd | — | — | -2560,1392,2560,48 | False | Win32 |
| 66 | 65 | Pane | Windows.UI.Composition.DesktopWindowContentBridge | — | DesktopWindowXamlSource | -2560,1392,2560,48 | False | Win32 |
| 67 | 66 | Pane | Windows.UI.Input.InputSite.WindowClass | — | — | -2560,1392,2560,48 | False | XAML |
| 68 | 67 | Pane | Taskbar.TaskbarFrameAutomationPeer | TaskbarFrame | — | -2560,1392,2560,48 | False | XAML |
| 69 | 68 | Group | — | TaskbarFrameRepeater | — | -2554,1392,1607,48 | False | XAML |
| 70 | 69 | Button | ToggleButton | WidgetsButton | Widgets / погодный ticker | -2554,1392,152,48 | False | XAML |
| 71 | 70 | Group | NamedContainerAutomationPeer | — | Widgets / погодный ticker | -2544,1396,136,40 | False | XAML |
| 72 | 71 | Image | Image | BadgeAnchorLargeTicker | — | -2541,1403,24,24 | False | XAML |
| 73 | 71 | Text | TextBlock | — | Температура | -2509,1399,23,16 | False | XAML |
| 74 | 71 | Text | TextBlock | — | Описание погоды | -2509,1415,73,16 | False | XAML |
| 75 | 70 | Group | NamedContainerAutomationPeer | — | Widgets / погодный ticker | -2544,1396,136,40 | False | XAML |
| 76 | 75 | Image | Image | BadgeAnchorLargeTicker | — | -2541,1403,24,24 | False | XAML |
| 77 | 75 | Text | TextBlock | — | Температура | -2509,1399,23,16 | False | XAML |
| 78 | 75 | Text | TextBlock | — | Описание погоды | -2509,1415,73,16 | False | XAML |
| 79 | 69 | Button | ToggleButton | StartButton | Пуск | -1612,1392,45,48 | False | XAML |
| 80 | 79 | Image | Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer | Icon | — | -1601,1404,24,24 | False | XAML |
| 81 | 69 | Button | ToggleButton | SearchButton | Поиск | -1565,1400,220,32 | False | XAML |
| 82 | 81 | Image | Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer | Icon | — | -1554,1408,16,16 | False | XAML |
| 83 | 81 | Text | TextBlock | SearchBoxTextBlock | Поиск | -1528,1406,40,19 | False | XAML |
| 84 | 69 | Button | ToggleButton | TaskViewButton | Представление задач | -1343,1392,44,48 | False | XAML |
| 85 | 84 | Image | Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer | Icon | — | -1333,1404,24,24 | False | XAML |
| 86 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: Microsoft.Windows.Explorer | Приложение Проводник закреплено | -1299,1392,44,48 | False | XAML |
| 87 | 86 | Image | Image | Icon | — | -1289,1404,24,24 | False | XAML |
| 88 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: 308046B0AF4A39CB | Приложение Firefox —1 запущенное окно закреплено | -1255,1392,44,48 | False | XAML |
| 89 | 88 | ProgressBar | Microsoft.UI.Xaml.Controls.ProgressBar | ProgressIndicator | — | -1245,1432,24,3 | False | XAML |
| 90 | 88 | Image | Image | Icon | — | -1245,1404,24,24 | False | XAML |
| 91 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: {6D809377-6AF0-444B-8957-A3773F02200E}\Steam\Steam.exe | Приложение Steam закреплено | -1211,1392,44,48 | False | XAML |
| 92 | 91 | Image | Image | Icon | — | -1201,1404,24,24 | False | XAML |
| 93 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: io.github.sykia.soundcloud-desktop | Приложение SoundCloud Desktop закреплено | -1167,1392,44,48 | False | XAML |
| 94 | 93 | Image | Image | Icon | — | -1157,1404,24,24 | False | XAML |
| 95 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: com.squirrel.Discord.Discord | Приложение Discord —1 запущенное окно закреплено | -1123,1392,44,48 | False | XAML |
| 96 | 95 | Image | Image | Icon | — | -1113,1404,24,24 | False | XAML |
| 97 | 95 | Image | Image | OverlayIcon | — | -1099,1397,16,16 | False | XAML |
| 98 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: Valve.Steam.Client | Steam —1 запущенное окно | -1079,1392,44,48 | False | XAML |
| 99 | 98 | Image | Image | Icon | — | -1069,1404,24,24 | False | XAML |
| 100 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: OpenAI.Codex_2p2nqsd0c76g0!App | ChatGPT —1 запущенное окно | -1035,1392,44,48 | False | XAML |
| 101 | 100 | Image | Image | Icon | — | -1025,1404,24,24 | False | XAML |
| 102 | 69 | Button | Taskbar.TaskListButtonAutomationPeer | Appid: Telegram.TelegramDesktop | Telegram —1 запущенное окно | -991,1392,44,48 | False | XAML |
| 103 | 102 | Image | Image | Icon | — | -981,1404,24,24 | False | XAML |
| 104 | 102 | Image | Image | OverlayIcon | — | -967,1397,16,16 | False | XAML |
| 105 | 67 | Button | SystemTray.OmniButton | SystemTrayIcon | Часы | -72,1392,64,48 | False | XAML |
| 106 | 105 | Text | TextBlock | TimeInnerTextBlock | Время | -68,1399,57,16 | False | XAML |
| 107 | 105 | Text | TextBlock | DateInnerTextBlock | Дата | -68,1415,57,16 | False | XAML |
| 108 | 65 | Pane | WorkerW | — | — | -1613,1392,489,48 | False | Win32 |
| 109 | 108 | Pane | MSTaskListWClass | — | — | -1613,1392,489,48 | False | Win32 |

## Скрытые и условные элементы

Статусы: **Д** — обнаружен узел; **Н** — не обнаружен в текущем дереве/среди перечисленных HWND; **У** — условная функция из документации, локальное существование не доказано. «Не обнаружен» не означает «невозможен» или «удалён». Для всех строк У точные класс, AutomationId, visual parent, координаты, размеры и порядок **неизвестны**; зона в таблице — функциональное назначение, не измеренный контейнер. Причины отсутствия не устанавливались изменением настроек или подключением устройств.

| Элемент / назначение | Статус | Контейнер / место | Условия и пределы подтверждения |
|---|---|---|---|
| Скрытые InnerTextBlock трея | Д | Дети кнопок 40/46/50/57 | offscreen=true, нет RECT; запасные glyph-слоты, не отдельные команды |
| Дубли погодного содержимого | Д | 6/10 под 5; 71/75 под 70 | Одинаковые RECT и роли; причина дубля не доказана, не две Widgets-кнопки |
| ProgressIndicator | Д | Firefox: 24/89 | 24×3 px; наличие peer не доказывает текущий прогресс |
| OverlayIcon | Д | Discord 32/97; Telegram 39/104 | 16×16 px; смысл значка/счётчик не прочитан |
| Активность, подсветка, attention/мигание | У | Внутри кнопки приложения | Состояние окна/приложения; отдельные peers не найдены |
| Подписи и раздельные кнопки окон | У | TaskbarFrameRepeater | Режим группировки; текущие кнопки без текстовых подписей [S1, S9] |
| App overflow («…») и его список | Н/У | Область приложений | Нехватка места; не вызывалась, ID и HWND не подтверждены |
| Hidden-icons overflow и значки приложений | Н/У | Всплывающий контейнер вне основного UIA-корня | Кнопка открытия есть, список закрыт; состав неизвестен [S1] |
| TopLevelWindowForOverflowXamlIsland / NotifyIconOverflowWindow | Н | Отдельные top-level HWND | Имена известны из TaskbarPopupPlacement.h; дополнительное чтение EnumWindows с фильтром Overflow не нашло их |
| Значки фоновых приложений | Н/У | Системный трей или overflow | Регистрация приложением и видимость; скрытые message-window приложений не равны видимым значкам [S1] |
| Батарея / питание / зарядка | Н/У | Область Quick Settings | Батарея и устройство; скрытый SystemTray_Main не подтверждает наличие кнопки [S1] |
| Pen menu | Н/У | Системный трей | Перо и настройка [S1] |
| Touch keyboard | Н/У | Системный трей | Режим Never/Always/When no keyboard attached [S2] |
| Virtual touchpad | Н/У | Системный трей | Поддерживаемое устройство и настройка [S1] |
| Emoji and more | Н/У | Системный трей | Зависит от сборки/rollout/настройки; объявлен как Insider-функция [S3] |
| Studio Effects | Н/У | Системный трей / переход к эффектам | Совместимое оборудование и эффекты; локальная доступность не проверялась [S4] |
| Микрофон: использование | Н/У | Область уведомлений | Активный захват; индикатор privacy [S5] |
| Микрофон: mute/unmute звонка | Н/У | Панель во время совместимого звонка | Отдельная функция, не тождественна индикатору захвата; локальная поддержка неизвестна [S6] |
| Геолокация / камера: privacy | Н/У | Область трея | Использование разрешений; наличие отдельной кнопки зависит от версии [S1] |
| Notification bell / DND / focus | Н/У | Рядом с часами / область уведомлений | Уведомления и режим DND; отдельный peer отсутствует [S7] |
| Безопасное извлечение | Н/У | Трей или overflow | Поддерживаемое съёмное устройство; его наличие не проверялось [S8] |
| VPN shield | У | Наложение на сетевой значок | Поддерживаемое VPN-соединение; отдельный peer/визуальная форма не подтверждены [S10] |
| Wi-Fi/Ethernet/cellular/airplane/offline | У | Варианты сетевого контрола | Это состояния/варианты, а не гарантированно отдельные кнопки; активное имя сети не публикуется |
| Mute / отсутствие устройства звука | У | Варианты звукового контрола | Состояние аудио; не переключалось |
| Часы с секундами / сокращённая дата / скрытые часы | У | Текущие OmniButton и text-потомки | Варианты настроек; текущие RECT относятся только к данному снимку [S1] |
| Chat/Teams, Copilot и иные версии служебных кнопок | Н/У | Зависит от поколения Windows | Не объявляются обязательными для данной сборки; отдельные системные peers не найдены. Кнопка обычного приложения не равна системному контролу |
| Обновление/перезапуск, безопасность, синхронизация, Bluetooth, сторонние агенты | У | Зарегистрированные tray icons/overflow | Кандидаты по состоянию системы и ПО; состав закрытого overflow неизвестен, локальные ID не установлены |
| Remote-session/совместное использование/запись экрана | У | Возможные состояние/индикатор трея | Сценарии не запускались; отдельные кнопки этой сборки не доказаны |
| TaskbarBackground / BackgroundControl | У (код проекта) | Внутренний XAML | Известен селектор из TaskbarVisualLayout.h; UIA его не раскрыл, RECT неизвестен |
| SystemTray.SystemTrayFrame | У (код проекта) | Внутренний XAML | Селектор из проекта; нельзя подменять им наблюдаемого UIA parent InputSite |

## Условные всплывающие поверхности

Это связанные с Taskbar поверхности, а не постоянно существующие дочерние контролы её HWND. Для них не получены локальные ID, класс, родитель, RECT, размер или порядок. Статус всех строк — **не открывались / не исследованы**.

| Поверхность / назначение | Условие | Что могло бы входить; локально не подтверждено |
|---|---|---|
| Start menu | Активация Пуск | Закреплённое, приложения, поиск, профиль, питание |
| Search | Активация поиска | Поле, результаты, highlights; это отдельная поверхность |
| Task View / desktop preview | Активация/hover Task View | Окна, рабочие столы, создание/переключение |
| Widgets board | Активация/hover Widgets | Карточки и команды доски |
| Quick Settings | Активация сетевой/звуковой области | Связь, звук, яркость, accessibility; состав зависит от оборудования |
| Calendar / Notification Center | Активация часов/уведомлений | Календарь, уведомления, focus/DND |
| Input switcher / IME | Активация индикатора ввода | Языки, раскладки, IME-режимы |
| Hidden icons | Активация chevron | Реальные зарегистрированные значки и их меню |
| App overflow | Переполнение и активация | Кнопки приложений, не вместившиеся в основную область |
| Thumbnail preview / группа окон | Hover/активация группы | Миниатюры, заголовки, закрытие, необязательная toolbar [S11] |
| Jump List / app context menu | Вторичное действие кнопки | Задачи, недавнее, закрепление/закрытие [S11] |
| Taskbar context menu | Вторичное действие фона | Системные команды; уже работающий UmbrellaMenuHost может расширять меню |
| Tooltips | Hover | Подсказки, сведения о состоянии; не отдельные постоянные кнопки |
| Share window in meeting | Совместимый звонок и preview | Условная команда; локально не вызвана [S1] |

Нельзя вывести конечный список всех tray icons из одного закрытого состояния: приложения динамически регистрируют значки. UIA raw tree также не перечисляет неинстанцированные XAML-шаблоны. Без запуска условных сценариев или вмешательства абсолютная полнота недостижима. Размеры и внутренние имена отсутствующих объектов намеренно не придуманы.

## Источники условных функций

Сведения ниже использованы только для условий появления, а не как доказательство локального дерева. Прочитаны/найдены 19 октября 2026; Insider-анонсы не доказывают включённость функции в текущем сеансе.

- [S1 — Microsoft: Customize the Taskbar](https://support.microsoft.com/en-us/windows/experience/personalization/customize-the-taskbar-in-windows) — области и необязательные элементы.
- [S2 — Microsoft: Touch keyboard](https://support.microsoft.com/en-us/windows/hardware/input-devices/enable-and-disable-the-touch-keyboard-in-windows) — условия видимости.
- [S3 — Microsoft: Insider 27858](https://blogs.windows.com/windows-insider/2025/05/16/announcing-windows-11-insider-preview-build-27858-canary-channel/) — Emoji tray entry.
- [S4 — Microsoft Learn: Studio Effects](https://learn.microsoft.com/en-us/windows/apps/develop/windows-integration/studio-effects) — поддержка и вход с панели.
- [S5 — Microsoft: Camera and microphone privacy](https://support.microsoft.com/en-us/windows/privacy/windows-camera-microphone-and-privacy) — индикатор микрофона.
- [S6 — Microsoft: Insider 22494](https://blogs.windows.com/windows-insider/2021/11/03/announcing-windows-11-insider-preview-build-22494/) — call mute.
- [S7 — Microsoft: Notifications and DND](https://support.microsoft.com/en-us/windows/experience/notifications-and-do-not-disturb-in-windows) — уведомления и колокольчик.
- [S8 — Microsoft: Safely remove hardware](https://support.microsoft.com/en-us/windows/hardware/safely-remove-hardware-in-windows) — извлечение устройств.
- [S9 — Microsoft: Taskbar and Start, 2026](https://blogs.windows.com/windows-insider/2026/05/15/improving-windows-quality-making-taskbar-and-start-more-personal/) — новые варианты панели, не гарантия локальной доступности.
- [S10 — Microsoft: Windows 11 innovation, 2023](https://blogs.windows.com/windowsexperience/2023/05/23/announcing-new-windows-11-innovation-with-features-for-secure-efficient-it-management-and-intuitive-user-experience/) — VPN overlay.
- [S11 — Microsoft Learn: Taskbar Extensions](https://learn.microsoft.com/en-us/windows/win32/shell/taskbar-extensions) — progress, overlay, thumbnail toolbars, Jump Lists.

## Итог и неизвестное

**Обнаружено:** две нижние панели, их HWND-иерархия и 110 связанных UIA-узлов; 18 кнопок основной панели и 13 дополнительной; точные ID/классы, родители, порядок и физическая геометрия доступных элементов; скрытые служебные HWND, offscreen glyph-узлы, дубли Widgets, progress и overlay-потомки. Дополнительная панель повторяет центральные приложения и Widgets, но из системных кнопок содержит только часы.

**Неизвестно:** состав закрытого overflow; полный внутренний XAML visual tree; фактический родитель и геометрия нераскрытых background/tray frame; ID и размеры неинстанцированных условных элементов; точная причина их отсутствия; содержимое flyout/меню, результаты действий; поведение при других настройках, DPI, устройствах, звонках и состояниях. Повторное чтение позднее вернуло 109 UIA-узлов вместо 115 без ошибок: динамическое дерево меняется, таблица фиксирует первоначальный снимок, а не постоянную схему ОС. Ни перестановки, ни реализация конструктора, ни изменение приложения не выполнялись.
