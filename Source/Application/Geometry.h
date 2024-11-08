#pragma once

#include "Mesh.h"
#include <fstream>
#include <sstream>
#include <DirectXMath.h>
using namespace DirectX;

#include <type_traits>

namespace std
{
	template <> struct hash<FVertexWithNormal>
	{
		size_t operator()(const FVertexWithNormal& x) const
		{
			size_t size;
			for (size_t i = 0; i < 3; ++i)
			{
				if (i < 2)
				{
					size ^= static_cast<size_t>(x.position[i]);
					size ^= static_cast<size_t>(x.normal[i]);
					size ^= static_cast<size_t>(x.uv[i]);
				}
				else
				{
					size ^= static_cast<size_t>(x.position[i]);
					size ^= static_cast<size_t>(x.normal[i]);
				}
			}

			return size;
		}
	};
}

namespace GeometryGenerator
{
	template<class TVertex, class TIndex = unsigned>
	struct GeometryData
	{
		static_assert(std::is_same<TIndex, unsigned>() || std::is_same<TIndex, unsigned short>()); // ensure UINT32 or UINT16 indices
		std::vector<TVertex>  Vertices;
		std::vector<TIndex>   Indices;
	};

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Triangle(float size);

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Quad(float scale);

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> FullScreenQuad();

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Cube();

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Sphere(float radius, unsigned ringCount, unsigned sliceCount, int numLODLevels = 1);

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Grid(float width, float depth, unsigned tilingX, unsigned tilingY, int numLODLevels = 1);

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Cylinder(float height, float topRadius, float bottomRadius, unsigned sliceCount, unsigned stackCount, int numLODLevels = 1);

	template<class TVertex, class TIndex = unsigned> 
	constexpr GeometryData<TVertex, TIndex> Cone(float height, float radius, unsigned sliceCount, int numLODLevels = 1);




	// --------------------------------------------------------------------------------------------------------------------------------------------
	// TEMPLATE DEFINITIONS
	// --------------------------------------------------------------------------------------------------------------------------------------------
	template<size_t LEN> // Helper function for setting values on a C-style array using an initializer list
	void SetFVec(float* p, const std::initializer_list<float>& values) 
	{
		static_assert(LEN > 0 && LEN <= 4); // assume [vec1-vec4]
		for (size_t i = 0; i < LEN; ++i)
			p[i] = values.begin()[i];
	}


 
	//  Bitangent
	// 
	// ^  
	// |                  V1 (uv1)
	// |                  ^
	// |                 / \ 
	// |                /   \
	// |               /     \
	// |              /       \ 
	// |             /         \
	// |            /           \
	// |           /             \
	// |          /               \
	// |         /                 \
	// |    V0   ___________________ V2 
	// |   (uv0)                    (uv2)
	// ----------------------------------------->  Tangent
	template<class TVertex, class TIndex>
	constexpr GeometryData<TVertex, TIndex> Triangle(float size)
	{
		constexpr bool bHasTangents = std::is_same<TVertex, FVertexWithNormalAndTangent>();
		constexpr bool bHasNormals  = std::is_same<TVertex, FVertexWithNormal>() || std::is_same<TVertex, FVertexWithNormalAndTangent>();
		constexpr bool bHasColor    = std::is_same<TVertex, FVertexWithColor>()  || std::is_same<TVertex, FVertexWithColorAndAlpha>();
		constexpr bool bHasAlpha    = std::is_same<TVertex, FVertexWithColorAndAlpha>();

		constexpr size_t NUM_VERTS = 3;

		GeometryData<TVertex, TIndex> data;
		std::vector<TVertex>& v = data.Vertices;
		v.resize(NUM_VERTS);

		// indices
		data.Indices = { 0u, 1u, 2u };
		
		// position
		SetFVec<3>(v[0].position, { -size, -size, 0.0f });
		SetFVec<3>(v[1].position, {  0.0f,  size, 0.0f });
		SetFVec<3>(v[2].position, {  size, -size, 0.0f });

		// uv
		SetFVec<2>(v[0].uv, { 0.0f, 1.0f });
		SetFVec<2>(v[1].uv, { 0.5f, 0.0f });
		SetFVec<2>(v[2].uv, { 1.0f, 1.0f });

		// normals
		if constexpr (bHasNormals)
		{
			constexpr std::initializer_list<float> NormalVec = { 0, 0, -1 };
			SetFVec<3>(v[0].normal, NormalVec);
			SetFVec<3>(v[1].normal, NormalVec);
			SetFVec<3>(v[2].normal, NormalVec);
		}

		// color
		if constexpr (bHasColor)
		{
			SetFVec<3>(v[0].color, { 1.0f, 0.0f, 0.0f });
			SetFVec<3>(v[1].color, { 0.0f, 1.0f, 0.0f });
			SetFVec<3>(v[2].color, { 0.0f, 0.0f, 1.0f });
			if constexpr (bHasAlpha)
			{
				v[0].color[3] = 1.0f;
				v[1].color[3] = 1.0f;
				v[2].color[3] = 1.0f;
			}
		}

		// tangent
		if constexpr (bHasTangents)
		{
			// todo: CalculateTangents(vector<T>& vertices, const vector<unsigned> indices)
		}

		return data;
	}


