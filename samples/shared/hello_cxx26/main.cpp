/* Copyright (C) 2025 cragson
   Copyright (C) 2026 ItsJokerZz

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <algorithm>
#include <cstdint>
#include <flat_map>
#include <format>
#include <mdspan>
#include <numeric>
#include <ranges>
#include <string>
#include <string_view>
#include <text_encoding>
#include <thread>
#include <tuple>
#include <vector>

#if defined(__PROSPERO__)
#include <ps5/klog.h>
#else
#include <ps4/klog.h>
#endif

static_assert(sizeof(int32_t) == 4,
              std::string_view("int32_t has to be 32 bits"));

template <typename... Ts> auto second(Ts... ts) { return ts...[1]; }

void forbidden(int) = delete("pass a double");
void forbidden(double) {}

void say(const std::string &msg) { klog_puts(msg.c_str()); }

void task(const std::string_view prefix) {
  std::vector<int32_t> vec = {4, 2, 3, 1, 5};

  std::ranges::sort(vec);

  say(std::format("[+] sorted vec: {}", vec));

  try {
    throw int32_t(1337);
  } catch (int e) {
    say(std::format("[!] Caught exception: {}", e));
  }
}

void news() {
  say(std::format("[+] pack indexing, second of ( 10, 20, 30 ): {}",
                  second(10, 20, 30)));

  auto [_, _, third] = std::tuple{1, 2, 3};
  say(std::format("[+] placeholder names, third: {}", third));

  forbidden(1.5);

  std::vector<int> a{1, 2}, b{3, 4};
  auto both = std::views::concat(a, b) | std::ranges::to<std::vector>();
  say(std::format("[+] views::concat of {{1, 2}} and {{3, 4}}: {}", both));

  say(std::format("[+] saturating_add<int8_t>( 100, 100 ): {}",
                  int(std::saturating_add<int8_t>(100, 100))));
  say(std::format("[+] saturating_cast<uint8_t>( 300 ): {}",
                  int(std::saturating_cast<uint8_t>(300))));

  std::flat_map<int, int> fm{{3, 30}, {1, 10}, {2, 20}};
  say(std::format("[+] flat_map, lowest key {} has value {}", fm.begin()->first,
                  fm.begin()->second));

  int data[6] = {1, 2, 3, 4, 5, 6};
  std::mdspan md(data, 2, 3);
  say(std::format("[+] mdspan 2x3, md[ 1, 2 ]: {}", md[1, 2]));

  say(std::format("[+] text_encoding of narrow literals: {}",
                  std::text_encoding::literal().name()));
}

int main() {
  say("[+] Hello, libcxx!");
  std::thread t(task, __FILE__);
  t.join();

  news();

  return 0;
}
