#include "engine.hpp"
#include "validation_sink.hpp"

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

  const lucus::u32 validation_errors = lucus::validation_sink::instance().errorCount();
  if (validation_errors > 0) {
      std::cerr << "Validation reported " << validation_errors << " error(s)" << std::endl;
      return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
