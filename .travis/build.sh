#!/bin/bash

echo "Configuring for ${BUILD_CONFIGURATION}"

if [[ "$TRAVIS_OS_NAME" == "windows" ]]; then
  cmake ../ -G "Visual Studio 15 2017 Win64"
  cmake --build . --config ${BUILD_CONFIGURATION}
else
  cmake -DCMAKE_BUILD_TYPE=$BUILD_CONFIGURATION ../
  VERBOSE=1 cmake --build .
fi