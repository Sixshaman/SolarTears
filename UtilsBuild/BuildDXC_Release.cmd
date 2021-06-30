CALL "../3rdParty/DirectXShaderCompiler/utils/hct/hctstart.cmd" ../3rdParty/DirectXShaderCompiler ../../UtilsBuild/DirectXShaderCompiler.bin

if not exist external/taef python utils/hct/hctgettaef.py
CALL "utils/hct/hctbuild.cmd" -Release -vs2019

cd ../../..
if not exist "Utils" mkdir Utils
cd Utils
if not exist "DXC" mkdir DXC
cd ..

move /Y "UtilsBuild/DirectXShaderCompiler.bin/Release" "Utils/DXC/Release