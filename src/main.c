#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAPI_EXPERIMENTAL 1
#include <node_api.h>

struct example_method;
typedef int (*example_method_get_args)(struct example_method* method, napi_env env);

typedef struct example_method {
  pthread_mutex_t mutex;
  napi_threadsafe_function threadsafe_function;
  bool initialized;
  bool completed;
  bool success;

  int result_int;
  int parameter;

  int error;
  napi_value result;
  size_t argc;
  napi_value argv[1];
  example_method_get_args get_args;
  napi_callback set_return;
  napi_callback fail;
  napi_ref set_return_ref;
  napi_ref fail_ref;
} example_method;

typedef struct example_info {
  pthread_t tid;
  napi_ref this;
  example_method func;
} example_info;

example_info example;

static inline void example_zero(example_info* info)
{
  memset(info, 0, sizeof(*info));
}

static inline void example_method_zero(example_method* method)
{
  memset(method, 0, sizeof(*method));
}

static inline void example_method_reset(example_method* method)
{
  method->initialized = false;
  method->completed = false;
  method->success = false;
  method->error = 0;
  method->result_int = 0;
  method->parameter = 0;
}

static inline int example_method_lock(example_method* method, bool block)
{
  if (block)
    return pthread_mutex_lock(&method->mutex);

  return pthread_mutex_trylock(&method->mutex);
}

static inline int example_method_unlock(example_method* method)
{
  return pthread_mutex_unlock(&method->mutex);
}

static inline int example_method_call_tsf(example_method* method, bool block)
{
  method->initialized = true;
  method->completed = false;
  method->success = false;

  if (example_method_unlock(method)) {
    return 1;
  } else if (napi_ok !=
             napi_call_threadsafe_function(method->threadsafe_function, 0, block ? napi_tsfn_blocking : napi_tsfn_nonblocking)) {
    return 1;
  } else {
    if (block) {
      while (!method->completed) {
      }

      if (example_method_lock(method, true)) {
        return 1;
      }
    }
  }

  return 0;
}

static inline napi_value example_method_set_return_int(example_method* method, napi_env env, napi_callback_info info)
{
  size_t argc = 1;
  napi_value argv[2];
  napi_value this;
  napi_value then;
  napi_value result;
  bool ispromise;

  if (example_method_lock(method, true))
    napi_throw_error(env, 0, "Could not lock mutex");
  else if (napi_ok != napi_get_cb_info(env, info, &argc, &result, &this, 0))
    napi_throw_error(env, 0, "Could not get callback info");
  else if (napi_ok != napi_is_promise(env, result, &ispromise))
    napi_throw_error(env, 0, "Could not check whether a promise was returned");
  else if (ispromise) {
    argc = 2;

    if (napi_get_named_property(env, result, "then", &then))
      napi_throw_error(env, 0, "Could not get 'then' from the returned promise");
    else if (napi_ok != napi_get_reference_value(env, method->set_return_ref, &argv[0]))
      napi_throw_error(env, 0, "Could not get referenced value 'set_return'");
    else if (napi_ok != napi_get_reference_value(env, method->fail_ref, &argv[1]))
      napi_throw_error(env, 0, "Could not get referenced value 'fail'");
    else if (napi_ok != napi_call_function(env, result, then, argc, argv, &result))
      napi_throw_error(env, 0, "Could not call 'then'");

  } else if (napi_ok != napi_get_value_int32(env, result, &method->result_int))
    napi_throw_error(env, 0, "Could not get return value");
  else {
    method->success = true;
    method->completed = true;
  }

  if (example_method_unlock(method))
    napi_throw_error(env, 0, "Could not unlock mutex");

  return 0;
}

static inline napi_value example_method_fail(example_method* method, napi_env env, napi_callback_info info)
{
  if (example_method_lock(method, true))
    napi_throw_error(env, 0, "Could not lock mutex");

  method->success = false;
  method->completed = true;

  example_method_unlock(method);

  return 0;
}

static inline int example_method_call_js_callback(example_method* method, napi_env env, napi_value callback, napi_value* result)
{
  napi_value this;

  if (method->get_args(method, env))
    napi_throw_error(env, 0, "Could not get args");
  else if (napi_ok != napi_get_reference_value(env, example.this, &this))
    napi_throw_error(env, 0, "Could not get 'this'");
  else if (napi_ok != napi_call_function(env, this, callback, method->argc, method->argv, result))
    napi_throw_error(env, 0, "Could not call js callback");
  else {
    return 0;
  }

  return 1;
}

