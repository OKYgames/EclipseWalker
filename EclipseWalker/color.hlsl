cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld; 
    float4 gDiffuseAlbedo; 
    float3 gFresnelR0;     
    float  gRoughness;    
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

Texture2D    gDiffuseMap : register(t0); // 텍스처 (슬롯 2번 -> t0)
SamplerState gsamLinear  : register(s2); // 샘플러 (LinearWrap 사용)

struct VertexIn
{
    float3 PosL  : POSITION;
    float3 NormalL : NORMAL; 
    float2 TexC  : TEXCOORD; 
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float3 PosW  : POSITION; 
    float3 NormalW : NORMAL; 
    float2 TexC  : TEXCOORD; // 픽셀 쉐이더로 UV 넘김
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.PosH = mul(posW, gViewProj);
    
    // 텍스처 좌표 전달
    vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 N = normalize(pin.NormalW);
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 lightDir = -normalize(float3(-1.0f, -1.0f, 1.0f));
    float3 L = lightDir;

    // 텍스처에서 색상 뽑아오기 (Sample)
    // 재질 색상(gDiffuseAlbedo)과 텍스처 색상(texColor)을 곱해서 최종 색 결정
    float4 texColor = gDiffuseMap.Sample(gsamLinear, pin.TexC) * gDiffuseAlbedo;

    // 조명 계산 
    float3 ambient = float3(0.1f, 0.1f, 0.1f) * texColor.rgb;

    float diffuseFactor = max(dot(N, L), 0.0f);
    float3 diffuse = diffuseFactor * texColor.rgb;

    float3 halfVec = normalize(L + toEyeW);
    float m = (1.0f - gRoughness) * (1.0f - gRoughness);
    float shininess = (1.0f - m) / (m + 0.0001f);
    shininess *= 256.0f;
    float specFactor = pow(max(dot(N, halfVec), 0.0f), shininess);
    float3 specular = specFactor * gFresnelR0;

    float3 finalColor = ambient + diffuse + specular;

    return float4(finalColor, texColor.a);
}