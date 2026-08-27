// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "microblocks.h"

class TestMicroblocks : public TestBase
{
public:
	TestMicroblocks() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMicroblocks"; }

	void runTests(IGameDef *gamedef);

	void testGeometry();
	void testTileSlot();
	void testNodeInTile();
	void testSubIndex();
	void testPool();
	void testPoolExhaustion();
	void testPoolDoubleRelease();
	void testPoolReleaseNeverAllocated();
	void testLayer();
	void testLayerPaletteFull();
	void testLayerPoolExhausted();
};

static TestMicroblocks g_test_instance;

void TestMicroblocks::runTests(IGameDef *gamedef)
{
	TEST(testGeometry);
	TEST(testTileSlot);
	TEST(testNodeInTile);
	TEST(testSubIndex);
	TEST(testPool);
	TEST(testPoolExhaustion);
	TEST(testPoolDoubleRelease);
	TEST(testPoolReleaseNeverAllocated);
	TEST(testLayer);
	TEST(testLayerPaletteFull);
	TEST(testLayerPoolExhausted);
}

void TestMicroblocks::testGeometry()
{
	UASSERT(microSubdivisionSupported(2));
	UASSERT(microSubdivisionSupported(4));
	UASSERT(!microSubdivisionSupported(0));
	UASSERT(!microSubdivisionSupported(3));
	UASSERT(!microSubdivisionSupported(8));

	const MicroGeometry g4 = microGeometryFor(4);
	UASSERTEQ(int, g4.n, 4);
	UASSERTEQ(int, g4.sub_count, 64);
	UASSERTEQ(int, g4.bytes_per_node, 32);

	const MicroGeometry g2 = microGeometryFor(2);
	UASSERTEQ(int, g2.n, 2);
	UASSERTEQ(int, g2.sub_count, 8);
	UASSERTEQ(int, g2.bytes_per_node, 4);
}

void TestMicroblocks::testTileSlot()
{
	// A tile is a 4x4x4 cube of nodes, so the four nodes below share one tile.
	UASSERTEQ(int, microTileSlot(v3s16(0, 0, 0)), 0);
	UASSERTEQ(int, microTileSlot(v3s16(3, 3, 3)), 0);
	// Crossing four nodes on X moves to the next slot.
	UASSERTEQ(int, microTileSlot(v3s16(4, 0, 0)), 1);
	UASSERTEQ(int, microTileSlot(v3s16(0, 4, 0)), 4);
	UASSERTEQ(int, microTileSlot(v3s16(0, 0, 4)), 16);
	UASSERTEQ(int, microTileSlot(v3s16(15, 15, 15)), MICRO_TILES_PER_BLOCK - 1);

	// Regression guard: a tile slot is not "node index >> 6", which would group
	// 64 consecutive indices into a 16x4x1 slab instead of a cube. At (0,4,0)
	// the cube gives slot 4 while the shift gives 1.
	UASSERTEQ(int, microTileSlot(v3s16(0, 4, 0)), 4);
	UASSERT(microTileSlot(v3s16(0, 4, 0)) != ((0 * 256 + 4 * 16 + 0) >> 6));
}

void TestMicroblocks::testNodeInTile()
{
	UASSERTEQ(int, microNodeInTile(v3s16(0, 0, 0)), 0);
	UASSERTEQ(int, microNodeInTile(v3s16(3, 3, 3)), MICRO_TILE_NODES - 1);
	// Only the low two bits of each axis matter.
	UASSERTEQ(int, microNodeInTile(v3s16(4, 4, 4)), 0);
	UASSERTEQ(int, microNodeInTile(v3s16(15, 15, 15)), MICRO_TILE_NODES - 1);
	UASSERTEQ(int, microNodeInTile(v3s16(1, 0, 0)), 1);
	UASSERTEQ(int, microNodeInTile(v3s16(0, 1, 0)), 4);
	UASSERTEQ(int, microNodeInTile(v3s16(0, 0, 1)), 16);
}

