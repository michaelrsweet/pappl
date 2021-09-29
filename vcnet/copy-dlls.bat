:: Script to copy dependent DLLs to the named build directory
copy packages\libcups2_native.redist.2.4.0.1\build\native\bin\x64\Debug\*.dll %1
copy packages\libjpeg-turbo-v142.2.0.4.3\build\native\bin\x64\v142\Debug\*.dll %1
copy packages\zlib_native.redist.1.2.11\build\native\bin\x64\Debug\*.dll %1
copy packages\zlib_native.redist.1.2.11\build\native\bin\x64\Release\*.dll %1

