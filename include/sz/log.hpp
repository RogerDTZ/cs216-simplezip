#pragma once

#include <iostream>

#include "sz/common.hpp"

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
  if (log_info_switch) {
    std::cerr << "[INFO] ";
    (std::cerr << ... << args);
    std::cerr << std::endl;
  }
}

}  // namespace log

}  // namespace sz