static void example_method_threadsafe_callback(napi_env env, napi_value callback, void* context, void* data)
{
  example_method* method = context;
  napi_value then;
  napi_value result;
  bool ispromise;
  size_t argc = 2;
  napi_value global;
  napi_value argv[argc];

  if (example_method_lock(method, true))
    napi_throw_error(env, 0, "Could not lock a mutex");
  else if (example_method_call_js_callback(method, env, callback, &result))
    napi_throw_error(env, 0, "Could not call js callback");
  else if (napi_ok != napi_is_promise(env, result, &ispromise))
    napi_throw_error(env, 0, "Could not check whether a promise was returned");
  else if (!ispromise) {
    if (napi_ok != napi_get_global(env, &global))
      napi_throw_error(env, 0, "Could not get global");
    else if (napi_ok != napi_get_reference_value(env, method->set_return_ref, argv))
      napi_throw_error(env, 0, "Could not get referenced value 'set_return'");
    else if (example_method_unlock(method))
      napi_throw_error(env, 0, "Could not unlock a mutex");
    else if (napi_ok != napi_call_function(env, global, *argv, 1, &result, &result))
      napi_throw_error(env, 0, "Could not call 'set_return'");
    else
      return;
  } else if (napi_get_named_property(env, result, "then", &then))
    napi_throw_error(env, 0, "Could not get 'then' from the returned promise");
  else if (napi_ok != napi_get_reference_value(env, method->set_return_ref, &argv[0]))
    napi_throw_error(env, 0, "Could not get referenced value 'set_return'");
  else if (napi_ok != napi_get_reference_value(env, method->fail_ref, &argv[1]))
    napi_throw_error(env, 0, "Could not get referenced value 'fail'");
  else if (napi_ok != napi_call_function(env, result, then, argc, argv, &result))
    napi_throw_error(env, 0, "Could not call 'then'");

  if (example_method_unlock(method))
    napi_throw_error(env, 0, "Could not unlock a mutex");
}

static int example_method_func_get_args(example_method* method, napi_env env)
{
  method->argc = 1;

  if (napi_ok != napi_create_int32(env, method->parameter, &method->argv[0]))
    return 1;

  return 0;
}

napi_value example_method_func_set_return(napi_env env, napi_callback_info info)
{
  return example_method_set_return_int(&example.func, env, info);
}

napi_value example_method_func_fail(napi_env env, napi_callback_info info)
{
  return example_method_fail(&example.func, env, info);
}

int func(int parameter)
{
  example_method* method = &example.func;
  int ret = -1;

  if (!example_method_lock(method, true)) {
    if (!method->initialized) {
      method->parameter = parameter;

      if (example_method_call_tsf(method, true)) {
        return -1;
      }
    }

    if (method->completed) {
      if (method->success) {
        if (method->parameter == parameter) {
          ret = method->result_int;
          printf("func(%i) = %i\n", parameter, ret);
        }
      }

      example_method_reset(method);
    }

    example_method_unlock(method);
  }

  return ret;
}

void* start_thread(void* arguments)
{
  puts("Started");

  while(1)
    func(8008);

  return NULL;
}

static void example_finalize(napi_env env, void* data, void* hint)
{
  pthread_join(example.tid, 0);
}

static int example_method_init(example_method* method, napi_env env, napi_callback_info info, const char* name)
{
  size_t argc = 1;
  napi_value argv[1];
  napi_value this;
  napi_value property;
  napi_value async_name;
  napi_value set_return;
  napi_value fail;

  if (pthread_mutex_init(&method->mutex, NULL))
    return 1;
  else if (napi_ok != napi_get_cb_info(env, info, &argc, argv, &this, 0))
    return 1;
  else if (napi_ok != napi_get_named_property(env, argv[0], name, &property))
    return 1;
  else if (napi_ok != napi_create_function(env, "set_return", NAPI_AUTO_LENGTH, method->set_return, NULL, &set_return))
    return 1;
  else if (napi_ok != napi_create_reference(env, set_return, 0, &method->set_return_ref))
    return 1;
  else if (napi_ok != napi_create_function(env, "fail", NAPI_AUTO_LENGTH, method->fail, NULL, &fail))
    return 1;
  else if (napi_ok != napi_create_reference(env, fail, 0, &method->fail_ref))
    return 1;
  else if (napi_ok != napi_create_string_utf8(env, name, NAPI_AUTO_LENGTH, &async_name))
    return 1;
  else if (napi_ok != napi_create_threadsafe_function(env, property, 0, async_name, 1, 1, &example.tid, example_finalize, method,
                                                      example_method_threadsafe_callback, &method->threadsafe_function))
    return 1;

  return 0;
}

napi_value example_initialize(napi_env env, napi_callback_info info)
{
  napi_value this;
  size_t argc = 1;
  napi_value argv[argc];

  example.func.get_args = example_method_func_get_args;
  example.func.set_return = example_method_func_set_return;
  example.func.fail = example_method_func_fail;

  if (napi_ok != napi_get_cb_info(env, info, &argc, argv, &this, 0))
    napi_throw_error(env, 0, "Could not get callback info");
  else if (napi_ok != napi_create_reference(env, argv[0], 0, &example.this))
    napi_throw_error(env, 0, "Could not create a reference for 'this'");
  else if (example_method_init(&example.func, env, info, "func"))
      napi_throw_error(env, 0, "Could not initialize method 'func'");

  return 0;
}

napi_value start(napi_env env, napi_callback_info info)
{
  if (!pthread_create(&example.tid, NULL, start_thread, NULL)) {
    pthread_detach(example.tid);
    return 0;
  }

  napi_throw_error(env, 0, "Start failed");
  return 0;
}

static inline void node_export_fn(napi_env env, napi_value exports, const char* name, napi_callback cb)
{
  napi_value fn;

  if (napi_ok != napi_create_function(env, name, NAPI_AUTO_LENGTH, cb, NULL, &fn))
    napi_throw_error(env, 0, "Could not create a function");
  else if (napi_ok != napi_set_named_property(env, exports, name, fn))
    napi_throw_error(env, 0, "Could not export a function");
}

napi_value init(napi_env env, napi_value exports)
{
  example_zero(&example);
  node_export_fn(env, exports, "initialize", example_initialize);
  node_export_fn(env, exports, "start", start);

  return exports;
}

NAPI_MODULE(async_tsf, init);
