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
};

static TestMicroblocks g_test_instance;

void TestMicroblocks::runTests(IGameDef *gamedef)
{
	TEST(testGeometry);
	TEST(testTileSlot);
	TEST(testNodeInTile);
	TEST(testSubIndex);
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
