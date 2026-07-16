# ESP32-DIV — доработки поверх v1.7.0 (ветка `feat/battery-ip5306`)

База: тег `v1.7.0` (коммит `3f9f87e`). Плата: **ESP32-DIV v2 (ESP32-S3)**.
Сборка: Arduino-CLI, core `esp32 @ 2.0.10`, раздел `Minimal SPIFFS (min_spiffs)`,
Flash 16 МБ. Дисплей — bundled `TFT_eSPI` + `User_Setup v2.h`.

Версия прошивки на экране увеличивается при каждой перепрошивке: `v1.7.0-1` … текущая **`v1.7.0-8`**
(`ESP32DIV_VERSION` в `shared.h`).

---

## 1. Индикатор заряда батареи

**Проблема:** после обновления до 1.7.0 заряд всегда показывал 0%.

**Причина (двойная):**
- `BATTERY_ADC_PIN` в `shared.h` имел fallback `-1`, ни один board-блок его не переопределял → `analogRead(-1)` = 0.
- Даже с верным пином GPIO2 старая формула `*2.2` (полная шкала АЦП, настроенная под классический ESP32) на S3 даёт заниженное напряжение → 0%.

**Решение:**
- По схеме v2 подтверждено: IP5306 распаян в **I2C‑режиме** (адрес `0x75`) на общей с PCF8574 шине. Заряд читается напрямую из IP5306 (регистр `0x78` → 25/50/75/100%, `0x70`/`0x71` → зарядка/полный), что надёжнее АЦП и совпадает с 4 LED‑индикаторами.
- Резервный путь: калиброванный `analogReadMilliVolts` на `BATTERY_ADC_PIN = 2` (задан в `BoardConfig.h`) с делителем ×2.
- **100% вместо 99%:** статус‑бар делает `map((long)(V*100),300,420,…)` — усечение float `4.2` → 419 → 99%. Добавлен эпсилон `+0.006 В`, теперь каждая ступень (вкл. 100%) округляется точно.
- **Индикатор на 1px уже справа:** ширина зелёной заливки ограничена 18px, чтобы на 100% оставался пустой пиксель до рамки (не было наложения).

Файлы: `BoardConfig.h`, `utils.cpp` (`readBatteryVoltage`, `readIP5306Percent`, статус‑бар).

## 2. Фикс бесконечной перезагрузки (bootloop)

**Проблема:** после добавления I2C‑чтения заряда прошивка ушла в вечный `RTCWDT_RTC_RST`.

**Причина:** глобальная переменная `float currentBatteryVoltage = readBatteryVoltage();`
инициализируется **до `setup()`**, когда I2C/шина ещё не готовы → `Wire` блокируется → watchdog.

**Решение:** глобал инициализируется константой `0.0f`; реальное чтение перенесено в `setup()`/`loop()`.
Файлы: `ESP32-DIV.ino`.

## 3. Устранение гонки на шине I2C

**Проблема:** навигация в Settings «залипала»/скакала, Replay/Jammer зависали при входе.

**Причина:** фоновая задача статус‑бара (`statusBarTask`) каждые 400 мс вызывала IP5306‑чтение (I2C) **одновременно** с чтением кнопок PCF8574 из основной задачи. `Wire` (TwoWire) не защищён для перекрывающихся repeated‑start транзакций между задачами → шина портилась/зависала.

**Решение:**
- `statusBarTask` больше не делает I2C — использует кэш `currentBatteryVoltage`.
- Батарея читается только в основной задаче (`loop()` раз в 2 с + циклы фич) — весь I2C в одной задаче.
- Дополнительно добавлен мьютекс `i2cLock()/i2cUnlock()` вокруг чтения IP5306 и кнопок (подстраховка).

Файлы: `utils.cpp`, `utils.h`, `ESP32-DIV.ino`.

## 4. Модель ввода кнопок (одиночное нажатие vs удержание)

**Требование:** удержание повторяет действие **только** для Влево/Вправо в SubGHz → Replay Attack и Jammer. Везде остальное и Вверх/Вниз — **одиночное нажатие** (для повтора отпустить и нажать снова).

**Особенность железа:** кнопки PCF8574 на этой плате при удержании **дребезжат** (0↔1 ~каждые 30 мс) — выявлено логированием пинов.

**Решение:**
- `isButtonPressed()` переведён на **фронт с дебаунсом**: срабатывает один раз на нажатие; повторный «взвод» — только после устойчивого отпускания ≥70 мс (дребезжащее удержание не считается новыми нажатиями).
- Добавлен `isButtonHeld()` (уровень) для циклов ожидания отпускания и для hold частоты.
- Все навигационные и feature‑места через `isButtonPressed` стали одиночными; циклы `while(isButtonPressed)` и негативные проверки переведены на `isButtonHeld`.
- Причина «перепутанных кнопок» и «застревания курсора» ранее — фантомные touch‑nav слоты: `isTouchNavSlotDown` читал тач «мягким» порогом (`readTouchXYDismiss`) и ловил шум. Переведён на твёрдый `readTouchXY`.

