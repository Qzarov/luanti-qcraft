// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include "constants.h"
#include "mapnode.h"
#include "debug.h"

/*
	A carved node is subdivided into N^3 sub-cubes, each holding its own
	material. N is fixed for the lifetime of a world.

	Storage is grouped into tiles of MICRO_TILE_AXIS^3 nodes. A tile is
	allocated whole and always has the same size, so the cost of an edit does
	not depend on how much of the mapblock is already carved. A tile is a cube
	rather than a run of consecutive node indices because carving clusters in
	three dimensions, while a run would be a 16x4x1 slab that only suits walls
	in one plane.
*/

//! Nodes along one axis of a tile.
static constexpr u16 MICRO_TILE_AXIS = 4;
//! Nodes in one tile.
static constexpr u16 MICRO_TILE_NODES =
		MICRO_TILE_AXIS * MICRO_TILE_AXIS * MICRO_TILE_AXIS;
//! Tiles in one mapblock.
static constexpr u16 MICRO_TILES_PER_BLOCK =
		(MAP_BLOCKSIZE / MICRO_TILE_AXIS) * (MAP_BLOCKSIZE / MICRO_TILE_AXIS) *
		(MAP_BLOCKSIZE / MICRO_TILE_AXIS);
//! Directory entry meaning "this tile was never allocated".
static constexpr u16 MICRO_NO_TILE = 0xFFFF;
//! Palette slots addressable by a 4-bit sub-cube index.
static constexpr u8 MICRO_PALETTE_SLOTS = 16;
//! Bytes reserved at the front of a tile for its palette and bookkeeping.
static constexpr u16 MICRO_TILE_HEADER = 64;

static_assert(MAP_BLOCKSIZE % MICRO_TILE_AXIS == 0,
		"a mapblock must divide evenly into tiles");

struct MicroGeometry
{
	//! Sub-cubes along one axis of a node. 2 or 4.
	u8 n = 4;
	//! Sub-cubes in one node, n^3.
	u16 sub_count = 64;
	//! Bytes of sub-cube indices per node, 4 bits each.
	u16 bytes_per_node = 32;
};

bool microSubdivisionSupported(u8 n);

/*!
 * Geometry for a subdivision factor.
 * \throws std::invalid_argument if n is not supported.
 */
MicroGeometry microGeometryFor(u8 n);

// microTileSlot()/microNodeInTile() below hardcode a shift of 2 and a mask of
// 3 for each axis, which is only correct for MICRO_TILE_AXIS == 4. Tie that
// assumption to the constant so it cannot silently drift.
static_assert(MICRO_TILE_AXIS == 4,
		"microTileSlot/microNodeInTile shifts and masks assume an axis of 4");

//! Which tile of a mapblock a node belongs to. relp is block-relative, 0..15.
inline u16 microTileSlot(v3s16 relp)
{
	return (u16)(((relp.Z >> 2) << 4) | ((relp.Y >> 2) << 2) | (relp.X >> 2));
}

//! Which node inside its tile. relp is block-relative, 0..15.
inline u16 microNodeInTile(v3s16 relp)
{
	return (u16)(((relp.Z & 3) << 4) | ((relp.Y & 3) << 2) | (relp.X & 3));
}

//! Which sub-cube inside a node. sub is 0..n-1 on each axis.
inline u16 microSubIndex(const MicroGeometry &g, v3s16 sub)
{
	return (u16)((sub.Z * g.n + sub.Y) * g.n + sub.X);
}

#include <vector>

/*
	One contiguous allocation handed out as fixed-size tiles. Carving never
	calls the allocator: the storage and the freelist are sized once, at world
	load, when N is known.
*/
class MicroTilePool
{
public:
	//! tile_count is clamped to MICRO_NO_TILE - 1, since indices are u16.
	MicroTilePool(MicroGeometry geom, u32 tile_count);

	//! Returns MICRO_NO_TILE when the pool is full. The tile is zeroed and its
	//! palette holds CONTENT_AIR in slot 0.
	u16 allocate();
	//! Harmless no-op if tile is MICRO_NO_TILE, out of range, or not currently
	//! allocated (double release, or a tile that was never allocated). Only a
	//! tile that is actually in use is returned to the freelist, so a bad
	//! release can never hand the same tile to two owners and can never grow
	//! the freelist past its constructor-time capacity.
	void release(u16 tile);

	content_t *palette(u16 tile);
	const content_t *palette(u16 tile) const;
	u8 *nibbles(u16 tile);
	const u8 *nibbles(u16 tile) const;

	//! Used palette slots of a tile, at least 1.
	u8 &paletteUsed(u16 tile);
	u8 paletteUsed(u16 tile) const;

	u32 capacity() const { return m_capacity; }
	u32 used() const { return m_used; }
	u32 tileStride() const { return m_stride; }
	const MicroGeometry &geometry() const { return m_geom; }

private:
	//! Bounds-checks tile with a check that survives a Release build (unlike
	//! assert()), since an out-of-range index would otherwise read or write
	//! past m_storage with no crash.
	u8 *tileAt(u16 tile);
	const u8 *tileAt(u16 tile) const;
	void resetTile(u16 tile);

	MicroGeometry m_geom;
	u32 m_capacity;
	u32 m_stride;
	u32 m_used = 0;
	std::vector<u8> m_storage;
	std::vector<u16> m_free;
	//! One flag per tile: whether it is currently handed out. Sized once in
	//! the constructor alongside m_storage and m_free.
	std::vector<bool> m_in_use;
};

/*
	The carved nodes of one mapblock. The directory is either empty, meaning
	nothing in this mapblock was ever carved, or holds one entry per tile.
	It is allocated once and never grows.
*/
class MicroLayer
{
public:
	MicroLayer() = default;
	// Copying would duplicate the raw pool indices held in m_directory
	// without telling the pool, so releasing either copy's tiles (or letting
	// both be destroyed) would release the same tile twice. Moves are fine:
	// they transfer ownership, leaving the source empty.
	MicroLayer(const MicroLayer &) = delete;
	MicroLayer &operator=(const MicroLayer &) = delete;
	MicroLayer(MicroLayer &&) = default;
	MicroLayer &operator=(MicroLayer &&) = default;

	bool empty() const { return m_directory.empty(); }

	//! MICRO_NO_TILE when the node's tile was never allocated.
	u16 tileAt(v3s16 relp) const;

	//! CONTENT_IGNORE when the node is not carved.
	content_t getSub(const MicroTilePool &pool, v3s16 relp, u16 sub) const;

	//! False when the pool is exhausted or the tile palette is full. On false
	//! nothing is written.
	bool setSub(MicroTilePool &pool, v3s16 relp, u16 sub, content_t c);

	void clear(MicroTilePool &pool);

	//! Sizes the (empty) directory to one MICRO_NO_TILE entry per tile, for
	//! deSerialize to fill in via adoptTile as it reads allocated slots.
	void resetDirectory()
	{
		m_directory.assign(MICRO_TILES_PER_BLOCK, MICRO_NO_TILE);
	}

	//! Records a tile the caller (deSerialize) already allocated from the
	//! pool into directory slot `slot`. Unlike writing into directory()
	//! directly, this cannot be used to silently overwrite a live slot and
	//! leak whatever tile was there: slot must currently be MICRO_NO_TILE.
	void adoptTile(u16 slot, u16 tile)
	{
		sanity_check(slot < m_directory.size());
		sanity_check(m_directory[slot] == MICRO_NO_TILE);
		m_directory[slot] = tile;
	}

	const std::vector<u16> &directory() const { return m_directory; }

private:
	std::vector<u16> m_directory;
};
