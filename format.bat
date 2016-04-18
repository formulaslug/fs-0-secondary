@rem skips source control metadata folders and hidden files (.*)

set FOLDERS=src

@rem format C++ code
dir /s /b %FOLDERS% | findstr /v "^\." | findstr "\.cpp$ \.h$ \.inc$" | uncrustify.exe -c uncrustify.cfg -F - -l CPP --replace --no-backup

@rem format C code
dir /s /b %FOLDERS% | findstr /v "^\." | findstr "\.c$" | uncrustify.exe -c uncrustify.cfg -F - -l C --replace --no-backup

@rem format Java code
dir /s /b %FOLDERS% | findstr /v "^\." | findstr "\.java$" | uncrustify.exe -c uncrustify.cfg -F - -l JAVA --replace --no-backup
