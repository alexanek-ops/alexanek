# Bytebeat Synth (VST3)

Оригинальный полифонический синтезатор, где звук задаётся **формулой от времени `t`**
(так называемый bytebeat) — идея, знакомая по 8-бит синтам. 8 голосов, MIDI, пресеты,
VST3 для Ableton Live 11+ (а также Standalone; AU на macOS).

Это не копия какого-либо коммерческого плагина: интерфейс, код и название собственные.

## Как это работает

Каждая нажатая нота запускает счётчик `t`. Он идёт со скоростью `Rate` Гц (для ноты C4),
а для других нот ускоряется/замедляется по высоте. Формула считается при каждом изменении `t`,
результат берётся по модулю 256 (`& 255`) и становится 8-битным сэмплом — отсюда хрустящий звук.

| Что | Описание |
|---|---|
| Переменные | `t` время, `n` MIDI-нота, `v` velocity (0–127), `f` частота ноты в Гц |
| Операторы | `+ - * / %`  `<< >>`  `& \| ^ ~ !`  `< > <= >= == !=`  `&& \|\|`  `a ? b : c` (приоритеты как в C) |
| Функции | `sin(x)` `cos(x)` (период 256, диапазон −127..127), `abs` `min` `max` `sqrt` |
| Числа | целые, десятичные и `0x..`. Дробей нет: вместо `t*1.5` пишите `t*3/2` |

Примеры: `t*2|t|t/2`, `t&t>>8`, `t*((t>>12|t>>8)&63&t>>4)`, `128+sin(t)`.

Если формула с ошибкой, под полем появится сообщение, а предыдущая рабочая формула продолжит играть.
Формула сохраняется вместе с проектом Ableton.

Ручки: Rate, Octave, Semi, Cents, Bits (1–8, «bitcrush»), Attack, Release, Volume.

## Сборка

Нужны: **CMake 3.22+**, git, компилятор (Windows: Visual Studio 2022 с «Desktop development with C++»;
macOS: Xcode). JUCE скачается автоматически при первой конфигурации (нужен интернет).

### Windows
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```
Готовый плагин: `build\BytebeatSynth_artefacts\Release\VST3\Bytebeat Synth.vst3`.
Скопируйте его (папку целиком) в `C:\Program Files\Common Files\VST3\`.
(Автокопирование после сборки туда же сработает только если терминал запущен от администратора.)

### macOS
```
cmake -B build -G Xcode
cmake --build build --config Release
```
Копируется в `~/Library/Audio/Plug-Ins/VST3/` автоматически. Если Live не видит плагин на Apple Silicon:
`codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/"Bytebeat Synth.vst3"`

### В Ableton Live
Preferences → Plug-Ins → включите *Use VST3 Plug-In System Folders* → *Rescan*.
Плагин появится в Browser → Plug-Ins → VST3 как **Bytebeat Synth**. Перетащите на MIDI-дорожку.

## Тесты движка (без JUCE)
```
./Tests/run_tests.sh
```

## Структура
- `Source/FormulaEngine.h` — парсер и вычислитель формул
- `Source/SynthCore.h` — голоса, высота тона, огибающая, bitcrush
- `Source/PluginProcessor.*` — параметры, MIDI, состояние, пресеты
- `Source/PluginEditor.*` — интерфейс
