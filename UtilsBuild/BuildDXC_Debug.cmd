CALL "../3rdParty/DirectXShaderCompiler/utils/hct/hctstart.cmd" ../3rdParty/DirectXShaderCompiler ../../UtilsBuild/DirectXShaderCompiler.bin
CALL "utils/hct/hctbuild.cmd" -Debug

cd ../../..
if not exist "Utils" mkdir Utils
if not exist "Utils/DXC" mkdir Utils/DXC

move /Y "UtilsBuild/DirectXShaderCompiler.bin/Debug" "Utils/DXC/Debug"
copy dxil.dll "Utils/DXC/Debug/bin/dxil.dll"