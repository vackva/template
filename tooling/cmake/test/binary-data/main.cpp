#include "Assets.h"

#include <cstdio>
#include <cstring>

int main() {
    if (Assets::greeting_txtSize != 19 || std::memcmp(Assets::greeting_txt, "hello, binary data\n", 19) != 0) {
        std::printf("greeting.txt mismatch (size %d)\n", Assets::greeting_txtSize);
        return 1;
    }
    if (Assets::noise_binSize != 300) {
        std::printf("noise.bin size %d\n", Assets::noise_binSize);
        return 1;
    }
    int size = 0;
    const char* data = Assets::getNamedResource("greeting.txt", size);
    if (data == nullptr || size != 19) {
        std::printf("registry lookup failed\n");
        return 1;
    }
    return 0;
}
