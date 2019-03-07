@echo off
forge build --release
copy _bin\release\forge.exe .
copy _bin\release\forge.exe %install_path%
