cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld; 
    float4 gDiffuseAlbedo; // 재질의 본래 색상
    float3 gFresnelR0;     // 반사율 (금속성)
    float  gRoughness;     // 거칠기 (0~1)
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
    float3 PosW  : POSITION; 
    float3 NormalW : NORMAL; 
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
    // (1) 법선과 시선 벡터 정규화
    float3 N = normalize(pin.NormalW);
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // (2) 빛의 방향 (임시: 1시 방향 위쪽에서 오는 빛)
    float3 lightDir = -normalize(float3(-1.0f, -1.0f, 1.0f));
    float3 L = lightDir;

    // (3) 조명 계산 (Blinn-Phong)

    // [Ambient] 환경광
    float3 ambient = float3(0.1f, 0.1f, 0.1f) * gDiffuseAlbedo.rgb;

    // [Diffuse] 확산광 (난반사)
    float diffuseFactor = max(dot(N, L), 0.0f);
    float3 diffuse = diffuseFactor * gDiffuseAlbedo.rgb;

    // [Specular] 정반사광 (하이라이트)
    float3 halfVec = normalize(L + toEyeW);
    
    // 거칠기(Roughness)를 광택 계수(Shininess)로 변환하는 공식
    // 거칠수록(1.0) 반짝임이 퍼지고 약해짐, 매끄러울수록(0.0) 쨍하고 좁게 맺힘
    float m = (1.0f - gRoughness) * (1.0f - gRoughness);
    float shininess = (1.0f - m) / (m + 0.0001f); 
    shininess *= 256.0f; // 값 범위 확장

    float specFactor = pow(max(dot(N, halfVec), 0.0f), shininess);
    
    // FresnelR0(반사율)을 곱해서 금속/비금속 느낌 차이 냄
    float3 specular = specFactor * gFresnelR0; 

    // (4) 최종 색상 합산
    float3 finalColor = ambient + diffuse + specular;

    return float4(finalColor, gDiffuseAlbedo.a);
}