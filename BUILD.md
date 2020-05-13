Build Instructions
==================

PAPPL requires a POSIX-compliant host operating system such as Linux®, macOS®,
QNX®, or VxWorks®, a C99 compiler like Clang or GCC, a `make` program that
supports the `include` directive, and the following support libraries:

- CUPS 2.2 or later for the CUPS libraries (libcups2/libcupsimage2)
- GNU TLS 2.8 or later (except on macOS) for TLS support
- JPEGLIB 9 or later for JPEG image support
- LIBPNG 1.6 or later for PNG image support
- ZLIB 1.1 or later for compression support


Getting Prerequisites
---------------------

CentOS 7/Fedora 22-/RHEL 7:

    sudo yum groupinstall 'Development Tools'
    sudo yum install avahi-devel cups-devel gnutls-devel libjpeg-turbo-devel \
        libpng-devel libusbx-devel zlib-devel

CentOS 8/Fedora 23+/RHEL 8:

    sudo dnf groupinstall 'Development Tools'
    sudo dnf install avahi-devel cups-devel gnutls-devel libjpeg-turbo-devel \
        libpng-devel libusbx-devel zlib-devel

Raspbian/Ubuntu:

    sudo apt-get install build-essential libavahi-client-dev libcups2-dev \
        libcupsimage2-dev libgnutls28-dev libjpeg-dev libpng-dev \
        libusb-1.0-0-dev zlib1g-dev

macOS (after installing Xcode from the AppStore):

    (install brew if necessary)
    brew install libjpeg
    brew install libpng
    brew install libusb

(or then download, build, and install libjpeg, libpng, and libusb from source)


Building PAPPL
--------------

PAPPL uses the usual `configure` script to generate a `make` file:

    ./configure [options]
    make

Use `./configure --help` to see a full list of options.

There is also an Xcode project under the `xcode` directory that can be used on
macOS:

    open xcode/pappl.xcodeproj

You can test the build by running the PAPPL test program:

    testsuite/testpappl


Installing PAPPL
----------------

Once you have successfully built PAPPL, install it using:

    sudo make install

By default everything will be installed under `/usr/local`.  Use the `--prefix`
configure option to override the base installation directory.  Set the
`DESTDIR`, `DSTROOT`, or `RPM_BUILD_ROOT` environment variables to redirect the
installation to a staging area, as is typically done for most software packaging
systems (using one of those environment variables...)
