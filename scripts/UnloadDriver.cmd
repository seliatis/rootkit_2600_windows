set drvname=%~n1

@echo UNLOAD DRVNAME %drvname%

@sc.exe stop %drvname%
@sc.exe delete %drvname%
