cd ../Sources/3rdParty/DirectXShaderCompiler
"utils/hct/hctstart.cmd" ./ ../../../UtilsBuild/DirectXShaderCompiler.bin

hctbuild -Release

cd ../../..
if not exist "Utils" mkdir Utils
if not exist "Utils/DXC" mkdir Utils/DXC

move /Y "UtilsBuild/DirectXShaderCompiler.bin/Release" "Utils/DXC/Release"
copy dxil.dll "Utils/DXC/Release/bin/dxil.dll"