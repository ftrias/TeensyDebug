REM  Needs 7zip
call build.bat
set ZF=..\TeensyDebug.zip
set ZC="C:\Program Files\7-zip\7z"
%ZC% u %ZF% ..\examples
%ZC% u %ZF% ..\extras
%ZC% u %ZF% ..\src
%ZC% u %ZF% ..\keywords.txt
%ZC% u %ZF% ..\library.properties
%ZC% u %ZF% ..\license.txt
%ZC% u %ZF% ..\README.md


