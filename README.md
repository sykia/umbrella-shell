# Umbrella Shell

Фундамент будущей графической оболочки Windows 11: C++20, Qt 6 / QML,
Win32, Direct3D 11. Сейчас это **консольный диагностический host без UI**.
Запуск без аргументов печатает справку и завершается. Нет фонового процесса,
сетевых запросов, автозапуска, замены Explorer и загрузки графического движка.

## Что подготовлено

- CMake, MSVC x64, presets, границы core / Windows / rendering / presentation.
- `--diagnostics`: версия, Qt, путь будущих настроек, выключенные возможности.
- `--restore-explorer`: запуск системного explorer.exe по абсолютному пути.
  Это не ремонт реестра и не гарантия восстановления панели задач.
- Политика допуска установки и фильтрация Stable / Beta с CTest-проверками.
  Политика не выполняет криптографическую проверку или загрузку обновлений.
- Inno Setup: установка для текущего пользователя, удаление через Windows,
  ярлык Explorer, отсутствие удаления пользовательских настроек.
- CI сборки, тестов и упаковки; workflow подписанного релиза по тегу.
  Фактические результаты сборок доступны во вкладке Actions репозитория.

**Не реализованы:** shell UI, регистрация оболочки, update-клиент, appcast,
фоновые загрузки/прогресс, проверка пакета на стороне приложения,
транзакционная замена, восстановление после сбоя, автоматический откат.
Автообновления намеренно отключены без переключателя включения.

## Сборка

Windows 11 x64, Visual Studio 2022 (Desktop development with C++, Windows SDK),
CMake >= 3.25, Qt 6.8.3 `msvc2022_64`, Inno Setup 6.4.3.
Откройте Developer PowerShell и укажите путь своего Qt:

```powershell
$env:CMAKE_PREFIX_PATH = 'C:\Qt\6.8.3\msvc2022_64'
cmake --preset windows-msvc
cmake --build --preset release
ctest --preset release
./build/windows-msvc/Release/UmbrellaShell.exe --diagnostics
./scripts/Build-Package.ps1
```

Build-Package повторяет сборку и тесты, затем запускает Qt deployment и Inno.
Результат: `dist/UmbrellaShell-0.1.0-windows-x64-setup.exe` и SHA-256.
Каталог `stage` должен отсутствовать: это защищает пакет от старых DLL.
Несертифицированный локальный/CI installer предназначен для разработки.
Не распространяйте его как доверенное автоматическое обновление.

Проверки core без Qt доступны на машине с C++20 toolchain:

```powershell
cmake -S . -B build/core -DUMBRELLA_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```

`./scripts/Test-Foundation.ps1` проверяет файлы, JSON, синтаксис PowerShell
и базовые ограничения установщика. Это не заменяет компиляцию или VM-тест.

## Установка и безопасный выход

Установщик размещает приложение в `%LOCALAPPDATA%\Programs\Umbrella Shell`,
не запрашивает повышение прав и создаёт uninstaller. Корпоративная политика
может запрещать установку/запуск из пользовательского каталога.
Работающий Explorer остаётся стандартной оболочкой Windows.
Удаление: «Параметры → Приложения → Установленные приложения».

Если Explorer был закрыт вручную: Task Manager → Run new task → `explorer.exe`.
Доступен также `UmbrellaShell.exe --restore-explorer`.
Будущая замена оболочки потребует отдельного opt-in и испытанного аварийного
выхода; в текущем фундаменте такой режим отсутствует.

Подробнее: [архитектура](docs/ARCHITECTURE.md),
[обновления и доверие](docs/UPDATES.md),
[релиз и проверки](docs/RELEASING.md), [лицензии](docs/THIRD_PARTY.md),
[история изменений](CHANGELOG.md).

## Репозиторий и сопровождение

Публичный репозиторий: [sykia/umbrella-shell](https://github.com/sykia/umbrella-shell).
Исходники находятся в `src/`, тесты в `tests/`, сборочные шаблоны в `cmake/`,
установщик в `packaging/windows/`, инструкции в `docs/`, автоматизация в
`scripts/` и `.github/workflows/`.

Значимые изменения добавляются в `CHANGELOG.md` → `Unreleased`.
Перед коммитом выполните `git add` для нужных файлов, затем
`./scripts/Test-PublicRepository.ps1`: проверяется содержимое Git index.
Не добавляйте личные пути, пароли, сертификаты, локальные настройки или
результаты сборки. Локальный `AGENTS.md` исключён из публичной истории.
`.gitignore` предотвращает случайное добавление, но не является защитой от
принудительного `git add -f`; состав коммита нужно проверять.
