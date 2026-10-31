<!-- project-presentation -->
<p align="center"><img src="assets/umbrella.png" alt="Umbrella Shell" width="88"></p>
<h1 align="center">Umbrella Shell</h1>
<p align="center">Панель задач Windows 11 — с вашей раскладкой.</p>
<p align="center"> <strong>Windows 11 x64 · C++20 · Qt 6.8.3 · Win32</strong></p>
<p align="center"><a href="https://github.com/sykia/umbrella-shell/releases/tag/v1.0.2"><img src="docs/presentation/action.svg" alt="Открыть предварительный релиз" width="280" height="60"></a></p>

Настройка штатной панели задач Explorer: расположение, размеры, прозрачность и группы. Отдельно — часы-виджеты для рабочего стола.

## Три вида панели

| Слитный | Отдельный | Островки |
| :--- | :--- | :--- |
| Единая поверхность панели | Отступы и отдельное оформление | Визуальное разделение групп |

- Панель сверху, снизу, слева или справа; настройка длины, толщины и прозрачности.
- Конструктор системных групп Left / Center / Right.
- Часы с горизонтальным и вертикальным расположением цифр, цветами и blur.
- Светлая, тёмная и контрастная тема; Mica/Acrylic с непрозрачным fallback.

## Установка и восстановление

**Проект находится в предварительном выпуске.** Требуется Windows 11 x64, build 22000 или новее; доступные операции зависят от совместимости системных DLL. Автообновления отключены.

