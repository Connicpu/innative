// Copyright (c)2019 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in innative.h

#ifndef IN__TEST_H
#define IN__TEST_H

#include <utility>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#include "innative/path.h"
#include "innative/export.h"

class TestHarness
{
public:
  TestHarness(const IRExports& exports, const char* arg0, int loglevel, FILE* out, const char* folder);
  ~TestHarness();
  size_t Run(FILE* out);
  void test_allocator();
  void test_environment();
  void test_path();
  void test_queue();
  void test_stack();
  void test_stream();
  void test_util();
  void test_parallel_parsing();
  void test_malloc();
  int CompileWASM(const char* file);

  inline std::pair<uint32_t, uint32_t> Results() { auto r = _testdata; _testdata = { 0,0 }; return r; }

protected:
  inline void DoTest(bool test, const char* text, const char* file, int line)
  {
    ++_testdata.second;
    
    const char* f = strrchr(file, '/');
    if(!f)
      f = strrchr(file, '\\');
    if(f)
      file = f + 1;

    if(test)
      ++_testdata.first;
    else
      fprintf(_target, "%s[%i]: %s\n", file, line, text);
  }

  std::pair<uint32_t, uint32_t> _testdata;
  FILE* _target;
  const IRExports& _exports;
  const char* _arg0;
  int _loglevel;
  std::vector<std::string> _garbage;
  innative::Path _folder;
};

#define TEST(x) DoTest(x, ""#x, __FILE__, __LINE__)

#endif