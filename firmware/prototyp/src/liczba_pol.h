// SPDX-License-Identifier: GPL-3.0-or-later
// Liczba pól struktury-agregatu w czasie kompilacji (T-WYM-99): strażnik kluczy pamięci podręcznej. Struktura bez
// konstruktorów, klas bazowych i pól prywatnych (agregat, także z domyślnymi wartościami pól) da się zbudować jako
// `T{x1, …, xN}` dla N nie większego niż liczba jej pól, a dla większego nie; `Dowolny` zamienia się na typ każdego
// pola. Działa niezależnie od wypełnienia (padding). Użycie: static_assert(liczba_pol::policz<T>() == N, "…").
#pragma once
#include <cstddef>
#include <utility>

namespace liczba_pol {
struct Dowolny {
  template <class T>
  operator T() const;  // tylko w kontekście nieobliczanym (decltype), bez definicji
};
template <class T, class... A>
constexpr auto daSie(int) -> decltype(T{std::declval<A>()...}, true) {
  return true;
}
template <class T, class... A>
constexpr bool daSie(...) {
  return false;
}
template <class T, std::size_t... I>
constexpr bool zN(std::index_sequence<I...>) {
  return daSie<T, decltype((void)I, Dowolny{})...>(0);
}
// Liczba pól agregatu T (najwyżej 64).
template <class T, std::size_t N = 0>
constexpr std::size_t policz() {
  if constexpr (N < 64 && zN<T>(std::make_index_sequence<N + 1>{})) {
    return policz<T, N + 1>();
  } else {
    return N;
  }
}
}  // namespace liczba_pol
