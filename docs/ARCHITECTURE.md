# Архитектура и границы

## Модули

| Каталог / target | Ответственность сейчас | Позже |
| --- | --- | --- |
| `src/app`, UmbrellaShell | QCoreApplication, CLI, диагностика | Композиция сервисов, lifecycle |
| `src/core`, umbrella_core | Независимая от Qt политика обновлений | Доменные модели и команды |
| `src/platform/windows`, umbrella_platform | Запуск системного Explorer, RAII закрытие handles | Изолированные адаптеры Win32 |
| umbrella_render (INTERFACE) | Граница зависимостей d3d11/dxgi | Управление device, device-loss |
| umbrella_presentation (INTERFACE) | Граница Qt Quick/QML, пока не подключена к host | View models, настоящий QML-модуль |
| `packaging/windows` | Ресурсы версии, Inno Setup | Подписанная поставка |
| `scripts`, `.github/workflows` | Сборка, deployment, публикация | Интеграционные VM-тесты |

Пустые QML-экраны, фиктивные сервисы и невостребованные устройства D3D
не создаются. QML и Direct3D представлены dependency targets, а не реализацией.
При появлении UI QCoreApplication заменяется на QGuiApplication; QML собирается
через qt_add_qml_module, deployment дополняется QML import scan. Qt Quick
использует Qt RHI: совместное владение D3D11 ресурсами потребует явного контракта
потоков/жизненного цикла; не создавать второй renderer без измерений.

## Настройки и ресурсы

Диагностика возвращает QStandardPaths::AppLocalDataLocation; каталог не создаётся.
Будущее хранилище: settings.json в этом каталоге, schemaVersion, QSaveFile
для атомарной записи, резервная копия до миграции. Секреты — Windows Credential
Manager, не JSON. Миграции только с сохранением предыдущей схемы до принятия
новой версии: откат бинарника не должен уничтожать совместимые настройки.
Установщик не пишет и не удаляет пользовательские данные.

Нет polling, таймеров, GPU allocations, сетевых клиентов и резидентного процесса.
Показатели RAM/CPU пока не измерены: нет собранного host и будущего UI.
Будущий updater — отдельный адаптер с ограниченным временем жизни, а не служба.

## Безопасность Explorer

Не трогаем HKLM/HKCU Winlogon, shell policies, автозапуск и explorer.exe.
Recovery-команда использует GetWindowsDirectoryW и CreateProcessW с явным
application name, без поиска исполняемого файла через PATH и без elevated token.
Успешный запуск процесса не доказывает работоспособность панели задач.
Любая будущая регистрация shell блокируется до отдельного решения и испытаний.
