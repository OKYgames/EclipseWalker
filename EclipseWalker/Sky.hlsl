TextureCube gCubeMap : register(t0);
SamplerState gsamLinearWrap : register(s0);

cbuffer cbPass : register(b1)
{
    float4x4 gView;      
    float4x4 gInvView;   
    float4x4 gProj;      
    float4x4 gInvProj;    
    float4x4 gViewProj;  
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 cbPerObjectPad2;
    float4 gRenderTargetSize;
    float4 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};


cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

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

    float3 fixedScalePos = vin.PosL * 500.0f; 
    float4 posW = float4(fixedScalePos, 1.0f);

    vout.PosH = mul(posW, gViewProj);

    
    return vout;
}

static const float4 gRedFilter = float4(1.0f, 0.2f, 0.2f, 1.0f); 

float4 PS(VertexOut pin) : SV_Target
{

    float4 texColor = gCubeMap.Sample(gsamLinearWrap, pin.PosL);
    return texColor * gRedFilter; 
}