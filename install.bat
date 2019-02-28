@echo off
forge build --release
copy _bin\release\f.exe .
copy _bin\release\f.exe %install_path%
