// Copyright (c)2019 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in innative.h

#ifndef IN__DEBUG_PDB_H
#define IN__DEBUG_PDB_H

#include "debug_sourcemap.h"
#include "stack.h"
#include <unordered_set>

namespace innative {
  namespace code {
    KHASH_DECLARE(intset, size_t, char);

    class DebugPDB : public DebugSourceMap
    {
    public:
      DebugPDB(SourceMap* s, Context* context, llvm::Module& m, const char* name, const char* filepath);
      ~DebugPDB();
      virtual void FuncDecl(llvm::Function* fn, unsigned int offset, unsigned int line, bool optimized) override;
      virtual void PostFuncBody(llvm::Function* fn, FunctionBody& body) override;
      virtual void FuncParam(llvm::Function* fn, size_t indice, FunctionDesc& desc) override;
      virtual void FuncLocal(llvm::Function* fn, size_t indice, FunctionDesc& desc) override;
      virtual llvm::DIType* GetDebugType(size_t index, llvm::DIType* parent = 0) override;
      virtual void UpdateVariables(llvm::Function* fn, SourceMapScope& scope) override;
      llvm::DIType* StructOffsetType(llvm::DIType* ty, llvm::DIScope* scope, llvm::DIFile* file, llvm::StringRef name,
                                     uint64_t indice, llvm::Function* fn);
      virtual void Finalize() override;

    protected:
      uint64_t _uid;
      kh_intset_t* _deferred;
    };
  }
}

#endif