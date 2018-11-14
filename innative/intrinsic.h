// Copyright �2018 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in innative.h

#ifndef __INTRINSIC_H__IR__
#define __INTRINSIC_H__IR__

#include "innative/schema.h"
#include "stack.h"
#pragma warning(push)
#pragma warning(disable : 4146 4267 4141 4244 4624)
#define _SCL_SECURE_NO_WARNINGS
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Target/TargetMachine.h"
#pragma warning(pop)

namespace innative {
  namespace code {
    struct BlockResult
    {
      llvm::Value* v;
      llvm::BasicBlock* b;
      BlockResult* next;
    };

    struct Block
    {
      llvm::BasicBlock* block; // Label
      llvm::BasicBlock* ifelse; // Label for else statement
      size_t limit; // Limit of value stack
      varsint7 sig; // Block signature
      uint8_t op; // instruction that pushed this label
      llvm::DIScope* scope; // Debug lexical scope for this block
      BlockResult* results; // Holds alternative branch results targeting this block
    };

    struct Function
    {
      llvm::Function* internal;
      llvm::Function* exported;
      llvm::Function* imported;
    };

    KHASH_DECLARE(importhash, const char*, llvm::GlobalObject*);

    struct Context
    {
      const Environment& env;
      Module& m;
      llvm::LLVMContext& context;
      llvm::Module* llvm;
      llvm::IRBuilder<>& builder;
      llvm::TargetMachine* machine;
      kh_importhash_t* importhash;
      llvm::IntegerType* intptrty;
      llvm::DIBuilder* dbuilder;
      llvm::DIType* diF32;
      llvm::DIType* diF64;
      llvm::DIType* diI32;
      llvm::DIType* diI64;
      llvm::DIType* diI1;
      llvm::DIType* diVoid;
      llvm::DICompileUnit* dcu;
      llvm::DIFile* dunit;
      Stack<llvm::Value*> values; // Tracks the current value stack
      Stack<Block> control; // Control flow stack
      std::vector<llvm::AllocaInst*> locals;
      std::vector<llvm::GlobalVariable*> memories;
      std::vector<llvm::GlobalVariable*> tables;
      std::vector<llvm::GlobalVariable*> globals;
      std::vector<Function> functions;
      llvm::Function* init;
      llvm::Function* start;
      llvm::Function* memgrow;
    };

    llvm::Function* IR_Intrinsic_ToC(llvm::Function* f, struct Context& context);
    llvm::Function* IR_Intrinsic_FromC(llvm::Function* f, struct Context& context);

    struct Intrinsic
    {
      const char* name;
      llvm::Function* (*gen)(llvm::Function* f, struct Context&);
      llvm::Function* fn;
    };

    static Intrinsic intrinsics[] = {
      Intrinsic{ "_innative_to_c", &IR_Intrinsic_ToC, nullptr },
      Intrinsic{ "_innative_from_c", &IR_Intrinsic_FromC, nullptr }
    };
  }
}

#endif