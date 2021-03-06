#include "ShaderFactory.h"
#include <Windows.h>
#include "Debug.h"
#include "DebugMenu.h"

ShaderFactory gShaderFactory;

HANDLE hotreloadHandle;

void ShaderFactory::CompileAllShadersFromFile()
{
    WIN32_FIND_DATAW data;
    HANDLE file = FindFirstFileW(L"Shaders/*.hlsl", &data);

    if (file == INVALID_HANDLE_VALUE)
    {
        HR(GetLastError());
    }

    ShaderItem shaderItem;

    shaders.clear();
    shaderMap.clear();

    do
    {
        shaderItem.filename = data.cFileName;
        shaders.push_back(shaderItem);
    } while (FindNextFileW(file, &data) != 0);

    FindClose(file);

    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#endif

    for (int i = 0; i < shaders.size(); i++)
    {
        shaderMap[shaders[i].filename] = &shaders[i];

        const char* vsEntry = "VSMain";
        const char* vsTarget = "vs_5_0";

        std::wstring directory;
        directory += L"Shaders/";
        directory += shaders[i].filename;

        ID3DBlob* vertexError;
        HRESULT hr = D3DCompileFromFile(directory.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, vsEntry, vsTarget, flags, 0, &shaders[i].vertexCode, &vertexError);
        if (vertexError && (hr != S_OK))
        {
            const char* errMsg = (char*)vertexError->GetBufferPointer();
            OutputDebugString(errMsg);
            MessageBox(0, errMsg, vsEntry, 0);
        }

        const char* psEntry = "PSMain";
        const char* psTarget = "ps_5_0";
        ID3DBlob* pixelError;
        HR(D3DCompileFromFile(directory.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, psEntry, psTarget, flags, 0, &shaders[i].pixelCode, &pixelError));
        if (pixelError)
        {
            const char* errMsg = (char*)pixelError->GetBufferPointer();
            OutputDebugString(errMsg);
            MessageBox(0, errMsg, psEntry, 0);
        }
    }
}

void ShaderFactory::InitHotLoading()
{
    hotreloadHandle = FindFirstChangeNotificationA("Shaders", false, FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hotreloadHandle == INVALID_HANDLE_VALUE)
    {
        DebugPrint("Handle for shader reload file tracking invalid\n");
    }
}

void ShaderFactory::CleanUpShaders()
{
    for (int i = 0; i < shaders.size(); i++)
    {
        shaders[i].vertexCode->Release();
        shaders[i].pixelCode->Release();

        shaders[i].vertexShader->Release();
        shaders[i].pixelShader->Release();
    }
}

void ShaderFactory::HotReloadShaders()
{
    //TODO: right now Im stuck recompiling every single shader if I can't get the filename
    //Reloading the shaders also pushes the camera back on the z-axis. Figure that one out (random negative val?)

    //Potentially engine could let the previous vertex/shader object buffers be used until new compile is ready

    //REF: https://docs.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications

    bool nextChange = FindNextChangeNotification(hotreloadHandle);
    if (nextChange)
    {
        DebugPrint("Shader reload start...\n");
        CleanUpShaders();
        CompileAllShadersFromFile();
        gRenderSystem.CreateAllShaders();
        DebugPrint("Shader reload complete\n");

        //g_DebugMenu->notifications.push_back(DebugNotification(L"Shader reload complete."));
    }

    FindCloseChangeNotification(hotreloadHandle);
    InitHotLoading();
}
