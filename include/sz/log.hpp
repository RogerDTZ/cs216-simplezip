#pragma once

#include <iostream>

namespace sz {

namespace log {

inline bool log_info_switch = false;

template <typename... Ts>
void panic(Ts... args) {
  std::cerr << "[ERROR] ";
  (std::cerr << ... << args);
  std::cerr << std::endl;
  exit(-1);
}

template <typename... Ts>
void log(Ts... args) {
  if (log_info_switch) {
    std::cerr << "[INFO]  ";
    (std::cerr << ... << args);
    std::cerr << std::endl;
  }
}

}  // namespace log

}  // namespace sz