void TestMicroblocks::testSubIndex()
{
	const MicroGeometry g = microGeometryFor(4);
	UASSERTEQ(int, microSubIndex(g, v3s16(0, 0, 0)), 0);
	UASSERTEQ(int, microSubIndex(g, v3s16(1, 0, 0)), 1);
	UASSERTEQ(int, microSubIndex(g, v3s16(0, 1, 0)), 4);
	UASSERTEQ(int, microSubIndex(g, v3s16(0, 0, 1)), 16);
	UASSERTEQ(int, microSubIndex(g, v3s16(3, 3, 3)), 63);
}

void TestMicroblocks::testPool()
{
	MicroTilePool pool(microGeometryFor(4), 4);
	UASSERTEQ(u32, pool.capacity(), 4);
	UASSERTEQ(u32, pool.used(), 0);
	// 64 header bytes plus 64 nodes of 32 bytes each.
	UASSERTEQ(u32, pool.tileStride(), 2112);

	const u16 a = pool.allocate();
	const u16 b = pool.allocate();
	UASSERT(a != MICRO_NO_TILE);
	UASSERT(b != MICRO_NO_TILE);
	UASSERT(a != b);
	UASSERTEQ(u32, pool.used(), 2);

	// A fresh tile starts as a single air slot, because CONTENT_AIR is 126 and
	// zeroed memory would otherwise mean content id 0.
	UASSERTEQ(int, pool.palette(a)[0], CONTENT_AIR);
	for (u16 i = 0; i < MICRO_TILE_NODES * 32; i++)
		UASSERTEQ(int, pool.nibbles(a)[i], 0);

	// Tiles must not overlap.
	pool.nibbles(a)[0] = 0xAB;
	UASSERTEQ(int, pool.nibbles(b)[0], 0);

	// A released tile comes back clean.
	pool.release(a);
	UASSERTEQ(u32, pool.used(), 1);
	const u16 c = pool.allocate();
	UASSERTEQ(int, pool.nibbles(c)[0], 0);
}

void TestMicroblocks::testPoolExhaustion()
{
	MicroTilePool pool(microGeometryFor(2), 2);
	UASSERT(pool.allocate() != MICRO_NO_TILE);
	UASSERT(pool.allocate() != MICRO_NO_TILE);
	// Exhaustion is reported, never satisfied by growing the pool.
	UASSERTEQ(int, pool.allocate(), MICRO_NO_TILE);
	UASSERTEQ(u32, pool.used(), 2);
	UASSERTEQ(u32, pool.capacity(), 2);
}

void TestMicroblocks::testPoolDoubleRelease()
{
	MicroTilePool pool(microGeometryFor(4), 2);
	const u16 a = pool.allocate();
	UASSERT(a != MICRO_NO_TILE);

	pool.release(a);
	UASSERTEQ(u32, pool.used(), 0);
	// Releasing the same tile again must be a no-op: it must not push a
	// second copy of `a` onto the freelist, which would otherwise let the
	// next two allocations both hand out `a`.
	pool.release(a);
	UASSERTEQ(u32, pool.used(), 0);

	const u16 b = pool.allocate();
	const u16 c = pool.allocate();
	UASSERT(b != MICRO_NO_TILE);
	UASSERT(c != MICRO_NO_TILE);
	// Two allocations must never return the same tile.
	UASSERT(b != c);
}

void TestMicroblocks::testPoolReleaseNeverAllocated()
{
	MicroTilePool pool(microGeometryFor(4), 2);
	// Tile 1 was never allocated; releasing it must be harmless, not corrupt
	// the freelist, and not disturb the used() count.
	pool.release(1);
	UASSERTEQ(u32, pool.used(), 0);

	const u16 a = pool.allocate();
	const u16 b = pool.allocate();
	UASSERT(a != MICRO_NO_TILE);
	UASSERT(b != MICRO_NO_TILE);
	UASSERT(a != b);
	UASSERTEQ(u32, pool.used(), 2);
	UASSERTEQ(int, pool.allocate(), MICRO_NO_TILE);
}

