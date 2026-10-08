/*
 * The apparent wind dial: a ring with the bow at the top and a needle
 * pointing where the apparent wind comes from, clockwise from the bow.
 * No needle when there is no apparent wind to show.
 */
#ifndef HEAD_UNIT_AWA_DIAL_H_
#define HEAD_UNIT_AWA_DIAL_H_

#include <grvl/component/Canvas.h>

#include "wind_state.h"

class AwaDial : public grvl::Canvas {
public:
	AwaDial(int32_t x, int32_t y, int32_t size) : grvl::Canvas{x, y, size, size} {}

	void Show(const struct wind_dial_view &dial) { this->dial = dial; }

	void Draw(grvl::Painter &painter, int32_t parent_x, int32_t parent_y) override;

private:
	struct wind_dial_view dial {};
};

#endif /* HEAD_UNIT_AWA_DIAL_H_ */
