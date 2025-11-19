run "cmake" with "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" parameter, and then run clazy on every modified .cpp/.h file, and fix all errors and warnings before commiting it.
