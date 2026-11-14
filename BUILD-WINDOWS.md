# Получение готового VST3 для Windows

В репозитории настроена автоматическая сборка Windows x64 через GitHub Actions.
Сборка использует Visual Studio 2022 и загружает JUCE 8.0.12 автоматически.

1. Создайте пустой репозиторий на GitHub.
2. Загрузите в него всё содержимое этой папки проекта `BytebeatSynth` (включая скрытую папку `.github`).
3. Откройте вкладку **Actions** репозитория и разрешите запуск GitHub Actions, если GitHub попросит.
4. После завершения workflow **Build Windows VST3** откройте его запуск.
5. Внизу страницы в разделе **Artifacts** скачайте `BytebeatSynth-Windows-VST3`.
6. Распакуйте ZIP. Внутри будет папка `Bytebeat Synth.vst3`.
7. Скопируйте эту папку целиком в `C:\Program Files\Common Files\VST3\`.
8. В Ableton Live откройте Settings → Plug-Ins → Rescan.

Сборка может занять несколько минут. Для неё не нужен установленный на вашем компьютере Visual Studio — компиляция выполняется на GitHub.
