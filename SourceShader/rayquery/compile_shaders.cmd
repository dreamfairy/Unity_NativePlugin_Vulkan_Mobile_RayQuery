

setlocal
set GLSL_COMPILER=glslangValidator.exe
set SOURCE_FOLDER="D:/GitHub/UnityMobileRayTrancing/SourceShader/rayquery/"
set BINARIES_FOLDER="D:/GitHub/UnityMobileRayTrancing/SourceShader/rayquery/output/"
set MY_FOLDER="D:/GitHub/UnityMobileRayTrancing/SourceShader/rayquery/output/"

set COPY_SRC_PROJECT_FOLDER=D:\GitHub\UnityMobileRayTrancing\SourceShader\rayquery\output\
set COPY_DST_PROJECT_FOLDER=D:\Github\UnityMobileRayTrancing\Assets\Resources\CompiledShader\

:: raygen shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S frag %SOURCE_FOLDER%ray_shadow.frag -o %BINARIES_FOLDER%ray_shadow.frag

:: closest-hit shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S vert %SOURCE_FOLDER%ray_shadow.vert -o %BINARIES_FOLDER%ray_shadow.vert


::my folder

:: raygen shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S  frag %SOURCE_FOLDER%ray_shadow.frag -o %MY_FOLDER%ray_shadowFrag.bytes

:: closest-hit shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S vert %SOURCE_FOLDER%ray_shadow.vert -o %MY_FOLDER%ray_shadowVert.bytes

copy  "%COPY_SRC_PROJECT_FOLDER%ray_shadowFrag.bytes" "%COPY_DST_PROJECT_FOLDER%ray_shadowFrag.bytes" /Y
copy  "%COPY_SRC_PROJECT_FOLDER%ray_shadowVert.bytes" "%COPY_DST_PROJECT_FOLDER%ray_shadowVert.bytes" /Y