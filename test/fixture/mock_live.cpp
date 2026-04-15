#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>

extern "C" std::uint64_t RunLiveLoop(int rounds, int group_count, int lanes_per_group, std::uint64_t seed);

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

    for (;;) {
        const std::uint64_t result = RunLiveLoop(rounds, group_count, lanes_per_group, seed);
        std::cout << result << '\n';
        seed += 17;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}