	//     ASCII Cube art from: http://www.lonniebest.com/ASCII/Art/?ID=2
	// 
	//             0 _________________________ 1        0, 1, 2, 0, 2, 3,       // Top
	//              / _____________________  /|         4, 5, 6, 4, 6, 7,       // Front
	//             / / ___________________/ / |         8, 9, 10, 8, 10, 11,    // Right
	//            / / /| |               / /  |         12, 13, 14, 12, 14, 15, // Left
	//           / / / | |              / / . |         16, 17, 18, 16, 18, 19, // Back
	//          / / /| | |             / / /| |         20, 22, 21, 20, 23, 22, // Bottom
	//         / / / | | |            / / / | |           
	//        / / /  | | |           / / /| | |          +Y
	//       / /_/__________________/ / / | | |           |  +Z
	//  4,3 /________________________/5/  | | |           |  /
	//      | ______________________8|2|  | | |           | /
	//      | | |    | | |_________| | |__| | |           |/______+X
	//      | | |    | |___________| | |____| |           
	//      | | |   / / ___________| | |_  / /
	//      | | |  / / /           | | |/ / /
	//      | | | / / /            | | | / /
	//      | | |/ / /             | | |/ /
	//      | | | / /              | | ' /
	//      | | |/_/_______________| |  /
	//      | |____________________| | /
	//      |________________________|/6
	//      7
	//
	// vertices - CW 
	template<class TVertex, class TIndex>
	constexpr GeometryData<TVertex, TIndex> Cube()
	{
		constexpr bool bHasTangents = std::is_same<TVertex, FVertexWithNormalAndTangent>();
		constexpr bool bHasNormals  = std::is_same<TVertex, FVertexWithNormal>() || std::is_same<TVertex, FVertexWithNormalAndTangent>();
		constexpr bool bHasColor    = std::is_same<TVertex, FVertexWithColor>()  || std::is_same<TVertex, FVertexWithColorAndAlpha>();
		constexpr bool bHasAlpha    = std::is_same<TVertex, FVertexWithColorAndAlpha>();

		constexpr int NUM_VERTS   = 24;

		GeometryData<TVertex, TIndex> data;
		std::vector<TVertex>& v = data.Vertices;
		v.resize(NUM_VERTS);

		// indices
		data.Indices = {
			0, 1, 2, 0, 2, 3,		// Top
			4, 5, 6, 4, 6, 7,		// back
			8, 9, 10, 8, 10, 11,	// Right
			12, 13, 14, 12, 14, 15, // Back
			16, 17, 18, 16, 18, 19, // Left
			20, 22, 21, 20, 23, 22, // Bottom
		};

		// uv
		SetFVec<2>(v[0] .uv, { +0.0f, +0.0f });    SetFVec<2>(v[3] .uv, { +0.0f, +1.0f });
		SetFVec<2>(v[1] .uv, { +1.0f, +0.0f });    SetFVec<2>(v[4] .uv, { +0.0f, +0.0f });
		SetFVec<2>(v[2] .uv, { +1.0f, +1.0f });    SetFVec<2>(v[5] .uv, { +1.0f, +0.0f });

		SetFVec<2>(v[6] .uv, { +1.0f, +1.0f });    SetFVec<2>(v[9] .uv, { +1.0f, +0.0f });
		SetFVec<2>(v[7] .uv, { +0.0f, +1.0f });    SetFVec<2>(v[10].uv, { +1.0f, +1.0f });
		SetFVec<2>(v[8] .uv, { +0.0f, +0.0f });    SetFVec<2>(v[11].uv, { +0.0f, +1.0f });
		
		SetFVec<2>(v[12].uv, { +0.0f, +0.0f });    SetFVec<2>(v[15].uv, { +0.0f, +1.0f });
		SetFVec<2>(v[13].uv, { +1.0f, +0.0f });    SetFVec<2>(v[16].uv, { +0.0f, +0.0f });
		SetFVec<2>(v[14].uv, { +1.0f, +1.0f });    SetFVec<2>(v[17].uv, { +1.0f, +0.0f });

		SetFVec<2>(v[18].uv, { +1.0f, +1.0f });    SetFVec<2>(v[21].uv, { +0.0f, +0.0f });
		SetFVec<2>(v[19].uv, { +0.0f, +1.0f });    SetFVec<2>(v[22].uv, { +0.0f, +1.0f });
		SetFVec<2>(v[20].uv, { +1.0f, +0.0f });    SetFVec<2>(v[23].uv, { +1.0f, +1.0f });

		// positions / normals / tangents
		/* TOP */                   SetFVec<3>(v[0].position , { -1.0f, +1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[0].normal   , { +0.0f, +1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[0].tangent  , { +1.0f, +0.0f, +0.0f });

		                            SetFVec<3>(v[1].position , { +1.0f, +1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[1].normal   , { +0.0f, +1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[1].tangent  , { +1.0f, +0.0f, +0.0f });

		                            SetFVec<3>(v[2].position , { +1.0f, +1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[2].normal   , { +0.0f, +1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[2].tangent  , { +1.0f, +0.0f, +0.0f });

		                            SetFVec<3>(v[3].position , { -1.0f, +1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[3].normal   , { +0.0f, +1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[3].tangent  , { +1.0f, +0.0f, +0.0f });

		/* FRONT */                 SetFVec<3>(v[4].position , { -1.0f, +1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[4].normal   , { +0.0f, +0.0f, -1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[4].tangent  , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[5].position , { +1.0f, +1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[5].normal   , { +0.0f, +0.0f, -1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[5].tangent  , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[6].position , { +1.0f, -1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[6].normal   , { +0.0f, +0.0f, -1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[6].tangent  , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[7].position , { -1.0f, -1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[7].normal   , { +0.0f, +0.0f, -1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[7].tangent  , { +1.0f, +0.0f, +0.0f });

		/* RIGHT */                 SetFVec<3>(v[8].position , { +1.0f, +1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[8].normal   , { +1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[8].tangent  , { +0.0f, +0.0f, +1.0f });
		                            SetFVec<3>(v[9].position , { +1.0f, +1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[9].normal   , { +1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[9].tangent  , { +0.0f, +0.0f, +1.0f });
		                            SetFVec<3>(v[10].position, { +1.0f, -1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[10].normal  , { +1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[10].tangent , { +0.0f, +0.0f, +1.0f });
		                            SetFVec<3>(v[11].position, { +1.0f, -1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[11].normal  , { +1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[11].tangent , { +0.0f, +0.0f, +1.0f });

		/* BACK */                  SetFVec<3>(v[12].position, { +1.0f, +1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[12].normal  , { +0.0f, +0.0f, +1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[12].tangent , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[13].position, { -1.0f, +1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[13].normal  , { +0.0f, +0.0f, +1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[13].tangent , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[14].position, { -1.0f, -1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[14].normal  , { +0.0f, +0.0f, +1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[14].tangent , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[15].position, { +1.0f, -1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[15].normal  , { +0.0f, +0.0f, +1.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[15].tangent , { +1.0f, +0.0f, +0.0f });

		/* LEFT */                  SetFVec<3>(v[16].position, { -1.0f, +1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[16].normal  , { -1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[16].tangent , { +0.0f, +0.0f, -1.0f });
		                            SetFVec<3>(v[17].position, { -1.0f, +1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[17].normal  , { -1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[17].tangent , { +0.0f, +0.0f, -1.0f });
		                            SetFVec<3>(v[18].position, { -1.0f, -1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[18].normal  , { -1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[18].tangent , { +0.0f, +0.0f, -1.0f });
		                            SetFVec<3>(v[19].position, { -1.0f, -1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[19].normal  , { -1.0f, +0.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[19].tangent , { +0.0f, +0.0f, -1.0f });

		/* BOTTOM */                SetFVec<3>(v[20].position, { +1.0f, -1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[20].normal  , { +0.0f, -1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[20].tangent , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[21].position, { -1.0f, -1.0f, -1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[21].normal  , { +0.0f, -1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[21].tangent , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[22].position, { -1.0f, -1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[22].normal  , { +0.0f, -1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[22].tangent , { +1.0f, +0.0f, +0.0f });
		                            SetFVec<3>(v[23].position, { +1.0f, -1.0f, +1.0f });
		if constexpr (bHasNormals)  SetFVec<3>(v[23].normal  , { +0.0f, -1.0f, +0.0f });
		if constexpr (bHasTangents) SetFVec<3>(v[23].tangent , { +1.0f, +0.0f, +0.0f });

		if constexpr (bHasColor)
		{
			for (size_t i = 0; i < data.Indices.size(); i += 3)
			{
				TIndex Indices[3] =
				{
					  data.Indices[i + 0]
					, data.Indices[i + 1]
					, data.Indices[i + 2]
				};
				SetFVec<3>(v[Indices[0]].color, { 0.1f, 0.1f, 1.0f });
				SetFVec<3>(v[Indices[1]].color, { 0.1f, 0.1f, 1.0f });
				SetFVec<3>(v[Indices[2]].color, { 0.1f, 0.1f, 1.0f });
				if constexpr (bHasAlpha)
				{
					v[Indices[0]].color[3] = 1.0f;
					v[Indices[1]].color[3] = 1.0f;
					v[Indices[2]].color[3] = 1.0f;
				}
			}

		}

		return data;
	}


	template<class TVertex, class TIndex>
	bool ParseObjFile(const std::string& path, GeometryData<TVertex, TIndex>& geo){}

	template<>
	inline bool ParseObjFile<FVertexWithNormal, unsigned int>(const std::string& path , GeometryData<FVertexWithNormal, unsigned int>& geo)
	{
		std::unordered_map<FVertexWithNormal, unsigned int> indexMap;
	
		struct VertexIndices
		{
			WORD posIndex;
			WORD texIndex;
			WORD norIndex;
		};
	
		std::ifstream fileStream(path);
		if (!fileStream)
		{
			Log::Error("Failed to open file path : %s");
			return false;
		}
			 
		std::string line;
		std::istringstream ss;
		std::vector<XMFLOAT3> positions = {  };
		std::vector<XMFLOAT3> normals = {  };
		std::vector<XMFLOAT2> texCoords = {  };
		std::vector<std::vector<VertexIndices>> meshPolygons;
	
	
		while (std::getline(fileStream, line))
		{
			if (line._Starts_with("v "))
			{
				float x, y, z;
	
				ss.clear();
				ss.str(line);
				ss.ignore(2);
				ss >> x >> y >> z;
	
				positions.emplace_back(XMFLOAT3{ x,y,z });
				continue;
			}
			else if (line._Starts_with("vt"))
			{
				float x, y;
	
				ss.clear();
				ss.str(line);
				ss.ignore(2);
				ss >> x >> y;
	
				y = y - 2 * (y - 0.5f);
	
				texCoords.emplace_back(XMFLOAT2{ x,y });
				continue;
			}
			else if (line._Starts_with("vn"))
			{
				float x, y, z;
	
				ss.clear();
				ss.str(line);
				ss.ignore(2);
				ss >> x >> y >> z;
	
				normals.emplace_back(XMFLOAT3{ x,y,z });
				continue;
			}
			else if (line._Starts_with("f "))
			{
				ss.clear();
				ss.str(line);
				ss.ignore(1);
	
				meshPolygons.emplace_back(std::vector<VertexIndices>{});
	
				while (static_cast<size_t>(ss.tellg()) <= line.size())
				{
					VertexIndices indices;
	
					if (ss.peek() == '/')
					{
						indices.posIndex = 0;
					}
					else
					{
						ss >> indices.posIndex;
						indices.posIndex -= 1;
					}
					if (ss.peek() == '/')
					{
						ss.ignore(1);
						if (ss.peek() == '/')
						{
							ss.ignore(1);
							indices.texIndex = 0;
							ss >> indices.norIndex;
							indices.norIndex -= 1;
							meshPolygons.back().emplace_back(indices);
							continue;
						}
						ss >> indices.texIndex;
						indices.texIndex -= 1;
					}
					else
					{
						indices.texIndex = 0;
						indices.norIndex = 0;
						meshPolygons.back().emplace_back(indices);
						continue;
					}
					if (ss.peek() == '/')
					{
						ss.ignore(1);
						ss >> indices.norIndex;
						indices.norIndex -= 1;
						meshPolygons.back().emplace_back(indices);
					}
					else
					{
						indices.norIndex = 0;
						meshPolygons.back().emplace_back(indices);
					}
					ss >> std::ws;
				}
	
			}
		}
	
		//for now we preallocate our vertices with size of our positions
		//which at leats is at that size but can be larger
		//I don't any other way to estimate it's final size
		geo.Vertices.reserve(positions.size());
		indexMap.reserve(geo.Vertices.size());
		indexMap.max_load_factor(2.0f);
	
		for (auto& polygon : meshPolygons)
		{
			std::vector<unsigned int> faceIDX;
			faceIDX.reserve(polygon.size());
	
			for (auto& vertexIDX : polygon)
			{
				auto vertex = FVertexWithNormal{ {positions[vertexIDX.posIndex].x , positions[vertexIDX.posIndex].y ,positions[vertexIDX.posIndex].z},
												 {normals[vertexIDX.norIndex].x   , normals[vertexIDX.norIndex].y   , normals[vertexIDX.norIndex].z },
												 {texCoords[vertexIDX.texIndex].x , texCoords[vertexIDX.texIndex].y} };
	
				if (indexMap.find(vertex) == indexMap.end())
				{
					geo.Vertices.emplace_back(vertex);
	
					indexMap[vertex] = geo.Vertices.size() - 1;
	
					faceIDX.emplace_back(geo.Vertices.size() - 1);
				}
				else
				{
					faceIDX.emplace_back(indexMap[vertex]);
				}
			}
	
			for (int i = 2; i < faceIDX.size(); i++)
			{
				geo.Indices.emplace_back(faceIDX[0]);
				geo.Indices.emplace_back(faceIDX[i - 1]);
				geo.Indices.emplace_back(faceIDX[i]);
			}
		}

		return true;
	}
	
	template<class TVertex, class TIndex>
	bool LoadObjFromFile(const std::string& path , GeometryData<TVertex, TIndex>& data);
	
	template<>
	inline bool LoadObjFromFile<FVertexWithNormal , unsigned int>(const std::string& path, GeometryData<FVertexWithNormal, unsigned int>& data)
	{
		return ParseObjFile(path, data);
	}
};
