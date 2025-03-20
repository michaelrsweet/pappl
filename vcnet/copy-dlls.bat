:: Script to copy dependent DLLs to the named build directory

copy packages\libcups3_native.redist.3.0.0-rc4\build\native\bin\x64\Release\*.dll %1
copy packages\libcups3_native.redist.3.0.0-rc4\build\native\bin\x64\Debug\*.dll %1

copy packages\libjpeg-turbo-v142.2.0.4.3\build\native\bin\x64\v142\Release\*.dll %1
copy packages\libjpeg-turbo-v142.2.0.4.3\build\native\bin\x64\v142\Debug\*.dll %1

copy packages\libpng_native.redist.1.6.30\build\native\bin\x64\Release\*.dll %1
copy packages\libpng_native.redist.1.6.30\build\native\bin\x64\Debug\*.dll %1

copy packages\libressl_native.redist.4.0.0\build\native\bin\x64\Release\*.dll %1
copy packages\libressl_native.redist.4.0.0\build\native\bin\x64\Debug\*.dll %1

copy packages\zlib_native.redist.1.2.11\build\native\bin\x64\Release\*.dll %1
copy packages\zlib_native.redist.1.2.11\build\native\bin\x64\Debug\*.dll %1
