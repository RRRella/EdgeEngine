#include "Mesh.h"
#include <cassert>

#define VERBOSE_LOGGING 0
#if VERBOSE_LOGGING
#include "Utilities/Log.h"
#endif


std::pair<BufferID, BufferID> Mesh::GetIABuffers(int lod /*= 0*/) const
{
	assert(mLODBufferPairs.size() > 0); // maybe no assert and return <-1, -1> ?

	if (lod < mLODBufferPairs.size())
	{
		return mLODBufferPairs[lod].GetIABufferPair();
	}

#if VERBOSE_LOGGING
	Log::Warning("Requested LOD level (%d) doesn't exist (LOD levels = %d). Returning LOD=0", lod, static_cast<int>(mLODBufferPairs.size()));
#endif

	return mLODBufferPairs.back().GetIABufferPair();
}

