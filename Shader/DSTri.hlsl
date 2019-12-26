#include "Base.hlsli"

cbuffer cbSpace : register(b0)
{
	float4x4 ViewProjection;
}

cbuffer cbDisplacement : register(b1)
{
	bool UseDisplacement;
	float DisplacementFactor;
	float2 Pads;
}

SamplerState CurrentSampler : register(s0);
Texture2D DisplacementTexture : register(t0);

[domain("tri")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT ConstantData, float3 Domain : SV_DomainLocation, const OutputPatch<HS_OUTPUT, 3> ControlPoints)
{
	DS_OUTPUT Output;

	Output.TexCoord = ControlPoints[0].TexCoord * Domain.x + ControlPoints[1].TexCoord * Domain.y + ControlPoints[2].TexCoord * Domain.z;
	Output.bUseVertexColor = ControlPoints[0].bUseVertexColor + ControlPoints[1].bUseVertexColor + ControlPoints[2].bUseVertexColor;

	float4 P1 = ControlPoints[0].WorldPosition;
	float4 P2 = ControlPoints[1].WorldPosition;
	float4 P3 = ControlPoints[2].WorldPosition;
	float4 N1 = normalize(ControlPoints[0].WorldNormal);
	float4 N2 = normalize(ControlPoints[1].WorldNormal);
	float4 N3 = normalize(ControlPoints[2].WorldNormal);

	float4 BezierPosition = GetBezierPosition(P1, P2, P3, N1, N2, N3, Domain);
	float4 BezierNormal = GetBezierNormal(P1, P2, P3, N1, N2, N3, Domain);
	/*
	if (UseDisplacement)
	{
		float Displacement = DisplacementTexture.SampleLevel(CurrentSampler, Output.TexCoord.xy, 0).r;
		BezierPosition = BezierPosition + BezierNormal * (Displacement * DisplacementFactor);
	}
	*/
	Output.Position = Output.WorldPosition = BezierPosition;
	Output.WorldNormal = BezierNormal;

	if (Output.bUseVertexColor == 0)
	{
		Output.Position = mul(float4(Output.Position.xyz, 1), ViewProjection);
	}

	Output.Color = ControlPoints[0].Color * Domain.x + ControlPoints[1].Color * Domain.y + ControlPoints[2].Color * Domain.z;

	Output.WorldTangent = ControlPoints[0].WorldTangent * Domain.x + ControlPoints[1].WorldTangent * Domain.y + ControlPoints[2].WorldTangent * Domain.z;
	Output.WorldTangent = normalize(Output.WorldTangent);

	Output.WorldBitangent = ControlPoints[0].WorldBitangent * Domain.x + ControlPoints[1].WorldBitangent * Domain.y + ControlPoints[2].WorldBitangent * Domain.z;
	Output.WorldBitangent = normalize(Output.WorldBitangent);

	return Output;
}
