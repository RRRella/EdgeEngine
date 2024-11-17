#pragma once

#include "Mesh.h"
#include <fstream>
#include <sstream>
#include <DirectXMath.h>
using namespace DirectX;

#include "tiny_obj_loader.h"

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
		tinyobj::ObjReaderConfig reader_config;

		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile(path, reader_config))
		{
			if (!reader.Error().empty())
				Log::Error(reader.Error());
			return false;
		}

		if (!reader.Warning().empty())
			Log::Warning(reader.Warning());

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		auto& materials = reader.GetMaterials();

		geo.Vertices.reserve(attrib.vertices.size());

		// Loop over shapes
		for (size_t s = 0; s < shapes.size(); s++) 
		{
			// Loop over faces(polygon)
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
				size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

				std::vector<unsigned int> faceIDX;

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) 
				{
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

					tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

					tinyobj::real_t nx = 0;
					tinyobj::real_t ny = 0;
					tinyobj::real_t nz = 0;

					tinyobj::real_t tx = -1;
					tinyobj::real_t ty = -1;

					// Check if `normal_index` is zero or positive. negative = no normal data
					if (idx.normal_index >= 0)
					{
						nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
					}

					// Check if `texcoord_index` is zero or positive. negative = no texcoord data
					if (idx.texcoord_index >= 0) 
					{
						tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

						ty = 0.5f - (ty - 0.5f);
					}

					assert(tx >= 0);

					geo.Vertices.emplace_back(FVertexWithNormal{{vx , vy , vz}, {nx, ny, nz} ,{tx,ty}});

					faceIDX.emplace_back(geo.Vertices.size() - 1);
				}

				for (int i = 2; i < faceIDX.size(); i++)
				{
					geo.Indices.emplace_back(faceIDX[0]);
					geo.Indices.emplace_back(faceIDX[i - 1]);
					geo.Indices.emplace_back(faceIDX[i]);
				}

				index_offset += fv;
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
