@echo CLEAN FILTER
fltmc unload filter_win64

@echo CLEAN REGISTRY KEY
reg delete HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\filter_win64

@echo CLEAN DRIVER
del c:\Windows\system32\drivers\filter_win64.sys
