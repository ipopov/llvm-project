// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: cd %t
//
// RUN: %clang_cc1 -std=c++20 -fmodules -fno-implicit-modules \
// RUN:            -fmodules-local-submodule-visibility \
// RUN:            -fmodule-map-file=module.modulemap \
// RUN:            -fmodule-name=repro_module_a -emit-module \
// RUN:            -fmodules-embed-all-files -x c++ module.modulemap \
// RUN:            -Wuninitialized -Wuninitialized-const-reference \
// RUN:            -o repro_module_a.pcm
//
// RUN: %clang_cc1 -std=c++20 -fmodules -fno-implicit-modules \
// RUN:            -fmodules-local-submodule-visibility \
// RUN:            -fmodule-map-file=module.modulemap \
// RUN:            -fmodule-name=repro_wrapper_mock \
// RUN:            -fmodule-file=repro_module_a=repro_module_a.pcm \
// RUN:            -emit-module -fmodules-embed-all-files -x c++ module.modulemap \
// RUN:            -Wuninitialized -Wuninitialized-const-reference \
// RUN:            -o repro_wrapper_mock.pcm
//
// RUN: %clang_cc1 -std=c++20 -fmodules -fno-implicit-modules \
// RUN:            -fmodules-local-submodule-visibility \
// RUN:            -fmodule-map-file=module.modulemap \
// RUN:            -fmodule-name=repro \
// RUN:            -fmodule-file=repro_module_a=repro_module_a.pcm \
// RUN:            -fmodule-file=repro_wrapper_mock=repro_wrapper_mock.pcm \
// RUN:            -verify -Wuninitialized -Wuninitialized-const-reference \
// RUN:            -fsyntax-only repro_main.cpp

//--- module.modulemap
module TemplateClassModule {
  textual header "template_class.h"
}
module repro_module_a {
  header "module_a.h"
  export *
  use TemplateClassModule
}
module repro_wrapper_mock {
  header "repro_wrapper.h"
  export *
  use repro_module_a
  use TemplateClassModule
}
module repro {
  export *
  use repro_wrapper_mock
}

//--- template_class.h
#ifndef TEMPLATE_CLASS_H_
#define TEMPLATE_CLASS_H_