1. Откройте [релиз v1.0.2](https://github.com/sykia/umbrella-shell/releases/tag/v1.0.2), прочитайте ограничения и выберите установщик.
2. После установки откройте Umbrella Shell из меню «Пуск».
3. Для отката используйте **«Вернуть исходный вид»** или пункт **Restore taskbar appearance** в меню «Пуск».

Приложение оформляет существующую панель, не заменяя оболочку Windows. Сценарии, которые ещё требуют проверки, перечислены в [VERIFICATION.md](docs/VERIFICATION.md).

**[Возможности панели](docs/TASKBAR.md) · [Конструктор](docs/TASKBAR_CONFIGURATION.md) · [Архитектура](docs/ARCHITECTURE.md) · [Восстановление](docs/STARTUP_RECOVERY.md)**


<details>
<summary><strong>Полная документация, установка и технические подробности</strong></summary>

<div align="center">
  <img src="assets/umbrella.png" width="150" alt="Umbrella Shell">
  <h1>Umbrella Shell</h1>
  <p><strong>Современная настройка панели задач и часы-виджеты для Windows 11.</strong></p>
  <p>
    <a href="https://github.com/sykia/umbrella-shell/releases/latest">Скачать</a> ·
    <a href="docs/VERIFICATION.md">Проверки</a> ·
    <a href="docs/ARCHITECTURE.md">Архитектура</a> ·
    <a href="https://github.com/sykia/umbrella-shell/issues">Сообщить о проблеме</a>
  </p>
</div>

Umbrella Shell оформляет стандартную панель задач Explorer, не заменяя оболочку Windows. Приложение работает для текущего пользователя, не устанавливает службу, не завершает Explorer и не использует сеть. Текущая версия — **1.0.2**.

> [!IMPORTANT]
> Нативная интеграция зависит от версии системных DLL Windows 11. Umbrella проверяет совместимость и откатывает неподтверждённые изменения, но перед установкой прочитайте [известные ограничения](#совместимость-и-ограничения).

## Возможности

### Панель задач

- три вида: **Слитный**, **Отдельный** и **Островки**;
- расположение сверху, снизу, слева или справа;
- длина 40–100%, толщина 48–96 DIP, прозрачность и скругления;
- независимые профили мониторов для поддерживаемых конфигураций;
- визуальный конструктор системных групп Left / Center / Right;
- скрытие обнаруженных поддерживаемых компонентов и восстановление исходной раскладки;
- пересчёт рабочей области Explorer при изменении толщины;
- автоматическое сохранение подтверждённой конфигурации.

Состав доступных действий определяется реальной панелью и возможностями текущей сборки Windows. Недоступные операции блокируются до изменения системы. Подробности: [возможности Taskbar](docs/TASKBAR.md) и [конструктор панели](docs/TASKBAR_CONFIGURATION.md).

### Clock Widgets

- горизонтальный формат `00:00` и вертикальный `00 / 00`;
- 12 цветов цифр и настраиваемые интервалы;
- пустой фон, blur-карточка или blur только внутри цифр;
- размещение под значками рабочего стола через проверяемый слой Explorer;
- один минутный таймер для всех экземпляров часов;
- прозрачные окна, не перехватывающие ввод вне редактора.

### Интерфейс и восстановление

- Fluent 2, Mica/Acrylic и непрозрачный fallback;
- системная светлая, тёмная и контрастная тема, акцентный цвет и reduced motion;
- скрытие окна по Escape, Alt+F4 или кнопке закрытия без остановки оформления;
- отдельная команда полного выхода с восстановлением панели;
- журнал исходного состояния и ограниченный worker аварийного восстановления;
- резервная CLI-команда `UmbrellaShell.exe --restore-taskbar-style`.

## Установка

Требования: **Windows 11 x64, build 22000 или новее**.

1. Откройте [последний релиз](https://github.com/sykia/umbrella-shell/releases/latest).
2. Скачайте `UmbrellaShell-1.0.2-windows-x64-setup.exe` и одноимённый файл `.sha256`.
3. Проверьте контрольную сумму при необходимости:

   ```powershell
   Get-FileHash .\UmbrellaShell-1.0.2-windows-x64-setup.exe -Algorithm SHA256
   ```

4. Запустите установщик и подтвердите UAC. Umbrella устанавливается в
   `C:\Program Files\Umbrella Shell`; системная служба не создаётся.

После установки откройте **Umbrella Shell** из меню «Пуск» или **Umbrella** в контекстном меню панели задач. Пользовательские настройки находятся вне каталога установки и сохраняются при обновлении и удалении.

## Безопасное восстановление

- **Вернуть исходный вид** восстанавливает оформление и отключает его автоматическое применение.
- **Завершить Umbrella** полностью закрывает GUI и возвращает исходное оформление; помощник меню может продолжить работу.
- Пункт **Restore taskbar appearance** в меню «Пуск» запускает резервное восстановление.
- Удаление останавливает процессы Umbrella, восстанавливает панель и убирает автозапуск.

Umbrella никогда не завершает Explorer и не изменяет Winlogon, системную оболочку, политики или общесистемный автозапуск. Совместный сбой процессов, зависание Explorer и потеря питания не покрываются гарантией восстановления.

## Совместимость и ограничения

- Поддерживается только Windows 11 x64; нативные модули привязаны к совместимым системным DLL.
- Разные вертикальные стороны на нескольких мониторах одновременно не поддерживаются.
- Автоскрытие доступно только снизу при полной ширине и стандартной толщине без островков.
- Mixed-DPI, RDP, все варианты Snap Layouts, смена топологии и чистая VM требуют дополнительной проверки.
- Автообновления отключены: новая версия устанавливается вручную. Причины и security gates описаны в [UPDATES.md](docs/UPDATES.md).
- Если релиз не подписан Authenticode, Windows может показать предупреждение неизвестного издателя. SHA-256 не заменяет цифровую подпись.

Точные подтверждённые сценарии и невыполненные проверки перечислены в [VERIFICATION.md](docs/VERIFICATION.md).

## Сборка из исходников

Требуются Visual Studio 2022 с MSVC/Windows SDK, CMake 3.25+, Qt **6.8.3 msvc2022_64** и PowerShell. Для упаковки нужен Inno Setup **6.4.3**.

```powershell
$env:CMAKE_PREFIX_PATH = 'D:/Qt/6.8.3/msvc2022_64'
cmake --preset windows-msvc
cmake --build --preset release
ctest --preset release
./scripts/Test-Foundation.ps1
./scripts/Test-PublicRepository.ps1
```

Полный unsigned installer:

```powershell
./scripts/Build-Package.ps1
./scripts/Test-DeployedApp.ps1
```

`Build-Package.ps1 -Signed` работает только с уже существующим авторизованным сертификатом и `signtool`; ключи не хранятся в Git. Core можно собрать отдельно без Qt и Windows:

```powershell
cmake -S . -B build/core -DUMBRELLA_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```

Дополнительные команды и правила выпуска находятся в [RELEASING.md](docs/RELEASING.md).

## Архитектура

```text
src/core/                 платформенно-независимые модели и инварианты
src/platform/windows/     Win32, Explorer, DWM и native adapters
src/app/                  composition root и view models
src/presentation/         Qt Quick / QML
```

Core не зависит от Qt и Windows. Приложение владеет композицией, platform adapters — системными handles, Qt Quick — D3D11 rendering, а QML — представлением. Подробнее: [ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Документация

- [Taskbar: возможности и ограничения](docs/TASKBAR.md)
- [Конфигурация панели](docs/TASKBAR_CONFIGURATION.md)
- [Автозапуск и восстановление](docs/STARTUP_RECOVERY.md)
- [Контекстное меню](docs/TASKBAR_CONTEXT_MENU.md)
- [Жизненный цикл настроек](docs/SETTINGS_LIFETIME.md)
- [Зависимости и лицензии](docs/THIRD_PARTY.md)
- [История изменений](CHANGELOG.md)

Автор проекта — [sykia](https://github.com/sykia). Лицензия собственного кода владельцем пока не выбрана; отсутствие файла LICENSE не предоставляет разрешения на копирование, изменение или распространение кода.

</details>

---

[sykia](https://github.com/sykia) · [TG Mod](https://github.com/sykia/tdesktop) · [Blockyard](https://github.com/sykia/blockyard) · [SoundCloud Desktop](https://github.com/sykia/soundcloud-desktop) · [Umbrella Shell](https://github.com/sykia/umbrella-shell)
