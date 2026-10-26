# Сборка и выпуск

Текущая версия — 1.0.1. Релиз содержит установщик Windows x64 и SHA-256.
Наличие Authenticode определяется только фактической конфигурацией защищённого
GitHub environment `release`. Отсутствие подписи и невыполненные испытания не
должны скрываться в описании.

## Разработческая сборка

Требования/команды — в [README](../README.md). `Build-Package.ps1` требует
отсутствующего stage; используйте чистый checkout либо сохраните старый deployment.
Скрипт собирает MSVC/Qt, выполняет CTest, deploy Qt, создаёт runtime-inventory.json
с версиями/SHA-256 и компилирует Inno Setup 6.4.3. README/CHANGELOG, docs и
уведомления лицензий входят в пакет. Подпись включается только явным `-Signed`.

CI Windows проверяет сборку, восемь CTest, упаковку и deployed smoke с системным
PATH без Qt development paths. Linux проверяет только независимый core.
Результаты прежней Shell Bar к текущему коду не относятся: [VERIFICATION.md](VERIFICATION.md).

## Публикация

1. Проверить diff, metadata, `Test-Foundation.ps1`, actionlint и staged index через
   `Test-PublicRepository.ps1`. Проверить дерево коммита перед push.
2. Дождаться CI текущих исходников. Обновить notes, changelog и версию CMake.
   Проверить уведомления/исходники фактически поставляемых зависимостей.
3. Подготовить draft для точного SHA. Создать соответствующий тег `v1.0.1`.
   `release.yml` повторно собирает/проверяет/упаковывает приложение и публикует assets.
4. Без signing secrets workflow выпускает **unsigned prerelease**. При наличии
   обоих секретов подписывает собственные exe/DLL, setup/uninstaller и проверяет
   Authenticode. Частично заданные секреты — ошибка, а не переход к unsigned.
5. Существующий опубликованный release не меняется. Разрешён только заранее
   подготовленный draft для того же SHA; assets загружаются без перезаписи.
   После выпуска проверить фактические assets, checksum и CI, скачать пакет.

GitHub environment `release` используется для будущих защищённых signing secrets
`WINDOWS_CERTIFICATE_BASE64` / `WINDOWS_CERTIFICATE_PASSWORD`. PFX/key/password
не входят в Git/логи. Hardware/cloud-only ключ требует отдельного adapter.
Подпись приложения сама по себе не включает автообновления и не гарантирует SmartScreen.
Название релиза не заменяет фактическую матрицу испытаний ниже.

## Матрица дальнейших испытаний

- Чистая Windows 11 x64 без Qt/SDK, обычный пользователь, кириллический путь/пробелы.
- Per-user установка без UAC, diagnostics, GUI/меню и реальный logon helper.
- Скрытие/возврат настроек, полный выход с восстановлением панели.
- Повторная установка/upgrade, открытое меню/занятые DLL, отмена и недостаток места.
- Uninstall: процессы/Run/ярлыки удалены, оформление возвращено, настройки сохранены.
- Четыре стороны, два монитора, DPI/RDP, Explorer restart, normal/crash recovery.
- На неподдерживаемой сборке корректный отказ; длительные CPU/GPU/handle измерения.
- Для подписанных сборок — подписи installed files/uninstaller и SmartScreen отдельно.

Матрица пока не выполнена. In-place Inno не даёт транзакционного отката после
потери питания. Автообновления остаются выключены до отдельных условий
[UPDATES.md](UPDATES.md); unsigned preview скачивается/устанавливается вручную.
