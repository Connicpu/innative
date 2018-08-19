// Copyright �2018 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in innative.h

#include "innative/export.h"
#include "parse.h"
#include "validate.h"
#include "compile.h"
#include "tools.h"
#include "wat.h"
#include <atomic>
#include <thread>
#include <stdio.h>

using namespace innative;
using namespace utility;

Environment* innative::CreateEnvironment(unsigned int flags, unsigned int modules, unsigned int maxthreads)
{
  Environment* env = (Environment*)calloc(1, sizeof(Environment));
  if(env)
  {
    env->modulemap = kh_init_modules();
    env->whitelist = nullptr;
    env->cimports = kh_init_cimport();
    env->modules = trealloc<Module>(0, modules);
    if(!env->modules)
    {
      free(env);
      return 0;
    }

    env->capacity = modules;
    env->flags = flags;
    env->maxthreads = maxthreads;
  }
  return env;
}

void innative::DestroyEnvironment(Environment* env)
{
  if(!env)
    return;

  for(varuint32 i = 0; i < env->n_modules; ++i)
    kh_destroy_exports(env->modules[i].exports);

  if(env->whitelist)
    kh_destroy_modulepair(env->whitelist);
  
  kh_destroy_modules(env->modulemap);
  kh_destroy_cimport(env->cimports);
  free(env->modules);
  free(env);
}

void innative::LoadModule(Environment* env, size_t index, void* data, uint64_t size, const char* name, int* err)
{
  parse::Stream s = { (uint8_t*)data, size, 0 };
  if((env->flags & ENV_ENABLE_WAT) && size > 0 && s.data[0] != 0)
    *err = innative::wat::ParseWatModule(*env, env->modules[index], s.data, size, StringRef{ name, strlen(name) });
  else
    *err = ParseModule(s, env->modules[index], ByteArray((uint8_t*)name, (varuint32)strlen(name)));
  ((std::atomic<size_t>&)env->n_modules).fetch_add(1, std::memory_order_release);
}

void innative::AddModule(Environment* env, void* data, uint64_t size, const char* name, int* err)
{
  if(!env || !err)
  {
    *err = ERR_FATAL_NULL_POINTER;
    return;
  }

  if((env->flags & ENV_MULTITHREADED) != 0 && env->maxthreads > 0)
  {
    while(((std::atomic<size_t>&)env->size).load(std::memory_order_relaxed) - ((std::atomic<size_t>&)env->n_modules).load(std::memory_order_relaxed) >= env->maxthreads)
      std::this_thread::sleep_for(std::chrono::milliseconds(2)); // If we're using maxthreads, block until one finishes (we must block up here or we risk a deadlock)
  }

  size_t index = ((std::atomic<size_t>&)env->size).fetch_add(1, std::memory_order_acq_rel);
  if(index >= env->capacity)
  {
    while(((std::atomic<size_t>&)env->n_modules).load(std::memory_order_relaxed) != index)
      std::this_thread::sleep_for(std::chrono::milliseconds(5)); // If we've exceeded our capacity, block until all other threads have finished

    env->modules = trealloc<Module>(env->modules, index * 2);
    if(!env->modules)
    {
      *err = ERR_FATAL_OUT_OF_MEMORY;
      return;
    }
    ((std::atomic<size_t>&)env->capacity).store(index * 2, std::memory_order_release);
  }

  if(env->flags & ENV_MULTITHREADED)
    std::thread(LoadModule, env, index, data, size, name, err).detach();
  else
    LoadModule(env, index, data, size, name, err);
}

void innative::AddWhitelist(Environment* env, const char* module_name, const char* export_name, const FunctionSig* sig)
{
  if(!env->whitelist)
    env->whitelist = kh_init_modulepair();
  if(!module_name || !export_name)
    return;

  auto whitelist = tmalloc<char>(CanonWhitelist(module_name, export_name, nullptr));
  CanonWhitelist(module_name, export_name, whitelist);

  int r;
  auto iter = kh_put_modulepair(env->whitelist, whitelist, &r);
  kh_val(env->whitelist, iter) = !sig ? FunctionSig{ TE_NONE, 0, 0, 0, 0 } : *sig;
}

