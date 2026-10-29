
call .venv\Scripts\activate.bat

set ZES_ENABLE_SYSMAN=1
set ZEPHYR_TOOLCHAIN_VARIANT=zephyr

set WORKSPACE=%cd%
if "%ZEPHYR_SDK_INSTALL_DIR%"=="" set ZEPHYR_SDK_INSTALL_DIR=%WORKSPACE%\zephyr-sdk-0.16.8

call zephyr-rtos\zephyr-env.cmd


echo ================ Zephyr ENV =======================
echo ZES_ENABLE_SYSMAN=%ZES_ENABLE_SYSMAN%
echo ZEPHYR_TOOLCHAIN_VARIANT=%ZEPHYR_TOOLCHAIN_VARIANT%
echo ZEPHYR_SDK_INSTALL_DIR=%ZEPHYR_SDK_INSTALL_DIR%
