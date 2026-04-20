#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>

struct TraceLeaf
{
    int value;
    const char* label;
};

struct TraceNode
{
    int count;
    const char* message;
    TraceLeaf leaf;
};

extern "C" std::uint64_t RunLiveLoop(int rounds, int group_count, int lanes_per_group, std::uint64_t seed);
extern "C" int DerefInt(const int* value);
extern "C" std::size_t CountString(const char* text);
extern "C" int SumTraceNode(const TraceNode* node);

int main(int argc, char* argv[])
{
    int rounds = 4;
    int group_count = 3;
    int lanes_per_group = 4;
    int sleep_ms = 250;
    std::uint64_t seed = 12345;

    if (argc > 1)
        rounds = std::atoi(argv[1]);
    if (argc > 2)
        group_count = std::atoi(argv[2]);
    if (argc > 3)
        lanes_per_group = std::atoi(argv[3]);
    if (argc > 4)
        sleep_ms = std::atoi(argv[4]);

    std::cerr << "[ INFO ] pid=" << ::getpid() << '\n';
    std::cerr << "[ INFO ] calling RunLiveLoop(" << rounds << ", " << group_count << ", "
              << lanes_per_group << ", " << seed << ") repeatedly\n";
    std::cerr << "[ INFO ] also calling DerefInt, CountString, and SumTraceNode each loop\n";

    for (;;) {
        const int live_value = static_cast<int>((seed % 97) + rounds + group_count);
        const char* live_text = ((seed / 17) % 2 == 0) ? "sigminer-rich-output"
                                                       : "pointer-decoding-demo";
        const TraceNode live_node{
                .count = rounds + group_count + lanes_per_group,
                .message = live_text,
                .leaf =
                        TraceLeaf{
                                .value = static_cast<int>((seed % 13) + lanes_per_group),
                                .label = "leaf-node",
                        },
        };

        const std::uint64_t result = RunLiveLoop(rounds, group_count, lanes_per_group, seed);
        const int deref_result = DerefInt(&live_value);
        const std::size_t string_len = CountString(live_text);
        const int node_sum = SumTraceNode(&live_node);

        std::cout << result << '\n';
        std::cout << "deref=" << deref_result << " strlen=" << string_len
                  << " node_sum=" << node_sum << '\n';
        seed += 17;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}
