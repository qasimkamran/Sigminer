#include <stdint.h>

static volatile uint64_t g_live_total = 0;

static uint64_t TickLane(int lane, int cycle, uint64_t seed)
{
    uint64_t total = seed + (uint64_t)(lane * 131) + (uint64_t)(cycle * 17);

    for (int i = 0; i < 20000; ++i) {
        total += (uint64_t)((lane + 1) * (i + 3));
        total ^= total << 7;
        total ^= total >> 9;
    }

    return total;
}

__attribute__((noinline, visibility("default")))
uint64_t UpdateLaneGroup(int lane_start, int lane_count, int cycle, uint64_t seed)
{
    uint64_t group_total = 0;

    for (int lane = 0; lane < lane_count; ++lane)
        group_total += TickLane(lane_start + lane, cycle, seed + (uint64_t)lane);

    g_live_total ^= group_total;
    return group_total;
}

__attribute__((noinline, visibility("default")))
uint64_t ProcessRound(int group_count, int lanes_per_group, int cycle, uint64_t seed)
{
    uint64_t round_total = 0;

    for (int group = 0; group < group_count; ++group) {
        round_total ^= UpdateLaneGroup(
                group * lanes_per_group,
                lanes_per_group,
                cycle,
                seed + (uint64_t)(group * 97));
    }

    g_live_total += round_total;
    return round_total;
}

__attribute__((noinline, visibility("default")))
uint64_t RunLiveLoop(int rounds, int group_count, int lanes_per_group, uint64_t seed)
{
    uint64_t total = seed;

    for (int round = 0; round < rounds; ++round) {
        total += ProcessRound(group_count, lanes_per_group, round, seed + (uint64_t)round);
        total ^= total << 5;
        total ^= total >> 11;
    }

    g_live_total ^= total;
    return total;
}
