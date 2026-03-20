#include "engine.hpp"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

int main(int argc, char* argv[])
{
  lucus::engine e;

  try {
      e.run(argc, argv);
  } catch (const std::exception& err) {
      std::cerr << err.what() << std::endl;
      return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
