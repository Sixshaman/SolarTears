import urllib.request
import re
import os
import zipfile

if __name__ == "__main__":
    latest_release = str(urllib.request.urlopen("https://github.com/microsoft/DirectXShaderCompiler/releases/latest").read())
    
    release_links = re.findall("/microsoft/DirectXShaderCompiler/releases/download/.*/dxc_\\d{4}\_\\d{2}-\\d{2}.zip", latest_release)
    if len(release_links) != 1:
        exit(1)

    zipfile_name      = release_links[0][release_links[0].rfind('/') + 1:]
    zipfile_full_name = os.path.join(os.environ['TEMP'], zipfile_name)

    latest_dxil_release_link     = "https://github.com" + release_links[0]
    latest_dxil_release_response = urllib.request.urlopen(latest_dxil_release_link)

    with open(zipfile_full_name, 'wb') as f:
        f.write(latest_dxil_release_response.read())
        f.close()

    z = zipfile.ZipFile(zipfile_full_name)
    z.extract("bin/x64/dxil.dll", "./")
    z.close()