
#pragma once

#include <param/param.h>
#include <stdint.h>

int check_vts(uint16_t node, uint16_t id);
void vts_add(double arr[4], uint16_t id, int count, uint64_t time_ms);
