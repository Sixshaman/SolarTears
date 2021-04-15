cd ../Sources/3rdParty/DirectXShaderCompiler
"utils/hct/hctstart.cmd" ./ ../../../UtilsBuild/DirectXShaderCompiler.bin

hctbuild -Debug
hctbuild -Release

cd ../../..
if not exist "Utils" mkdir Utils
if not exist "Utils/DXC" mkdir Utils/DXC

move /Y "UtilsBuild/DirectXShaderCompiler.bin/Debug" "Utils/DXC/Debug"
move /Y "UtilsBuild/DirectXShaderCompiler.bin/Release" "Utils/DXC/Release"