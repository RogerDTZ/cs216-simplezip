#pragma once

#include <iostream>

namespace sz {

namespace log {

template <typename... Ts>
void panic(Ts... args) {
  std::cerr << "[ERROR] ";
  (std::cerr << ... << args);
  std::cerr << std::endl;
  exit(-1);
}

template <typename... Ts>
void log(Ts... args) {
  std::cerr << "[INFO]  ";
  (std::cerr << ... << args);
  std::cerr << std::endl;
}

}  // namespace log

}  // namespace sz
