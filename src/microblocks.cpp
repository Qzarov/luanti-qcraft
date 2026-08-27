// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "microblocks.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "debug.h"

bool microSubdivisionSupported(u8 n)
{
	return n == 2 || n == 4;
}

MicroGeometry microGeometryFor(u8 n)
{
	if (!microSubdivisionSupported(n))
		throw std::invalid_argument("unsupported microblock subdivision");

	MicroGeometry g;
	g.n = n;
	g.sub_count = (u16)(n * n * n);
	g.bytes_per_node = (u16)(g.sub_count / 2);
	return g;
}

MicroTilePool::MicroTilePool(MicroGeometry geom, u32 tile_count) :
		m_geom(geom)
{
	m_capacity = std::min<u32>(tile_count, MICRO_NO_TILE - 1);
	m_stride = MICRO_TILE_HEADER + (u32)MICRO_TILE_NODES * m_geom.bytes_per_node;
	m_storage.assign((size_t)m_capacity * m_stride, 0);

	m_free.reserve(m_capacity);
	// Reverse order so that the first allocation returns tile 0.
	for (u32 i = m_capacity; i > 0; i--)
		m_free.push_back((u16)(i - 1));

	// One flag per tile, sized once here so allocate()/release() never touch
	// the allocator.
	m_in_use.assign(m_capacity, false);
}

u8 *MicroTilePool::tileAt(u16 tile)
{
	// sanity_check (unlike assert) persists in Release builds, where NDEBUG
	// would otherwise strip this and let an out-of-range index read or write
	// past m_storage through the reinterpret_cast in palette()/nibbles().
	sanity_check(tile < m_capacity);
	return m_storage.data() + (size_t)tile * m_stride;
}

const u8 *MicroTilePool::tileAt(u16 tile) const
{
	sanity_check(tile < m_capacity);
	return m_storage.data() + (size_t)tile * m_stride;
}

void MicroTilePool::resetTile(u16 tile)
{
	std::memset(tileAt(tile), 0, m_stride);
	// CONTENT_AIR is 126, so an all-zero palette would not mean air.
	palette(tile)[0] = CONTENT_AIR;
	paletteUsed(tile) = 1;
}

u16 MicroTilePool::allocate()
{
	if (m_free.empty())
		return MICRO_NO_TILE;

	const u16 tile = m_free.back();
	m_free.pop_back();
	m_used++;
	m_in_use[tile] = true;
	resetTile(tile);
	return tile;
}

void MicroTilePool::release(u16 tile)
{
	if (tile == MICRO_NO_TILE)
		return;
	// Bounds-check with sanity_check, not assert: this must still catch a bad
	// caller-supplied index in a Release build, before it ever reaches
	// m_in_use or m_free.
	sanity_check(tile < m_capacity);
	if (!m_in_use[tile]) {
		// Double release, or a tile that was never allocated: harmless no-op.
		// Without this guard the same index would be pushed onto m_free
		// twice, so the next two allocate() calls would hand the same tile
		// to two different owners, and repeated bad releases would grow
		// m_free past its constructor-time reserve(), forcing a heap
		// allocation during carving.
		return;
	}
	m_in_use[tile] = false;
	m_used--;
	m_free.push_back(tile);
}

content_t *MicroTilePool::palette(u16 tile)
{
	return reinterpret_cast<content_t *>(tileAt(tile));
}

const content_t *MicroTilePool::palette(u16 tile) const
{
	return reinterpret_cast<const content_t *>(tileAt(tile));
}

u8 &MicroTilePool::paletteUsed(u16 tile)
{
	return tileAt(tile)[MICRO_PALETTE_SLOTS * sizeof(content_t)];
}

u8 MicroTilePool::paletteUsed(u16 tile) const
{
	return tileAt(tile)[MICRO_PALETTE_SLOTS * sizeof(content_t)];
}

u8 *MicroTilePool::nibbles(u16 tile)
{
	return tileAt(tile) + MICRO_TILE_HEADER;
}

const u8 *MicroTilePool::nibbles(u16 tile) const
{
	return tileAt(tile) + MICRO_TILE_HEADER;
}

namespace
{
	//! Position of a sub-cube inside a tile's nibble stream.
	inline u32 nibbleIndex(const MicroGeometry &g, u16 node_in_tile, u16 sub)
	{
		return (u32)node_in_tile * g.sub_count + sub;
	}

	inline u8 readNibble(const u8 *bytes, u32 index)
	{
		const u8 byte = bytes[index >> 1];
		return (index & 1) ? (u8)(byte >> 4) : (u8)(byte & 0x0F);
	}

	inline void writeNibble(u8 *bytes, u32 index, u8 value)
	{
		u8 &byte = bytes[index >> 1];
		if (index & 1)
			byte = (u8)((byte & 0x0F) | (value << 4));
		else
			byte = (u8)((byte & 0xF0) | (value & 0x0F));
	}
}

u16 MicroLayer::tileAt(v3s16 relp) const
{
	if (m_directory.empty())
		return MICRO_NO_TILE;
	return m_directory[microTileSlot(relp)];
}

content_t MicroLayer::getSub(const MicroTilePool &pool, v3s16 relp, u16 sub) const
{
	const u16 tile = tileAt(relp);
	if (tile == MICRO_NO_TILE)
		return CONTENT_IGNORE;

	const MicroGeometry &g = pool.geometry();
	const u32 index = nibbleIndex(g, microNodeInTile(relp), sub);
	const u8 slot = readNibble(pool.nibbles(tile), index);
	return pool.palette(tile)[slot];
}

bool MicroLayer::setSub(MicroTilePool &pool, v3s16 relp, u16 sub, content_t c)
{
	const u16 slot_index = microTileSlot(relp);

	if (m_directory.empty())
		m_directory.assign(MICRO_TILES_PER_BLOCK, MICRO_NO_TILE);

	u16 tile = m_directory[slot_index];
	bool allocated_here = false;
	if (tile == MICRO_NO_TILE) {
		tile = pool.allocate();
		if (tile == MICRO_NO_TILE)
			return false;
		allocated_here = true;
	}

	// Find the material in the palette, or add it.
	content_t *palette = pool.palette(tile);
	u8 &used = pool.paletteUsed(tile);
	u8 found = MICRO_PALETTE_SLOTS;
	for (u8 i = 0; i < used; i++) {
		if (palette[i] == c) {
			found = i;
			break;
		}
	}
	if (found == MICRO_PALETTE_SLOTS) {
		if (used >= MICRO_PALETTE_SLOTS) {
			// Roll back a tile allocated only for this refused write.
			if (allocated_here)
				pool.release(tile);
			return false;
		}
		found = used;
		palette[used] = c;
		used++;
	}

	const MicroGeometry &g = pool.geometry();
	writeNibble(pool.nibbles(tile), nibbleIndex(g, microNodeInTile(relp), sub), found);
	m_directory[slot_index] = tile;
	return true;
}

void MicroLayer::clear(MicroTilePool &pool)
{
	for (u16 tile : m_directory)
		pool.release(tile);
	m_directory.clear();
}
