#include "d3dUtil.h"
#include <d3dcompiler.h>

// 셰이더 파일을 읽어서 기계어로 번역(컴파일)하는 함수
ComPtr<ID3DBlob> d3dUtil::CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;

    // 디버그 모드일 때는 셰이더도 디버깅할 수 있게 정보를 포함시킴
#if defined(_DEBUG) || defined(DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;

    // 실제로 파일을 읽고 컴파일 시도
    hr = D3DCompileFromFile(filename.c_str(), defines, nullptr,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    // 에러가 있다면 출력창에 무슨 에러인지 띄워줌 
    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}