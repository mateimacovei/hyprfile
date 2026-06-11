#include "Application.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

#include "cli/Parser.hpp"
#include "debug/DependencyReport.hpp"

int run(int argc, char **argv)
{
    try
    {
        std::expected<CLI::SCLIOptions, std::string> optsOrErr = CLI::parseArguments(argc, argv);
        if (!optsOrErr)
        {
            std::cerr << "Error: " << optsOrErr.error() << '\n';
            return EXIT_FAILURE;
        }

        const CLI::SCLIOptions opts = *optsOrErr;
        if (opts.debug)
        {
            const auto report = Debug::buildDependencyReport(Debug::makeSystemCheckContext());
            std::cout << report.text;
            return report.allFound ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        Application app(&opts);
        int res = app.run();
        return res;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
int main(int argc, char **argv)
{
    return run(argc, argv);
}
