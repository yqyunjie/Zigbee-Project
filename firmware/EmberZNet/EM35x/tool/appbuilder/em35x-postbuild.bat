@echo off
REM Post Build processing for IAR Workbench.
SET PROJECT_DIR=%1%
SET TARGET_BPATH=%2%

echo " "
echo "This converts S37 to Ember Bootload File format if a bootloader has been selected in AppBuilder"
echo " "
@echo on
cmd /c ""%ISA3_UTILS_DIR%\em3xx_convert.exe" _replace_em3xxConvertFlags_ "%TARGET_BPATH%.s37" "%TARGET_BPATH%.ebl" > "%TARGET_BPATH%-em3xx-convert-output.txt"
@echo off
type %TARGET_BPATH%-em3xx-convert-output.txt

echo " "
echo "This creates a ZigBee OTA file if the "OTA Client Policy Plugin" has been enabled.
echo "It uses the parameters defined there.  "
echo " "
@echo on
cmd /c "_replace_imageBuilderCommand_ > %TARGET_BPATH%-image-builder-output.txt"
@echo off
type %TARGET_BPATH%-image-builder-output.txt

