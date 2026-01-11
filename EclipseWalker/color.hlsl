cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld; 
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW; 
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

struct VertexIn
{
    float3 PosL  : POSITION;
    float3 NormalL : NORMAL; 
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float3 PosW  : POSITION; // 월드 좌표 (조명 계산용)
    float3 NormalW : NORMAL; // 월드 법선 (조명 계산용)
    float4 Color : COLOR;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout;


    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    vout.PosH = mul(posW, gViewProj);
    
    vout.Color = vin.Color;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 N = normalize(pin.NormalW);
    
    float3 lightDir = normalize(float3(-1.0f, -1.0f, 1.0f)); 
    float3 L = -lightDir; // 빛을 향하는 벡터

    float3 ambient = float3(0.3f, 0.3f, 0.3f) * pin.Color.rgb;

    float diffuseFactor = max(dot(N, L), 0.0f);
    float3 diffuse = diffuseFactor * float3(1.0f, 1.0f, 1.0f) * pin.Color.rgb;

    float3 toEye = normalize(gEyePosW - pin.PosW); 
    float3 halfVec = normalize(L + toEye); 
    float specularFactor = pow(max(dot(N, halfVec), 0.0f), 64.0f);
    float3 specular = specularFactor * float3(0.5f, 0.5f, 0.5f); 

    float3 finalColor = ambient + diffuse + specular;

    return float4(finalColor, pin.Color.a);
}