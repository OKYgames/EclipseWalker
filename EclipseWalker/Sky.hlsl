cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld; 
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

TextureCube gCubeMap : register(t0);
SamplerState gsamLinearWrap : register(s2);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosL = vin.PosL;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);    
    // 최종 화면 위치 계산
    vout.PosH = mul(posW, gViewProj);
    vout.PosH.z = vout.PosH.w;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 texColor = gCubeMap.Sample(gsamLinearWrap, pin.PosL);
    float4 redFilter = float4(1.0f, 0.2f, 0.2f, 1.0f); 
    return texColor * redFilter;
}