Файлы: `ESP32-DIV.ino` (`isButtonPressed`/`isButtonHeld`, touch‑порог), `utils.cpp`, `bluetooth.cpp`, `wifi.cpp`, `gps.cpp`, `rfid.cpp`, `utils.h`.

## 5. Меню Settings

- Навигация в Settings читает только физические кнопки (`isPhysicalButtonPressed`), без фантомного touch‑nav.
- Добавлен пункт **«SubGHz Freq»** — переключатель режима частоты (фиксированные пресеты / ручная настройка). Сохраняется в настройках (`AppSettings.subghzManualFreq`, JSON).

Файлы: `utils.cpp`, `SettingsStore.h`, `SettingsStore.cpp`.

## 6. SubGHz — ручная настройка частоты + плавное ускорение

- Новый режим (тумблер «SubGHz Freq» = Manual): в **Replay Attack** и **Jammer** кнопки Влево/Вправо меняют частоту CC1101 напрямую (`setMHZ`) с шагом **0.01 МГц** (диапазон 300–928 МГц), а не циклируют пресеты.
- **Плавное ускорение при удержании** (`subghzFreqAccelStep`): тап = точный 0.01 МГц; удержание → интервал повтора сокращается 180→12 мс и шаг растёт 0.01→0.05→0.10→0.25 МГц (быстрый свип, точность на тапах). Терпимо к дребезгу.
- Фиксированный режим (пресеты) — прежнее циклирование по 200 мс.
- Единый путь ввода в Replay/Jammer: Вверх/Вниз — одиночные (`isButtonPressed`), Влево/Вправо — hold (`isButtonHeld`); покрывает и физические кнопки, и экранный nav‑бар.

Файлы: `subghz.cpp`.

## 7. Меню Jammer — прочие фиксы

- Надпись **«Mode:»** отображалась как «ode:» — ячейка значения частоты (x40, ширина 96) чёрным прямоугольником затирала букву «M» метки на x130. Ширина ячейки уменьшена до 85.

Файлы: `subghz.cpp`.

## 8. Boot‑экран: детектирование железа (из PR #179, довязано)

- Probes из `hardware_detect.h` вызваны в `setup()`: I2C‑скан PCF8574 (0x20–0x27), SPI‑проба nRF24 и CC1101 (с возвратом SPI на SD), UART‑проба GPS (NMEA), проба PN532 через `RfidNfc::begin()`.
- На экране статуса при загрузке выводятся: NRF24, CC1101, GPS, **PN532**, **PCF8574**, SD Card (со статусом present/absent). В Serial — сводка `[HW] summary`.

Файлы: `ESP32-DIV.ino`, `hardware_detect.h`.

## 9. NeoPixel — индикация работы радиомодулей и RFID (v1.7.0-8)

Цепочка из **4 адресуемых светодиодов WS2812** на выводе **IO1** (`NEOPIXEL_PIN` в `BoardConfig.h`;
на плате v2 IO1 свободен — значение `1` в `shared.h` относится только к V1). Новый модуль
`neopixel.h`/`neopixel.cpp` на библиотеке Adafruit NeoPixel.

Раскладка пикселей и цвета:

| Пиксель | Источник | Событие | Цвет |
|---------|----------|---------|------|
| 0 / 1 / 2 | nRF24 #1 / #2 / #3 | приём | зелёный (0,255,0) |
| 0 / 1 / 2 | nRF24 #1 / #2 / #3 | передача | красный (255,0,0) |
| 3 | CC1101 | приём | зелёный |
| 3 | CC1101 | передача | красный |
| 3 | PN532 RFID/NFC | чтение метки | синий (0,0,255) |
| 3 | PN532 RFID/NFC | запись блока | оранжевый (255,140,0) |

Пиксель 3 делится между CC1101 и RFID — они никогда не активны одновременно.

Как заведено в код (без дублирования «сырых» вызовов):
- **CC1101**: все переходы состояния идут через обёртки `cc1101GoRx()/cc1101GoTx()/cc1101GoIdle()`
  в `subghz.cpp`, которые вызывают `ELECHOUSE_cc1101.SetRx/SetTx/setSidle` и красят пиксель 3.
- **nRF24**: в джаммере/анализаторе все три модуля зажигаются красным при старте несущей
  (`configureRadio` сразу поднимает carrier), гаснут на выходе; 2.4 ГГц сканер зажигает пиксель 0
  зелёным через RAII-гвард `NrfScannerLedGuard` на время приёма.
