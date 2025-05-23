@echo CLEAN REGISTRY KEY
@reg delete HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\filter_win64

@echo ADD REGISTRY KEY
@reg import filter_win64.reg

@echo COPY FILE
@copy x64\Debug\filter_win64.sys c:\Windows\system32\drivers

@echo LOAD FILTER
@fltmc load filter_win64
