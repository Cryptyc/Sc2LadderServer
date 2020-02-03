mkdir build_vs2017
pushd build_vs2017
cmake --debug-trycompile -G "Visual Studio 16 2019" ..
popd
