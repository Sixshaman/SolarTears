cd ../../UtilsBuild
python ./DownloadDxil.py

cd ./bin/x64
move /Y dxil.dll "../../../Utils/DXC/Release/bin/dxil.dll"
cd ../../
rmdir /s /q bin