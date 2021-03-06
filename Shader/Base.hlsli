#include "Shared.hlsli"

struct VS_INPUT
{
	float4 Position		: POSITION;
	float4 Color		: COLOR;
	float3 TexCoord		: TEXCOORD;
	float4 Normal		: NORMAL;
	float4 Tangent		: TANGENT;
};

struct VS_OUTPUT
{
	float4	Position		: SV_POSITION;
	float4	WorldPosition	: POSITION;
	float4	Color			: COLOR;
	float3	TexCoord		: TEXCOORD;
	float4	WorldNormal		: NORMAL;
	float4	WorldTangent	: TANGENT;
	float4	WorldBitangent	: BITANGENT;
	int		bUseVertexColor : BOOL;
};

#define HS_OUTPUT VS_OUTPUT
#define DS_OUTPUT HS_OUTPUT

struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[3]	: SV_TessFactor;
	float InsideTessFactor : SV_InsideTessFactor;
};