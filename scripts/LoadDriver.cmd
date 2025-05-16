set sysfile=%CD%\%1
set drvname=%~n1

@echo LOAD DRVNAME %drvname% for file %sysfile%

@echo STOP/DELETE POTENTIAL PREVIOUS SERVICE
@sc.exe stop %drvname%
@sc.exe delete %drvname%

@echo COPY DRIVER IN SYSTEM32\DRIVERS
@echo copy %sysfile% c:\windows\system32\drivers\
@copy %sysfile% c:\windows\system32\drivers\

@echo CREATE SERVICE DEVICE
@sc.exe create %drvname% type= kernel start= demand error= normal binPath= c:\windows\system32\drivers\%drvname%.sys DisplayName= %drvname%

@echo START SERVICE DEVICE
@sc.exe start %drvname%