void innative::WaitForLoad(Environment* env)
{
  while(((std::atomic<size_t>&)env->size).load(std::memory_order_relaxed) > ((std::atomic<size_t>&)env->n_modules).load(std::memory_order_relaxed))
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Spin until all modules loaded
}

enum IR_ERROR innative::AddEmbedding(Environment* env, int tag, void* data, uint64_t size)
{
  if(!env)
    return ERR_FATAL_NULL_POINTER;
  
  Embedding* embed = tmalloc<Embedding>(1);
  embed->tag = tag;
  embed->data = data;
  embed->size = size;
  embed->next = env->embeddings;
  env->embeddings = embed;

  return ERR_SUCCESS;
}

enum IR_ERROR innative::Compile(Environment* env, const char* file)
{
  if(!env)
    return ERR_FATAL_NULL_POINTER;

  // Before validating, add all modules to the modulemap. We must do this outside of LoadModule for multithreading reasons.
  for(size_t i = 0; i < env->n_modules; ++i)
  {
    int r;
    khiter_t iter = kh_put_modules(env->modulemap, env->modules[i].name.str(), &r);
    if(!r)
      return ERR_FATAL_DUPLICATE_MODULE_NAME;
    kh_val(env->modulemap, iter) = i;
  }

  ValidateEnvironment(*env);
  if(env->errors)
  {
    internal::ReverseErrorList(env->errors); // Reverse error list so it appears in chronological order
    return ERR_VALIDATION_ERROR;
  }

  return CompileEnvironment(env, file);
}

IR_Entrypoint innative::LoadFunction(void* cache, const char* module_name, const char* function, const FunctionSig* sig)
{
  if(!function)
  {
    if(sig && !MatchFunctionSig(*sig, FunctionSig{ TE_func }))
      return nullptr;
    return (IR_Entrypoint)LoadDLLFunction(cache, IR_ENTRYPOINT);
  }

  //if(sig && !MatchFunctionSig(*sig, FunctionSig{ TE_func }))
  //  return nullptr;
  return (IR_Entrypoint)LoadDLLFunction(cache, utility::MergeName(module_name, function).c_str());
}
void* innative::LoadGlobal(void* cache, const char* module_name, const char* export_name)
{
  //if(sig && !MatchFunctionSig(*sig, FunctionSig{ TE_func }))
  //  return nullptr;
  return LoadDLLFunction(cache, utility::MergeName(module_name, export_name).c_str());
}

void* innative::LoadAssembly(int flags, const char* file)
{
  Path path(file != nullptr ? Path(file) : GetProgramPath() + IR_EXTENSION);
  void* handle = LoadDLL(path.Get().c_str());
  if(!handle)
    return 0;
  // TODO: return 0 if this isn't an exact CPU info match
  //uintcpuinfo source;
  //GetCPUInfo(source, flags);
  //return memcmp(target, source, sizeof(uintcpuinfo)) ? 0 : handle;
  return handle;
}

// Return pointers to all our internal functions
void innative_runtime(IRExports* exports)
{
  exports->CreateEnvironment = &CreateEnvironment;
  exports->AddModule = &AddModule;
  exports->AddWhitelist = &AddWhitelist;
  exports->WaitForLoad = &WaitForLoad;
  exports->AddEmbedding = &AddEmbedding;
  exports->Compile = &Compile;
  exports->LoadFunction = &LoadFunction;
  exports->LoadGlobal = &LoadGlobal;
  exports->LoadAssembly = &LoadAssembly;
  exports->DestroyEnvironment = &DestroyEnvironment;
}
void innative_set_work_dir_to_bin()
{
  SetWorkingDir(GetProgramPath().Get().c_str());
}
