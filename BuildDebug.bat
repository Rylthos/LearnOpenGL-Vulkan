cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DGLFW_BUILD_WAYLAND=0 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 .
ninja -C build
