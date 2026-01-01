@echo off
set PATH=C:\msys64\mingw64\bin;%PATH%

setlocal

echo Getting flags...
for /f "delims=" %%i in ('pkg-config --cflags --libs gtk+-3.0') do set GTK_FLAGS=%%i

echo Compiling...
gcc -o sorter.exe src/main.c src/gui/full_window.c src/gui/tab_sort.c src/gui/tab_list.c src/gui/tab_tree.c src/gui/tab_graph.c src/gui/gui_utils.c src/backend/sort.c src/backend/linked_list.c src/backend/tree.c src/backend/graph.c -Iinclude -Wall -g %GTK_FLAGS%

if %errorlevel% neq 0 (
    echo Compilation FAILED.
    exit /b %errorlevel%
)

echo Compilation SUCCESS.
endlocal
