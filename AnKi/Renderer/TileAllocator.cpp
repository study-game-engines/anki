// Copyright (C) 2009-2023, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/TileAllocator.h>

namespace anki {

class TileAllocator::Tile
{
public:
	Timestamp m_lightTimestamp = 0; ///< The last timestamp the light got updated.
	Timestamp m_lastUsedTimestamp = 0; ///< The last timestamp this tile was used.
	U64 m_lightUuid = 0;
	U32 m_lightDrawcallCount = 0;
	Array<U32, 4> m_viewport = {};
	Array<U32, 4> m_subTiles = {kMaxU32, kMaxU32, kMaxU32, kMaxU32};
	U32 m_superTile = kMaxU32; ///< The parent.
	U8 m_lightHierarchy = 0;
	U8 m_lightFace = 0;
};

class TileAllocator::HashMapKey
{
public:
	U64 m_lightUuid;
	U64 m_face;

	U64 computeHash() const
	{
		return anki::computeHash(this, sizeof(*this), 693);
	}
};

TileAllocator::TileAllocator()
{
}

TileAllocator::~TileAllocator()
{
}

void TileAllocator::init(U32 tileCountX, U32 tileCountY, U32 hierarchyCount, Bool enableCaching)
{
	// Preconditions
	ANKI_ASSERT(tileCountX > 0);
	ANKI_ASSERT(tileCountY > 0);
	ANKI_ASSERT(hierarchyCount > 0);

	// Store some stuff
	m_tileCountX = U16(tileCountX);
	m_tileCountY = U16(tileCountY);
	m_hierarchyCount = U8(hierarchyCount);
	m_cachingEnabled = enableCaching;
	m_firstTileIdxOfHierarchy.create(hierarchyCount + 1);

	// Create the tile array & index ranges
	U32 tileCount = 0;
	for(U32 hierarchy = 0; hierarchy < hierarchyCount; ++hierarchy)
	{
		const U32 hierarchyTileCountX = tileCountX >> hierarchy;
		const U32 hierarchyTileCountY = tileCountY >> hierarchy;
		ANKI_ASSERT((hierarchyTileCountX << hierarchy) == tileCountX
					&& "Every hierarchy should be power of 2 of its parent hierarchy");
		ANKI_ASSERT((hierarchyTileCountY << hierarchy) == tileCountY
					&& "Every hierarchy should be power of 2 of its parent hierarchy");

		m_firstTileIdxOfHierarchy[hierarchy] = tileCount;

		tileCount += hierarchyTileCountX * hierarchyTileCountY;
	}
	ANKI_ASSERT(tileCount >= tileCountX * tileCountY);
	m_allTiles.create(tileCount);
	m_firstTileIdxOfHierarchy[hierarchyCount] = tileCount - 1;

	// Init the tiles
	U32 tileIdx = 0;
	for(U32 hierarchy = 0; hierarchy < hierarchyCount; ++hierarchy)
	{
		const U32 hierarchyTileCountX = tileCountX >> hierarchy;
		const U32 hierarchyTileCountY = tileCountY >> hierarchy;

		for(U32 y = 0; y < hierarchyTileCountY; ++y)
		{
			for(U32 x = 0; x < hierarchyTileCountX; ++x)
			{
				ANKI_ASSERT(tileIdx >= m_firstTileIdxOfHierarchy[hierarchy]
							&& tileIdx <= m_firstTileIdxOfHierarchy[hierarchy + 1]);
				Tile& tile = m_allTiles[tileIdx];

				tile.m_viewport[0] = x << hierarchy;
				tile.m_viewport[1] = y << hierarchy;
				tile.m_viewport[2] = 1 << hierarchy;
				tile.m_viewport[3] = 1 << hierarchy;

				if(hierarchy > 0)
				{
					// Has sub tiles
					for(U32 j = 0; j < 2; ++j)
					{
						for(U32 i = 0; i < 2; ++i)
						{
							const U32 subTileIdx = translateTileIdx((x << 1) + i, (y << 1) + j, hierarchy - 1);
							m_allTiles[subTileIdx].m_superTile = tileIdx;

							tile.m_subTiles[j * 2 + i] = subTileIdx;
						}
					}
				}
				else
				{
					// No sub-tiles
				}

				++tileIdx;
			}
		}
	}
}

void TileAllocator::updateSubTiles(const Tile& updateFrom)
{
	if(updateFrom.m_subTiles[0] == kMaxU32)
	{
		return;
	}

	for(U32 idx : updateFrom.m_subTiles)
	{
		m_allTiles[idx].m_lightTimestamp = updateFrom.m_lightTimestamp;
		m_allTiles[idx].m_lastUsedTimestamp = updateFrom.m_lastUsedTimestamp;
		m_allTiles[idx].m_lightUuid = updateFrom.m_lightUuid;
		m_allTiles[idx].m_lightDrawcallCount = updateFrom.m_lightDrawcallCount;
		m_allTiles[idx].m_lightHierarchy = updateFrom.m_lightHierarchy;
		m_allTiles[idx].m_lightFace = updateFrom.m_lightFace;

		updateSubTiles(m_allTiles[idx]);
	}
}

void TileAllocator::updateSuperTiles(const Tile& updateFrom)
{
	if(updateFrom.m_superTile != kMaxU32)
	{
		m_allTiles[updateFrom.m_superTile].m_lightUuid = 0;
		m_allTiles[updateFrom.m_superTile].m_lastUsedTimestamp = updateFrom.m_lastUsedTimestamp;
		updateSuperTiles(m_allTiles[updateFrom.m_superTile]);
	}
}

Bool TileAllocator::searchTileRecursively(U32 crntTileIdx, U32 crntTileHierarchy, U32 allocationHierarchy,
										  Timestamp crntTimestamp, U32& emptyTileIdx, U32& toKickTileIdx,
										  Timestamp& tileToKickMinTimestamp) const
{
	const Tile& tile = m_allTiles[crntTileIdx];

	if(crntTileHierarchy == allocationHierarchy)
	{
		// We may have a candidate

		const Bool done =
			evaluateCandidate(crntTileIdx, crntTimestamp, emptyTileIdx, toKickTileIdx, tileToKickMinTimestamp);

		if(done)
		{
			return true;
		}
	}
	else if(tile.m_subTiles[0] != kMaxU32)
	{
		// Move down the hierarchy

		ANKI_ASSERT(allocationHierarchy < crntTileHierarchy);

		for(const U32 idx : tile.m_subTiles)
		{
			const Bool done = searchTileRecursively(idx, crntTileHierarchy - 1, allocationHierarchy, crntTimestamp,
													emptyTileIdx, toKickTileIdx, tileToKickMinTimestamp);

			if(done)
			{
				return true;
			}
		}
	}

	return false;
}

Bool TileAllocator::evaluateCandidate(U32 tileIdx, Timestamp crntTimestamp, U32& emptyTileIdx, U32& toKickTileIdx,
									  Timestamp& tileToKickMinTimestamp) const
{
	const Tile& tile = m_allTiles[tileIdx];

	if(m_cachingEnabled)
	{
		if(tile.m_lastUsedTimestamp == 0)
		{
			// Found empty
			emptyTileIdx = tileIdx;
			return true;
		}
		else if(tile.m_lastUsedTimestamp != crntTimestamp && tile.m_lastUsedTimestamp < tileToKickMinTimestamp)
		{
			// Found one with low timestamp
			toKickTileIdx = tileIdx;
			tileToKickMinTimestamp = tile.m_lightTimestamp;
		}
	}
	else
	{
		if(tile.m_lastUsedTimestamp != crntTimestamp)
		{
			emptyTileIdx = tileIdx;
			return true;
		}
	}

	return false;
}

TileAllocatorResult TileAllocator::allocate(Timestamp crntTimestamp, Timestamp lightTimestamp, U64 lightUuid,
											U32 lightFace, U32 drawcallCount, U32 hierarchy,
											Array<U32, 4>& tileViewport)
{
	// Preconditions
	ANKI_ASSERT(crntTimestamp > 0);
	ANKI_ASSERT(lightTimestamp > 0);
	ANKI_ASSERT(lightTimestamp <= crntTimestamp);
	ANKI_ASSERT(lightUuid != 0);
	ANKI_ASSERT(lightFace < 6);
	ANKI_ASSERT(hierarchy < m_hierarchyCount);

	// 1) Search if it's already cached
	HashMapKey key;
	if(m_cachingEnabled)
	{
		key.m_lightUuid = lightUuid;
		key.m_face = lightFace;
		auto it = m_lightInfoToTileIdx.find(key);
		if(it != m_lightInfoToTileIdx.getEnd())
		{
			Tile& tile = m_allTiles[*it];

			if(tile.m_lightUuid != lightUuid || tile.m_lightHierarchy != hierarchy || tile.m_lightFace != lightFace)
			{
				// Cache entry is wrong, remove it
				m_lightInfoToTileIdx.erase(it);
			}
			else
			{
				// Same light & hierarchy & face, found the cache entry.

				ANKI_ASSERT(tile.m_lastUsedTimestamp != crntTimestamp
							&& "Trying to allocate the same thing twice in this timestamp?");

				ANKI_ASSERT(tile.m_lightUuid == lightUuid && tile.m_lightHierarchy == hierarchy
							&& tile.m_lightFace == lightFace);

				tileViewport = {tile.m_viewport[0], tile.m_viewport[1], tile.m_viewport[2], tile.m_viewport[3]};

				const Bool needsReRendering =
					tile.m_lightDrawcallCount != drawcallCount || tile.m_lightTimestamp < lightTimestamp;

				tile.m_lightTimestamp = lightTimestamp;
				tile.m_lastUsedTimestamp = crntTimestamp;
				tile.m_lightDrawcallCount = drawcallCount;

				updateTileHierarchy(tile);

				return (needsReRendering) ? TileAllocatorResult::kAllocationSucceded : TileAllocatorResult::kCached;
			}
		}
	}

	// Start searching for a suitable tile. Do a hieratchical search to end up with better locality and not better
	// utilization of the atlas' space
	U32 emptyTileIdx = kMaxU32;
	U32 toKickTileIdx = kMaxU32;
	Timestamp tileToKickMinTimestamp = kMaxTimestamp;
	const U32 maxHierarchy = m_hierarchyCount - 1;
	if(hierarchy == maxHierarchy)
	{
		// This search is simple, iterate the tiles of the max hierarchy

		for(U32 tileIdx = m_firstTileIdxOfHierarchy[maxHierarchy];
			tileIdx <= m_firstTileIdxOfHierarchy[maxHierarchy + 1]; ++tileIdx)
		{
			const Bool done =
				evaluateCandidate(tileIdx, crntTimestamp, emptyTileIdx, toKickTileIdx, tileToKickMinTimestamp);

			if(done)
			{
				break;
			}
		}
	}
	else
	{
		// Need to do a recursive search

		for(U32 tileIdx = m_firstTileIdxOfHierarchy[maxHierarchy];
			tileIdx <= m_firstTileIdxOfHierarchy[maxHierarchy + 1]; ++tileIdx)
		{
			const Bool done = searchTileRecursively(tileIdx, maxHierarchy, hierarchy, crntTimestamp, emptyTileIdx,
													toKickTileIdx, tileToKickMinTimestamp);

			if(done)
			{
				break;
			}
		}
	}

	U32 allocatedTileIdx;
	if(emptyTileIdx != kMaxU32)
	{
		allocatedTileIdx = emptyTileIdx;
	}
	else if(toKickTileIdx != kMaxU32)
	{
		allocatedTileIdx = toKickTileIdx;
	}
	else
	{
		// Out of tiles
		return TileAllocatorResult::kAllocationFailed;
	}

	// Allocation succedded, need to do some bookkeeping

	// Mark the allocated tile
	Tile& allocatedTile = m_allTiles[allocatedTileIdx];
	allocatedTile.m_lightTimestamp = lightTimestamp;
	allocatedTile.m_lastUsedTimestamp = crntTimestamp;
	allocatedTile.m_lightUuid = lightUuid;
	allocatedTile.m_lightDrawcallCount = drawcallCount;
	allocatedTile.m_lightHierarchy = U8(hierarchy);
	allocatedTile.m_lightFace = U8(lightFace);

	updateTileHierarchy(allocatedTile);

	// Update the cache
	if(m_cachingEnabled)
	{
		m_lightInfoToTileIdx.emplace(key, allocatedTileIdx);
	}

	// Return
	tileViewport = {allocatedTile.m_viewport[0], allocatedTile.m_viewport[1], allocatedTile.m_viewport[2],
					allocatedTile.m_viewport[3]};

	return TileAllocatorResult::kAllocationSucceded;
}

void TileAllocator::invalidateCache(U64 lightUuid, U32 lightFace)
{
	ANKI_ASSERT(m_cachingEnabled);
	ANKI_ASSERT(lightUuid > 0);

	HashMapKey key;
	key.m_lightUuid = lightUuid;
	key.m_face = lightFace;

	auto it = m_lightInfoToTileIdx.find(key);
	if(it != m_lightInfoToTileIdx.getEnd())
	{
		m_lightInfoToTileIdx.erase(it);
	}
}

} // end namespace anki
