#include "Quad.hlsli"

cbuffer cbTessFactor : register(b0)
{
	float EdgeTessFactor;
	float InsideTessFactor;
	float2 Pads;
}

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

	Output.EdgeTessFactor[0] = Output.EdgeTessFactor[1] =
		Output.EdgeTessFactor[2] = Output.EdgeTessFactor[3] = EdgeTessFactor;

	Output.InsideTessFactor[0] = Output.InsideTessFactor[1] = InsideTessFactor;

	return Output;
}

[domain("quad")]
[maxtessfactor(64.0)]
[outputcontrolpoints(1)]
[outputtopology("triangle_ccw")]
[partitioning("integer")]
[patchconstantfunc("CalcHSPatchConstants")]
HS_OUTPUT main(uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	HS_OUTPUT Output;

	Output.HemisphereDirection = ((float)PatchID) * 2.0 - 1.0;

	return Output;
}