- **RFID**: RAII-гварды `RfidReadLedGuard` (синий) на цикле прослушки метки и `RfidWriteLedGuard`
  (оранжевый) на операциях записи блоков — гаснут на любом пути выхода.

Общий тумблер **Settings → NeoPixel** (`AppSettings.neopixelEnabled`) гасит/зажигает всю цепочку;
драйвер хранит «логический» цвет каждого пикселя, поэтому после повторного включения состояние
восстанавливается. Яркость ограничена `NEOPIXEL_BRIGHT_MAX` (64) в `shared.h`.

Файлы: `neopixel.h` (new), `neopixel.cpp` (new), `BoardConfig.h` (`NEOPIXEL_PIN`),
`subghz.cpp` (обёртки CC1101), `bluetooth.cpp` (nRF24 RX/TX), `rfid.cpp` (гварды read/write),
`ESP32-DIV.ino` (`neoPixelInit`/`neoPixelSetEnabled` в setup), `SettingsStore.*` (`neopixelEnabled`),
`utils.cpp` (пункт меню). Библиотека: **Adafruit NeoPixel** (+ Adafruit BusIO).

---

## Влитый PR #179 (RadDad87: bugfixes-and-features) + наши правки к нему

Влит апстрим‑PR «Bug fixes & safety improvements»:
- **MAC‑рандомизация**: исправлено, что случайный адрес генерировался, но не применялся. **Портировано с Bluedroid на NimBLE** (наш фикс): `esp_ble_gap_set_rand_addr()` не компилировался/тянул Bluedroid и переполнял раздел → заменено на `ble_hs_id_set_rnd()` + `setOwnAddrType(BLE_OWN_ADDR_RANDOM)`, старший байт random‑static адреса.
- **GPS**: валидация NMEA‑контрольной суммы перед приёмом.
- **Кнопки**: 36 «голых» busy‑wait заменены на хелперы с таймаутом (защита от зависания на залипшей кнопке).
- **WiFi**: добавлены exit‑cleanup для PacketMonitor, BeaconSpammer, WifiScan.
- **hardware_detect.h**: новый модуль детекта периферии (см. п.8).
- Прочее: Beats Fit Pro typo, SourApple rand, BLE address mask.

Файлы: `bluetooth.cpp`, `gps.cpp`, `wifi.cpp`, `ESP32-DIV.ino`, `hardware_detect.h`.

---

## Сводка по файлам

| Файл | Что изменено |
|------|--------------|
| `shared.h` | `ESP32DIV_VERSION` → `v1.7.0-7` |
| `BoardConfig.h` | `#define BATTERY_ADC_PIN 2` (v2) |
| `SettingsStore.h/.cpp` | поле `subghzManualFreq` + сохранение/загрузка |
| `utils.h/.cpp` | IP5306‑чтение, I2C‑мьютекс, статус‑бар (100%/1px), Settings (физ. кнопки, пункт SubGHz Freq), кэш батареи |
| `ESP32-DIV.ino` | `isButtonPressed` (дебаунс‑фронт)/`isButtonHeld`, touch‑порог, i2cGuardInit, HW‑probes в setup, фикс bootloop, рефреш батареи в loop |
| `subghz.cpp` | ручная частота + ускорение, единый ввод Replay/Jammer, фикс «Mode:», согласование кнопок |
| `bluetooth.cpp`,`wifi.cpp`,`gps.cpp`,`rfid.cpp` | PR #179 + перевод release‑wait на `isButtonHeld`; PN532‑проба |
| `hardware_detect.h` | новый модуль (PR #179), задействован на boot |
| `neopixel.h/.cpp` | новый модуль: 4 WS2812 на IO1, индикация nRF24/CC1101/RFID |

## Сборка и прошивка

- **Плата:** ESP32-S3 Dev Module, core esp32 2.0.10, PartitionScheme=min_spiffs, Flash 16MB.
- **Библиотеки:** bundled TFT_eSPI (User_Setup v2) + SmartRC-CC1101; NimBLE-Arduino 1.4.3, PCF8574 (Mischianti), RF24, IRremoteESP8266, arduinoFFT 1.6.2, ArduinoJson 6, rc-switch, XPT2046_Touchscreen, Adafruit PN532, **Adafruit NeoPixel** (+ Adafruit BusIO).
- **Линковка:** нужен `-zmuldefs` (оверрайд `ieee80211_raw_frame_sanity_check` и общий символ `spi` между TFT_eSPI и CC1101).
- **Прошивка:** merged‑bin на offset `0x0` (USB), либо app‑bin как `firmware.bin` через меню Update Firmware → SD Card (без USB).
