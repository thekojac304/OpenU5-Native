$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Path native/core/build-magic -Force | Out-Null
$env:IDF_PATH='C:/esp/v6.1/esp-idf'
$env:IDF_TOOLS_PATH='C:/Espressif/tools'
$env:IDF_PYTHON_ENV_PATH='C:/Espressif/tools/python/v6.1/venv'
$env:ESP_IDF_VERSION='6.1'
$env:ESP_ROM_ELF_DIR='C:/Espressif/tools/esp-rom-elfs/20241011'
$env:PYTHONUTF8='1'
$env:PATH='C:/Espressif/tools/python/v6.1/venv/Scripts;C:/Espressif/tools/cmake/4.0.3/bin;C:/Espressif/tools/ninja/1.12.1;C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin;'+$env:PATH
python C:/esp/v6.1/esp-idf/tools/idf.py -C native/targets/tdeck -B C:/Dev/OpenU5-TDeck/native/targets/tdeck/build-core build size 2>&1 | Tee-Object native/core/build-magic/idf-build-size.log
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
python native/targets/tdeck/package_launcher.py --build-dir native/targets/tdeck/build-core 2>&1 | Tee-Object native/core/build-magic/launcher.log
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
xtensa-esp32s3-elf-g++ -std=c++17 -I native/core/include -c native/core/tools/measure-magic-sizes.cpp -o native/core/build-magic/sizes.o
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
xtensa-esp32s3-elf-nm -S --size-sort --radix=d native/core/build-magic/sizes.o | Tee-Object native/core/build-magic/esp-sizes.txt
xtensa-esp32s3-elf-g++ -std=c++17 -Os -fstack-usage -fno-exceptions -fno-rtti -I native/core/include -c native/core/src/combat.cpp -o native/core/build-magic/combat.o
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
xtensa-esp32s3-elf-g++ -std=c++17 -Os -fstack-usage -fno-exceptions -fno-rtti -I native/core/include -c native/core/src/magic.cpp -o native/core/build-magic/magic.o
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
Get-Item native/core/build-magic/baseline.bin,native/targets/tdeck/build-core/openu5_tdeck.bin | Select-Object FullName,Length
Get-FileHash native/targets/tdeck/build-core/openu5_tdeck.bin -Algorithm SHA256
