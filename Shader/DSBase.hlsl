#include "Header.hlsli"

cbuffer cbSpace : register(b0)
{
	float4x4 VP;
}

[domain("tri")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT TessFactors, float3 Domain : SV_DomainLocation, const OutputPatch<HS_OUTPUT, 3> Patch)
{
	DS_OUTPUT Output;
	
	Output = Patch[0];

	Output.Position = Patch[0].WorldPosition * Domain.x + Patch[1].WorldPosition * Domain.y + Patch[2].WorldPosition * Domain.z;
	Output.Position = mul(float4(Output.Position.xyz, 1.0f), VP);

	return Output;
}