namespace std {
template <typename T>
T&& move(T& t) noexcept { return static_cast<T&&>(t); }

template <typename R, typename... Args>
struct coroutine_traits {
  using promise_type = typename R::promise_type;
};

template <typename Promise = void> struct coroutine_handle {
  static coroutine_handle from_address(void *addr) noexcept { return {}; }
};

struct suspend_always {
  bool await_ready() const noexcept { return false; }
  template <typename P> void await_suspend(coroutine_handle<P>) const noexcept {}
  void await_resume() const noexcept {}
};

struct suspend_never {
  bool await_ready() const noexcept { return true; }
  template <typename P> void await_suspend(coroutine_handle<P>) const noexcept {}
  void await_resume() const noexcept {}
};
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

enum class MyEnum { kValue1, kValue2 };
inline MyEnum GetLocalValue() { return MyEnum::kValue2; }

struct Pair { MyEnum first; MyEnum second; };
inline Pair GetLocalPair() { return Pair{MyEnum::kValue1, MyEnum::kValue2}; }

namespace test_namespace {
  inline int ns_foo = 42;
  void overloaded_func(int);
#if EXTRA_OVERLOAD
  void overloaded_func(double);
#endif
}

// 1. Basic local variables, type aliases, and decomposition declarations mapping
template <typename T>
class TemplateClassBasic {
 public:
  template <typename... Args>
  explicit TemplateClassBasic(Args&&... args);
};

template <typename T>
template <typename... Args>
TemplateClassBasic<T>::TemplateClassBasic(Args&&... args) {
  using LocalType = T*;
  const auto local_val = GetLocalValue();
  const auto [a, b] = GetLocalPair();
  auto holder = make_holder([&](auto x) {
    LocalType val = nullptr;
    (void)val;
    (void)local_val;
    (void)a;
  });
  holder.call();
}

// 2. Local structs and local enums mapping
template <typename T>
class TemplateClassLocalStruct {
 public:
  template <typename... Args>
  explicit TemplateClassLocalStruct(Args&&... args);
};

template <typename T>
template <typename... Args>
TemplateClassLocalStruct<T>::TemplateClassLocalStruct(Args&&... args) {
  struct LocalStruct { int x; };
  enum LocalEnum { kValue = 42 };
  using test_namespace::ns_foo;
  auto holder = make_holder([&](auto x) {
    LocalStruct l{0};
    int y = l.x + kValue + ns_foo;
    (void)y;
  });
  holder.call();
}

// 3. Spurious warning / switch-statement test
template <typename Stream, typename U>
void MockConsumer(Stream& strm, const U& val) {}

template <typename T> struct RemovePointer { using type = T; };
template <typename T> struct RemovePointer<T*> { using type = T; };

template <typename Stream>
struct MockPrinter {
  Stream* strm;
  template <typename U>
  void operator()(const U& val) {
    MockConsumer(*strm, val);
  }
};

template <typename F>
struct MockDumpVars {
  F f;
  explicit MockDumpVars(F f) : f(std::move(f)) {}
  template <typename Stream>
  void DoStream(Stream& strm) const { f(&strm); }
};

template <typename F>
auto mock_make_dump_vars(F f) { return MockDumpVars<F>(std::move(f)); }

struct MockStream {};
template <typename F>
MockStream& operator<<(MockStream& strm, const MockDumpVars<F>& lazy) {
  lazy.DoStream(strm);
  return strm;
}

#define MOCK_DUMP_VARS(var) \
  mock_make_dump_vars([&](auto* _strm) { \
    MockPrinter<typename RemovePointer<decltype(_strm)>::type>{_strm}(var); \
  })

template <typename T>
class TemplateClassWarning {
 public:
  template <typename... Args>
  explicit TemplateClassWarning(Args&&... args) {
    switch (const auto local_val = GetLocalValue(); local_val) {
      default:
        {
          MockStream strm;
          strm << MOCK_DUMP_VARS(local_val);
        }
    }
  }
};

// 4. Coroutines template method mapping
template <typename T>
struct MyTask {
  struct promise_type {
    MyTask get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

template <typename T>
class TemplateClassCoroutine {
 public:
  template <typename U>
  MyTask<U> coroutine_method() {
    using LocalType = T*;
    auto lam = [&](auto x) {
      LocalType val = nullptr;
      (void)val;
    };
    lam(0);
    co_return;
  }
};

// 5. Local class type alias mapping
template <typename T>
class TemplateClassLocalClass {
 public:
  template <typename... Args>
  explicit TemplateClassLocalClass(Args&&... args) {
    struct LocalClass {
      using LocalType = T*;
    };
    auto lam = [&](auto x) {
      typename LocalClass::LocalType val = nullptr;
      (void)val;
    };
    lam(0);
  }
};

// 6. Local function declaration mapping
template <typename T>
class TemplateClassLocalFunc {
 public:
  template <typename... Args>
  explicit TemplateClassLocalFunc(Args&&... args) {
    void local_func();
    auto lam = [&](auto x) {
      local_func();
    };
    lam(0);
  }
};

// 7. Local namespace alias mapping
template <typename T>
class TemplateClassNamespaceAlias {
 public:
  template <typename... Args>
  explicit TemplateClassNamespaceAlias(Args&&... args) {
    namespace my_alias = test_namespace;
    auto lam = [&](auto x) {
      int y = my_alias::ns_foo;
      (void)y;
    };
    lam(0);
  }
};

// 8. Nested generic lambda mapping
template <typename T>
class TemplateClassNestedLambda {
 public:
  template <typename... Args>
  explicit TemplateClassNestedLambda(Args&&... args) {
    using LocalType = T*;
    auto lam1 = [&](auto x) {
      auto lam2 = [&](auto y) {
        LocalType val = nullptr;
        (void)val;
      };
      lam2(0);
    };
    lam1(0);
  }
};

// 9. Overloaded local using declaration mapping
template <typename T>
class TemplateClassOverloadedUsing {
 public:
  template <typename... Args>
  explicit TemplateClassOverloadedUsing(Args&&... args) {
    using test_namespace::overloaded_func;
    auto lam = [&](auto x) {
      overloaded_func(0);
    };
    lam(0);
  }
};

#endif

//--- module_a.h
#pragma once
#include "template_class.h"

//--- repro_wrapper.h
#ifndef REPRO_WRAPPER_H_
#define REPRO_WRAPPER_H_
#define EXTRA_OVERLOAD 1
#include "template_class.h"
#include "module_a.h"

inline void TriggerInstantiation() {
  TemplateClassBasic<void> p1;
  TemplateClassLocalStruct<void> p2;
  TemplateClassWarning<void> p3;
  
  TemplateClassCoroutine<void> p4;
  p4.coroutine_method<int>();
  
  TemplateClassLocalClass<void> p5;
  TemplateClassLocalFunc<void> p6;
  TemplateClassNamespaceAlias<void> p7;
  TemplateClassNestedLambda<void> p8;
  TemplateClassOverloadedUsing<void> p9;
}
#endif

//--- repro_token.h
#ifndef REPRO_TOKEN_H_
#define REPRO_TOKEN_H_
#include "template_class.h"
#include "module_a.h"
template <typename... Ts>
struct MyOverload : Ts... {
  constexpr MyOverload(Ts... ts) : Ts(std::move(ts))... {}
};
template <typename... Ts>
MyOverload(Ts...) -> MyOverload<Ts...>;
inline auto my_lambda = [](int x) { return x; };
inline MyOverload visitor{my_lambda};

struct Token {
  void TriggerMethod() const {
    TemplateClassBasic<void> p1(*this);
    TemplateClassLocalStruct<void> p2(*this);
    TemplateClassWarning<void> p3(*this);
    
    TemplateClassCoroutine<void> p4;
    p4.coroutine_method<double>();
    
    TemplateClassLocalClass<void> p5(*this);
    TemplateClassLocalFunc<void> p6(*this);
    TemplateClassNamespaceAlias<void> p7(*this);
    TemplateClassNestedLambda<void> p8(*this);
    TemplateClassOverloadedUsing<void> p9(*this);
  }
};
#endif

//--- repro_main.cpp
// expected-no-diagnostics
#include "repro_wrapper.h"
#include "repro_token.h"
int main() {
  TriggerInstantiation();
  Token t;
  t.TriggerMethod();
}
