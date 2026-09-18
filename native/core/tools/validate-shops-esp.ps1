$ErrorActionPreference='Stop'
$env:IDF_PATH='C:/esp/v6.1/esp-idf'
$env:IDF_TOOLS_PATH='C:/Espressif/tools'
$env:IDF_PYTHON_ENV_PATH='C:/Espressif/tools/python/v6.1/venv'
$env:ESP_IDF_VERSION='6.1'
$env:ESP_ROM_ELF_DIR='C:/Espressif/tools/esp-rom-elfs/20241011'
$env:PYTHONUTF8='1'
$env:PATH='C:/Espressif/tools/python/v6.1/venv/Scripts;C:/Espressif/tools/cmake/4.0.3/bin;C:/Espressif/tools/ninja/1.12.1;C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin;'+$env:PATH
$base=(Resolve-Path native/core/build-shops/baseline-source/native/targets/tdeck).Path
python C:/esp/v6.1/esp-idf/tools/idf.py -C $base -B (Join-Path $base build-core) build size *> native/core/build-shops/idf-baseline.log
if($LASTEXITCODE -ne 0){Get-Content native/core/build-shops/idf-baseline.log -Tail 45;exit $LASTEXITCODE}
python C:/esp/v6.1/esp-idf/tools/idf.py -C native/targets/tdeck -B (Join-Path (Get-Location) native/targets/tdeck/build-core) build size *> native/core/build-shops/idf-build-size.log
if($LASTEXITCODE -ne 0){Get-Content native/core/build-shops/idf-build-size.log -Tail 45;exit $LASTEXITCODE}
python native/targets/tdeck/package_launcher.py --build-dir native/targets/tdeck/build-core *> native/core/build-shops/launcher.log
if($LASTEXITCODE -ne 0){Get-Content native/core/build-shops/launcher.log;exit $LASTEXITCODE}
xtensa-esp32s3-elf-g++ -std=c++17 -I native/core/include -c native/core/tools/measure-shop-sizes.cpp -o native/core/build-shops/sizes.o
xtensa-esp32s3-elf-nm -S --size-sort --radix=d native/core/build-shops/sizes.o *> native/core/build-shops/esp-sizes.txt
foreach($name in @('shops','shop_orchestration')) {
  xtensa-esp32s3-elf-g++ -std=c++17 -Os -fstack-usage -fno-exceptions -fno-rtti -I native/core/include -c "native/core/src/$name.cpp" -o "native/core/build-shops/$name.o"
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
}
Get-Content native/core/build-shops/*.su | Sort-Object { [int](($_ -split "`t")[1]) } -Descending | Select-Object -First 15 | Set-Content native/core/build-shops/largest-stack.txt
Get-Item (Join-Path $base build-core/openu5_tdeck.bin),native/targets/tdeck/build-core/openu5_tdeck.bin | Select-Object FullName,Length
Get-Content native/core/build-shops/esp-sizes.txt
Get-Content native/core/build-shops/idf-build-size.log -Tail 25
