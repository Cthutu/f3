# Overview

Forge will generate project files, run unit tests, build you source code, and manage dependencies for your C++ projects.

# Building

Make sure the environment variable %INSTALL_PATH% is defined on your system, preferably pointing to a folder that is also in your %PATH%
environment variable.  Then just run `install.bat`.  This will build forge using forge itself (the exe in the root), and copy the result
to both the root (updating the forge.exe used to build itself) and the folder pointed to by %INSTALL_PATH%.

# Usage

Run `forge.exe` to see the commands.

## new command

This will generate a new project.  It supports these flags:

| Flag            | Description
|-----------------|-------------------------------------------------------------
| --exe           | (optional) generate a binary application project.
| --lib           | generate a library project with unit tests.
| --dll           | generate a DLL project.
| --windows       | generate a Win32 project, other a console project will be generated instead

## edit command

This will generate Visual Studio solution and project files, and launch the IDE.

| Flag            | Description
|-----------------|-------------------------------------------------------------
| --gen           | Do not open the IDE, just do the generation.

## clean command

Building and generate IDE files create generated files that are always store in folders in the root that start with an underscore.  This
command will remove those folders.

## build command

Build the project and create the binary.

| Flag            | Description
|-----------------|-------------------------------------------------------------
| --release       | Build the release version, otherwise debug is built instead.
| --v/--verbose   | Output the actual command lines used to build the project.