void TestMicroblocks::testLayer()
{
	MicroTilePool pool(microGeometryFor(4), 8);
	MicroLayer layer;
	UASSERT(layer.empty());

	// An untouched node reports CONTENT_IGNORE, which is distinct from air.
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 0), CONTENT_IGNORE);

	UASSERT(layer.setSub(pool, v3s16(1, 2, 3), 5, 42));
	UASSERT(!layer.empty());
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 5), 42);
	// Neighbouring sub-cubes of the same node stay air.
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 4), CONTENT_AIR);
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 6), CONTENT_AIR);

	// Nibble packing must not bleed between adjacent sub-cubes.
	UASSERT(layer.setSub(pool, v3s16(1, 2, 3), 6, 77));
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 5), 42);
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 6), 77);

	// A node in the same 4x4x4 cube shares the tile.
	UASSERTEQ(int, layer.tileAt(v3s16(0, 0, 0)), layer.tileAt(v3s16(3, 3, 3)));
	UASSERTEQ(u32, pool.used(), 1);
	// A node in the next cube takes another tile.
	UASSERT(layer.setSub(pool, v3s16(4, 2, 3), 0, 42));
	UASSERTEQ(u32, pool.used(), 2);
	UASSERT(layer.tileAt(v3s16(1, 2, 3)) != layer.tileAt(v3s16(4, 2, 3)));

	// Each tile carries its own palette: the second one holds air plus 42 only.
	UASSERTEQ(int, pool.paletteUsed(layer.tileAt(v3s16(4, 2, 3))), 2);

	// A material already in the palette reuses its slot instead of adding one.
	UASSERTEQ(int, pool.paletteUsed(layer.tileAt(v3s16(1, 2, 3))), 3);
	UASSERT(layer.setSub(pool, v3s16(1, 2, 3), 7, 42));
	UASSERTEQ(int, pool.paletteUsed(layer.tileAt(v3s16(1, 2, 3))), 3);
	UASSERTEQ(int, layer.getSub(pool, v3s16(1, 2, 3), 7), 42);

	layer.clear(pool);
	UASSERT(layer.empty());
	UASSERTEQ(u32, pool.used(), 0);
}

void TestMicroblocks::testLayerPaletteFull()
{
	MicroTilePool pool(microGeometryFor(4), 2);
	MicroLayer layer;

	// Slot 0 is air, so fifteen materials fit and the sixteenth is refused.
	for (content_t c = 1; c <= 15; c++)
		UASSERT(layer.setSub(pool, v3s16(0, 0, 0), c, c));

	const u32 used_before = pool.used();
	const u16 tile_before = layer.tileAt(v3s16(0, 0, 0));
	UASSERT(!layer.setSub(pool, v3s16(0, 0, 0), 20, 200));
	// The refused write's tile already existed before this call, so it must
	// not be released: it is still in use for the fifteen materials already
	// written into it.
	UASSERTEQ(u32, pool.used(), used_before);
	UASSERTEQ(int, layer.tileAt(v3s16(0, 0, 0)), tile_before);

	// The refusal must not corrupt what was already written.
	UASSERTEQ(int, layer.getSub(pool, v3s16(0, 0, 0), 1), 1);
	UASSERTEQ(int, layer.getSub(pool, v3s16(0, 0, 0), 15), 15);
	UASSERTEQ(int, layer.getSub(pool, v3s16(0, 0, 0), 20), CONTENT_AIR);
}

void TestMicroblocks::testLayerPoolExhausted()
{
	MicroTilePool pool(microGeometryFor(4), 1);
	MicroLayer layer;

	UASSERT(layer.setSub(pool, v3s16(0, 0, 0), 0, 42));
	// The second tile cannot be allocated, so the write is refused, not queued.
	UASSERT(!layer.setSub(pool, v3s16(4, 0, 0), 0, 42));
	UASSERTEQ(int, layer.tileAt(v3s16(4, 0, 0)), MICRO_NO_TILE);
	UASSERTEQ(int, layer.getSub(pool, v3s16(0, 0, 0), 0), 42);
}
