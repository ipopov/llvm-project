// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
// RUN: cd %t
//
// RUN: %clang_cc1 -std=c++20 -I. m_template.cppm -emit-reduced-module-interface \
// RUN:            -o m_template.pcm
//
// RUN: %clang_cc1 -std=c++20 -I. m_wrapper.cppm \
// RUN:            -emit-reduced-module-interface \
// RUN:            -fmodule-file=m_template=m_template.pcm \
// RUN:            -o m_wrapper.pcm
//
// RUN: %clang_cc1 -std=c++20 -I. main.cpp \
// RUN:            -fmodule-file=m_template=m_template.pcm \
// RUN:            -fmodule-file=m_wrapper=m_wrapper.pcm \
// RUN:            -fsyntax-only -verify

//--- template_class.h
#pragma once

namespace std {
template <typename T>
T&& move(T& t) noexcept { return static_cast<T&&>(t); }
}

template <typename F>
struct Holder {
  F f;
  explicit Holder(F f) : f(std::move(f)) {}
  void call() const { f(0); }
};

template <typename F>
auto make_holder(F f) {
  return Holder<F>(std::move(f));
}

template <typename T>
class TemplateClass1 {
 public:
  template <typename... Args>
  explicit TemplateClass1(Args&&... args);
};

template <typename T>
template <typename... Args>
TemplateClass1<T>::TemplateClass1(Args&&... args) {
  using LocalType = T*;
  auto holder = make_holder([&](auto x) {
    LocalType val = nullptr;
  });
  holder.call();
}

template <typename T>
class TemplateClass2 {
 public:
  template <typename... Args>
  explicit TemplateClass2(Args&&... args);
};

template <typename T>
template <typename... Args>
TemplateClass2<T>::TemplateClass2(Args&&... args) {
  using LocalType = T*;
  auto holder = make_holder([&](auto x) {
    LocalType val = nullptr;
  });
  holder.call();
}

//--- m_template.cppm
module;
#include "template_class.h"
export module m_template;
export using ::TemplateClass1;
export using ::TemplateClass2;

//--- m_wrapper.cppm
module;
#include "template_class.h"
export module m_wrapper;
import m_template;

export inline void TriggerInstantiation() {
  TemplateClass1<void> p1;
  TemplateClass2<void> p2;
}

//--- main.cpp
// expected-no-diagnostics
import m_template;
import m_wrapper;
#include "template_class.h"

struct Token {};

int main() {
  TriggerInstantiation();
  Token t;
  TemplateClass1<void> p1(t);
  TemplateClass2<void> p2(t);
}
