#pragma once

#include <iostream>
#include <string>
#include <utility>

namespace sz {

class ProgressBar {
 public:
  ProgressBar() = delete;

  ProgressBar(std::string display_name, std::ostream* os, size_t workload,
              int width, char ch_space, char ch_fill, char ch_bound)
      : m_display_name(std::move(display_name)),
        m_os(os),
        m_accumulate(0),
        m_workload(workload),
        m_width(width),
        m_ch_space(ch_space),
        m_ch_fill(ch_fill),
        m_ch_bound(ch_bound),
        m_percentage(0.0f),
        m_fill_cnt(0),
        m_display_flag(false) {}

  void set_display(const bool flag) {
    if (flag != m_display_flag) {
      m_display_flag = flag;
      if (m_display_flag) {
        flush_display();
      } else {
        *m_os << "\n\r";
        m_os->flush();
      }
    }
  }

  void increase_progress(const size_t delta) {
    m_accumulate += delta;
    uniform_accumulation();
    update_percentage();
  }

  void set_progress(const size_t progress) {
    m_accumulate = progress;
    uniform_accumulation();
    update_percentage();
  }

  void set_full() {
    m_accumulate = m_workload;
    update_percentage();
  }

 private:
  std::string m_display_name;
  std::ostream* m_os;

  size_t m_accumulate;
  size_t m_workload;

  int m_width;
  char m_ch_space;
  char m_ch_fill;
  char m_ch_bound;

  float m_percentage;
  int m_fill_cnt;
  bool m_display_flag;

  void uniform_accumulation() {
    if (m_accumulate > m_workload) {
      m_accumulate = m_workload;
    }
  }

  void update_percentage() {
    m_percentage =
        static_cast<float>(m_accumulate) / static_cast<float>(m_workload);
    const int fill_cnt =
        static_cast<int>(m_percentage * static_cast<float>(m_width));
    if (fill_cnt == m_fill_cnt && m_percentage < 1.0f) {
      return;
    }
    m_fill_cnt = fill_cnt;
    flush_display();
  }

  void flush_display() const {
    *m_os << m_display_name << " [";
    for (int i = 0; i < m_fill_cnt - 1; ++i) {
      *m_os << m_ch_fill;
    }
    *m_os << ((m_percentage == 1.0f) ? m_ch_fill : m_ch_bound);
    for (int i = m_fill_cnt; i < m_width; ++i) {
      *m_os << m_ch_space;
    }
    *m_os << "] " << static_cast<int>(m_percentage * 100.0f) << " %    \r";
    m_os->flush();
  }
};

}  // namespace sz
