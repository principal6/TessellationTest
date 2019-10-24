#include "Header.hlsli"

cbuffer cbTessFactor
{
	float TessFactor;
	float3 Pads;
};

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(InputPatch<VS_OUTPUT, 3> Patch, uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

	Output.EdgeTessFactor[0] = TessFactor;
	Output.EdgeTessFactor[1] = TessFactor;
	Output.EdgeTessFactor[2] = TessFactor;

	Output.InsideTessFactor = TessFactor;

	/*
	if (all(Patch[0].WorldNormal == Patch[1].WorldNormal) && all(Patch[0].WorldNormal == Patch[2].WorldNormal))
	{
		Output.EdgeTessFactor[0] = 1.0f;
		Output.EdgeTessFactor[1] = 1.0f;
		Output.EdgeTessFactor[2] = 1.0f;

		Output.InsideTessFactor = 1.0f;
	}
	*/
	
	return Output;
}

[domain("tri")]
[maxtessfactor(64.0f)]
[outputcontrolpoints(3)]
[outputtopology("triangle_cw")]
[partitioning("fractional_odd")]
[patchconstantfunc("CalcHSPatchConstants")]
HS_OUTPUT main(InputPatch<VS_OUTPUT, 3> Patch, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID )
{
	HS_OUTPUT Output;

	Output = Patch[i];

	return Output;
}

[domain("tri")]
[maxtessfactor(64.0f)]
[outputcontrolpoints(3)]
[outputtopology("triangle_cw")]
[partitioning("fractional_even")]
[patchconstantfunc("CalcHSPatchConstants")]
HS_OUTPUT main_even(InputPatch<VS_OUTPUT, 3> Patch, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	HS_OUTPUT Output;

	Output = Patch[i];

	return Output;
}

[domain("tri")]
[maxtessfactor(64.0f)]
[outputcontrolpoints(3)]
[outputtopology("triangle_cw")]
[partitioning("integer")]
[patchconstantfunc("CalcHSPatchConstants")]
HS_OUTPUT main_integer(InputPatch<VS_OUTPUT, 3> Patch, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	HS_OUTPUT Output;

	Output = Patch[i];

	return Output;
}