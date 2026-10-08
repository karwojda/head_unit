#include "awa_dial.h"

#include <cmath>

static constexpr uint32_t RING_COLOR = 0xFF9EA2A6;
static constexpr uint32_t NEEDLE_COLOR = 0xFFFFFFFF;
static constexpr float PI = 3.14159265f;

void AwaDial::Draw(grvl::Painter &painter, int32_t parent_x, int32_t parent_y)
{
	if (!Visible) {
		return;
	}

	const int16_t cx = parent_x + X + Width / 2;
	const int16_t cy = parent_y + Y + Height / 2;
	const int16_t radius = Width / 2 - 2;

	/* grvl's circles take the top-left corner of their bounding box. */
	painter.DrawCircle(cx - radius, cy - radius, radius, RING_COLOR);
	/* Bow mark at the top. */
	painter.DrawLine(cx, cy - radius, cx, cy - radius + radius / 6, RING_COLOR);

	if (!dial.valid) {
		return;
	}

	/* Clockwise from the bow (up), as the sensor reports it. */
	const float rad = dial.deg * PI / 180.0f;
	const int16_t tip_x = cx + static_cast<int16_t>(std::sin(rad) * (radius - 6));
	const int16_t tip_y = cy - static_cast<int16_t>(std::cos(rad) * (radius - 6));

	painter.DrawAntialiasedLine(cx, cy, tip_x, tip_y, NEEDLE_COLOR);
	painter.FillCircle(cx - 3, cy - 3, 3, NEEDLE_COLOR);
}
