#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: %s versions...\n", argv[0]);
        return 1;
    }

    std::vector<std::string> versions(argv + 1, argv + argc);
    std::string latest;
    for (const auto& s : versions)
    {
        if (latest < s)
        {
            latest = s;
        }
    }

    printf("%s", latest.c_str());
    return 0;
}
