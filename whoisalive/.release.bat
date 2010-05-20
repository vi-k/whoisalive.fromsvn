@echo off

set project-name=whoisalive
set path-to-archive=D:\My\C++\!backup

call .ver.bat -
if not %verstate%=="" set verstate=-%verstate%
set project=%project-name%-%vermajor%.%verminor%.%verrelease%%verpatch%[%verbranch%%verbuild%]%verstate%
set dir=%path-to-archive%\%project%

echo.
echo Проект: %project%
echo Путь к архиву: %path-to-archive%\
echo.


call :md

call :md \whoisalive
call :copy Release\whoisalive.exe whoisalive\
call :copy work\config-release.xml whoisalive\config.xml

call :md \pinger
call :copy pinger-Release\pinger.exe pinger\
call :copy pinger-work\config-release.xml pinger\config.xml
call :copy pinger-work\hosts-release.xml pinger\hosts.xml
call :copy pinger-work\favicon.ico pinger\
call :copy pinger-work\schemes-release.xml pinger\schemes.xml
call :md \pinger\classes
call :copy pinger-work\classes\*.* pinger\classes\
call :md \pinger\maps
call :copy pinger-work\maps\maps.xml pinger\maps\maps.xml

pkzipc -add -dir=relative %dir%.zip %dir%\*.*
rd /S /Q %dir%

goto :eof

:md
echo md:   %project%%1
md %dir%\%1 2>nul
goto :eof

:copy
echo copy: %1 -^> %2
copy %1 %dir%\%2 >nul
if not %ERRORLEVEL%==0 (
  echo Ошибка: ERRORLEVEL=%ERRORLEVEL%
  goto :fail
)
goto :eof

:fail
echo.
echo Операция завершена с ошибкой
exit