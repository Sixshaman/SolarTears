cd ../../UtilsBuild
python ./DownloadDxil.py

cd ./bin/x64
move /Y dxil.dll "../../../Utils/DXC/Debug/bin/dxil.dll"
cd ../../
rmdir /s /q bin