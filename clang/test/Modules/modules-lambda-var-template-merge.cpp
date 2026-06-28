// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
// RUN: cd %t
//
// RUN: %clang_cc1 -std=c++20 -I. m_a.cppm -emit-reduced-module-interface \
// RUN:            -o m_a.pcm
//
// RUN: %clang_cc1 -std=c++20 -I. m_b.cppm \
// RUN:            -emit-reduced-module-interface \
// RUN:            -o m_b.pcm
//
// RUN: %clang_cc1 -std=c++20 -I. main.cpp \
// RUN:            -fmodule-file=m_a=m_a.pcm \
// RUN:            -fmodule-file=m_b=m_b.pcm \
// RUN:            -fsyntax-only -verify

//--- template.h
#pragma once
template <typename T>
inline auto var = [](T x){ return x; };

//--- m_a.cppm
module;
#include "template.h"
export module m_a;
export auto Trigger1() {
  return var<int>;
}

//--- m_b.cppm
module;
#include "template.h"
export module m_b;
export auto Trigger2() {
  return var<int>;
}

//--- main.cpp
// expected-no-diagnostics
import m_a;
import m_b;

void use() {
  static_assert(__is_same(decltype(Trigger1()), decltype(Trigger2())));
}
