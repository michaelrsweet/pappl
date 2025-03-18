:: Script to copy dependent DLLs to the named build directory
copy packages\*\build\native\bin\x64\Release\*.dll %1
copy packages\*\build\native\bin\x64\Debug\*.dll %1
