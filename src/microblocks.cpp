// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "microblocks.h"

#include <stdexcept>

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
