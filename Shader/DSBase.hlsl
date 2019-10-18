#include "Header.hlsli"

cbuffer cbSpace : register(b0)
{
	float4x4 VP;
}

[domain("tri")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT TessFactors, float3 Domain : SV_DomainLocation, const OutputPatch<HS_OUTPUT, 3> Patch)
{
	DS_OUTPUT Output;
	
	Output.Position = Patch[0].WorldPosition * Domain.x + Patch[1].WorldPosition * Domain.y + Patch[2].WorldPosition * Domain.z;
	Output.Position = mul(float4(Output.Position.xyz, 1.0f), VP);

	Output.WorldPosition = Patch[0].WorldPosition * Domain.x + Patch[1].WorldPosition * Domain.y + Patch[2].WorldPosition * Domain.z;
	Output.Color = Patch[0].Color * Domain.x + Patch[1].Color * Domain.y + Patch[2].Color * Domain.z;
	Output.UV = Patch[0].UV * Domain.x + Patch[1].UV * Domain.y + Patch[2].UV * Domain.z;
	Output.WorldNormal = Patch[0].WorldNormal * Domain.x + Patch[1].WorldNormal * Domain.y + Patch[2].WorldNormal * Domain.z;
	Output.WVPNormal = Patch[0].WVPNormal * Domain.x + Patch[1].WVPNormal * Domain.y + Patch[2].WVPNormal * Domain.z;

	return Output;
}
