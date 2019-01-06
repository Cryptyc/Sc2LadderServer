#!/bin/bash

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    sudo apt-get install -y g++-5
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 90
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 90
else
    # Apply compilation fixes for OS X.
    git apply hacks/civetweb_compilation_fix.patch
fi
