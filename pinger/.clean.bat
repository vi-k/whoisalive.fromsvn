@echo off
echo Usage: .clean.bat [exe]

del *.obj 2>nul
del *.ncb 2>nul
del /A:H *.suo 2>nul
call :delete Debug %1
call :delete Release %1
exit

:delete
echo %1...
del %1\BuildLog.htm 2>nul
del %1\mt.dep 2>nul
del %1\*.obj 2>nul
del %1\*.idb 2>nul
del %1\*.pdb 2>nul
del %1\*.ilk 2>nul
del %1\*.manifest 2>nul
del %1\*.manifest.res 2>nul
del %1\*.map 2>nul
if "%2"=="exe" del %1\*.exe 2>nul
goto :eof
