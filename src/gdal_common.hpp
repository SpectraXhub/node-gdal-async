
#ifndef __GDAL_COMMON_H__
#define __GDAL_COMMON_H__

#include <cpl_error.h>
#include <gdal_version.h>
#include <stdio.h>

// nan
#include "nan-wrapper.h"

#include "utils/ptr_manager.hpp"

namespace node_gdal {
extern FILE *log_file;
extern PtrManager ptr_manager;
} // namespace node_gdal

#ifdef ENABLE_LOGGING
#define LOG(fmt, ...)                                                                                                  \
  if (node_gdal::log_file) {                                                                                           \
    fprintf(node_gdal::log_file, fmt "\n", __VA_ARGS__);                                                               \
    fflush(node_gdal::log_file);                                                                                       \
  }
#else
#define LOG(fmt, ...)
#endif

// Nan::New(null) -> seg fault
class SafeString {
    public:
  static v8::Local<v8::Value> New(const char *data) {
    Nan::EscapableHandleScope scope;
    if (!data) {
      return scope.Escape(Nan::Null());
    } else {
      return scope.Escape(Nan::New<v8::String>(data).ToLocalChecked());
    }
  }
};

inline const char *getOGRErrMsg(int err) {
  if (err == 6) {
    // get more descriptive error
    // TODO: test if all OGRErr failures report an error msg
    return CPLGetLastErrorMsg();
  }
  switch (err) {
    case 0: return "No error";
    case 1: return "Not enough data";
    case 2: return "Not enough memory";
    case 3: return "Unsupported geometry type";
    case 4: return "Unsupported operation";
    case 5: return "Corrupt Data";
    case 6: return "Failure";
    case 7: return "Unsupported SRS";
    default: return "Invalid Error";
  }
};

#define NODE_THROW_CPLERR(err) Nan::ThrowError(CPLGetLastErrorMsg());

#define NODE_THROW_LAST_CPLERR NODE_THROW_CPLERR

#define NODE_THROW_OGRERR(err) Nan::ThrowError(getOGRErrMsg(err));

#define ATTR(t, name, get, set) Nan::SetAccessor(t->InstanceTemplate(), Nan::New(name).ToLocalChecked(), get, set);

#define ATTR_DONT_ENUM(t, name, get, set)                                                                              \
  Nan::SetAccessor(t->InstanceTemplate(), Nan::New(name).ToLocalChecked(), get, set, Local<Value>(), DEFAULT, DontEnum);

NAN_SETTER(READ_ONLY_SETTER);

#define IS_WRAPPED(obj, type) Nan::New(type::constructor)->HasInstance(obj)

// ----- object property conversion -------

#define NODE_DOUBLE_FROM_OBJ(obj, key, var)                                                                            \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (!Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Nan::ThrowError("Object must contain property \"" key "\"");                                                     \
      return;                                                                                                          \
    }                                                                                                                  \
    Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                            \
    if (!val->IsNumber()) {                                                                                            \
      Nan::ThrowTypeError("Property \"" key "\" must be a number");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    var = Nan::To<double>(val).ToChecked();                                                                            \
  }

#define NODE_INT_FROM_OBJ(obj, key, var)                                                                               \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (!Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Nan::ThrowError("Object must contain property \"" key "\"");                                                     \
      return;                                                                                                          \
    }                                                                                                                  \
    Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                            \
    if (!val->IsNumber()) {                                                                                            \
      Nan::ThrowTypeError("Property \"" key "\" must be a number");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    var = Nan::To<int32_t>(val).ToChecked();                                                                           \
  }

#define NODE_STR_FROM_OBJ(obj, key, var)                                                                               \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (!Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Nan::ThrowError("Object must contain property \"" key "\"");                                                     \
      return;                                                                                                          \
    }                                                                                                                  \
    Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                            \
    if (!val->IsString()) {                                                                                            \
      Nan::ThrowTypeError("Property \"" key "\" must be a string");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    var = *Nan::Utf8String(val);                                                                                       \
  }

#define NODE_WRAPPED_FROM_OBJ(obj, key, type, var)                                                                     \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (!Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Nan::ThrowError("Object must contain property \"" key "\"");                                                     \
      return;                                                                                                          \
    }                                                                                                                  \
    Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                            \
    if (!val->IsObject() || val->IsNull() || !Nan::New(type::constructor)->HasInstance(val)) {                         \
      Nan::ThrowTypeError("Property \"" key "\" must be a " #type " object");                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    var = Nan::ObjectWrap::Unwrap<type>(val.As<Object>());                                                             \
    if (!var->isAlive()) {                                                                                             \
      Nan::ThrowError(key ": " #type " object has already been destroyed");                                            \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_WRAPPED_FROM_OBJ_OPT(obj, key, type, var)                                                                 \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                              \
      Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                          \
      if (val->IsObject() && Nan::New(type::constructor)->HasInstance(val)) {                                          \
        var = Nan::ObjectWrap::Unwrap<type>(val.As<Object>());                                                         \
        if (!var->isAlive()) {                                                                                         \
          Nan::ThrowError(key ": " #type " object has already been destroyed");                                        \
          return;                                                                                                      \
        }                                                                                                              \
      } else if (!val->IsNull() && !val->IsUndefined()) {                                                              \
        Nan::ThrowTypeError(key "property must be a " #type " object");                                                \
        return;                                                                                                        \
      }                                                                                                                \
    }                                                                                                                  \
  }

#define NODE_DOUBLE_FROM_OBJ_OPT(obj, key, var)                                                                        \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                              \
      Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                          \
      if (!val->IsNumber()) {                                                                                          \
        Nan::ThrowTypeError("Property \"" key "\" must be a number");                                                  \
        return;                                                                                                        \
      }                                                                                                                \
      var = Nan::To<double>(val).ToChecked();                                                                          \
    }                                                                                                                  \
  }

#define NODE_INT_FROM_OBJ_OPT(obj, key, var)                                                                           \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                              \
      Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                          \
      if (!val->IsNumber()) {                                                                                          \
        Nan::ThrowTypeError("Property \"" key "\" must be a number");                                                  \
        return;                                                                                                        \
      }                                                                                                                \
      var = Nan::To<int32_t>(val).ToChecked();                                                                         \
    }                                                                                                                  \
  }

#define NODE_STR_FROM_OBJ_OPT(obj, key, var)                                                                           \
  {                                                                                                                    \
    Local<String> sym = Nan::New(key).ToLocalChecked();                                                                \
    if (Nan::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                              \
      Local<Value> val = Nan::Get(obj, sym).ToLocalChecked();                                                          \
      if (!val->IsString()) {                                                                                          \
        Nan::ThrowTypeError("Property \"" key "\" must be a string");                                                  \
        return;                                                                                                        \
      }                                                                                                                \
      var = *Nan::Utf8String(val);                                                                                     \
    }                                                                                                                  \
  }

// ----- argument conversion -------

// determine field index based on string/numeric js argument
// f -> OGRFeature* or OGRFeatureDefn*

#define ARG_FIELD_ID(num, f, var)                                                                                      \
  {                                                                                                                    \
    if (info[num]->IsString()) {                                                                                       \
      std::string field_name = *Nan::Utf8String(info[num]);                                                            \
      var = f->GetFieldIndex(field_name.c_str());                                                                      \
      if (field_index == -1) {                                                                                         \
        Nan::ThrowError("Specified field name does not exist");                                                        \
        return;                                                                                                        \
      }                                                                                                                \
    } else if (info[num]->IsInt32()) {                                                                                 \
      var = Nan::To<int32_t>(info[num]).ToChecked();                                                                   \
      if (var < 0 || var >= f->GetFieldCount()) {                                                                      \
        Nan::ThrowRangeError("Invalid field index");                                                                   \
        return;                                                                                                        \
      }                                                                                                                \
    } else {                                                                                                           \
      Nan::ThrowTypeError("Field index must be integer or string");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_INT(num, name, var)                                                                                   \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsNumber()) {                                                                                        \
    Nan::ThrowTypeError(name " must be an integer");                                                                   \
    return;                                                                                                            \
  }                                                                                                                    \
  var = static_cast<int>(Nan::To<int64_t>(info[num]).ToChecked());

#define NODE_ARG_ENUM(num, name, enum_type, var)                                                                       \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsInt32() && !info[num]->IsUint32()) {                                                               \
    Nan::ThrowTypeError(name " must be of type " #enum_type);                                                          \
    return;                                                                                                            \
  }                                                                                                                    \
  var = enum_type(Nan::To<uint32_t>(info[num]).ToChecked());

#define NODE_ARG_BOOL(num, name, var)                                                                                  \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsBoolean()) {                                                                                       \
    Nan::ThrowTypeError(name " must be an boolean");                                                                   \
    return;                                                                                                            \
  }                                                                                                                    \
  var = Nan::To<bool>(info[num]).ToChecked();

#define NODE_ARG_DOUBLE(num, name, var)                                                                                \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsNumber()) {                                                                                        \
    Nan::ThrowTypeError(name " must be a number");                                                                     \
    return;                                                                                                            \
  }                                                                                                                    \
  var = Nan::To<double>(info[num]).ToChecked();

#define NODE_ARG_ARRAY(num, name, var)                                                                                 \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsArray()) { return Nan::ThrowTypeError((std::string(name) + " must be an array").c_str()); }        \
  var = info[num].As<Array>();

#define NODE_ARG_OBJECT(num, name, var)                                                                                \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsObject()) { return Nan::ThrowTypeError((std::string(name) + " must be an object").c_str()); }      \
  var = info[num].As<Object>();

#define NODE_ARG_WRAPPED(num, name, type, var)                                                                         \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (info[num]->IsNull() || info[num]->IsUndefined() || !Nan::New(type::constructor)->HasInstance(info[num])) {       \
    Nan::ThrowTypeError(name " must be an instance of " #type);                                                        \
    return;                                                                                                            \
  }                                                                                                                    \
  var = Nan::ObjectWrap::Unwrap<type>(info[num].As<Object>());                                                         \
  if (!var->isAlive()) {                                                                                               \
    Nan::ThrowError(#type " parameter already destroyed");                                                             \
    return;                                                                                                            \
  }

#define NODE_ARG_STR(num, name, var)                                                                                   \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsString()) {                                                                                        \
    Nan::ThrowTypeError(name " must be a string");                                                                     \
    return;                                                                                                            \
  }                                                                                                                    \
  var = (*Nan::Utf8String(info[num]))

// delete callback is in AsyncWorker::~AsyncWorker
#define NODE_ARG_CB(num, name, var)                                                                                    \
  if (info.Length() < num + 1) {                                                                                       \
    Nan::ThrowError(name " must be given");                                                                            \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num]->IsFunction()) {                                                                                      \
    Nan::ThrowTypeError(name " must be a function");                                                                   \
    return;                                                                                                            \
  }                                                                                                                    \
  var = new Nan::Callback(info[num].As<Function>())

// ----- optional argument conversion -------

#define NODE_ARG_INT_OPT(num, name, var)                                                                               \
  if (info.Length() > num) {                                                                                           \
    if (info[num]->IsInt32()) {                                                                                        \
      var = static_cast<int>(Nan::To<int64_t>(info[num]).ToChecked());                                                 \
    } else if (!info[num]->IsNull() && !info[num]->IsUndefined()) {                                                    \
      Nan::ThrowTypeError(name " must be an integer");                                                                 \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_ENUM_OPT(num, name, enum_type, var)                                                                   \
  if (info.Length() > num) {                                                                                           \
    if (info[num]->IsInt32() || info[num]->IsUint32()) {                                                               \
      var = static_cast<enum_type>(Nan::To<uint32_t>(info[num]).ToChecked());                                          \
    } else if (!info[num]->IsNull() && !info[num]->IsUndefined()) {                                                    \
      Nan::ThrowTypeError(name " must be an integer");                                                                 \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_BOOL_OPT(num, name, var)                                                                              \
  if (info.Length() > num) {                                                                                           \
    if (info[num]->IsBoolean()) {                                                                                      \
      var = Nan::To<bool>(info[num]).ToChecked();                                                                      \
    } else if (!info[num]->IsNull() && !info[num]->IsUndefined()) {                                                    \
      Nan::ThrowTypeError(name " must be an boolean");                                                                 \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_OPT_STR(num, name, var)                                                                               \
  if (info.Length() > num) {                                                                                           \
    if (info[num]->IsString()) {                                                                                       \
      var = *Nan::Utf8String(info[num]);                                                                               \
    } else if (!info[num]->IsNull() && !info[num]->IsUndefined()) {                                                    \
      Nan::ThrowTypeError(name " must be a string");                                                                   \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_DOUBLE_OPT(num, name, var)                                                                            \
  if (info.Length() > num) {                                                                                           \
    if (info[num]->IsNumber()) {                                                                                       \
      var = Nan::To<double>(info[num]).ToChecked();                                                                    \
    } else if (!info[num]->IsNull() && !info[num]->IsUndefined()) {                                                    \
      Nan::ThrowTypeError(name " must be a number");                                                                   \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_WRAPPED_OPT(num, name, type, var)                                                                     \
  if (info.Length() > num && !info[num]->IsNull() && !info[num]->IsUndefined()) {                                      \
    if (!Nan::New(type::constructor)->HasInstance(info[num])) {                                                        \
      Nan::ThrowTypeError(name " must be an instance of " #type);                                                      \
      return;                                                                                                          \
    }                                                                                                                  \
    var = Nan::ObjectWrap::Unwrap<type>(info[num].As<Object>());                                                       \
    if (!var->isAlive()) {                                                                                             \
      Nan::ThrowError(#type " parameter already destroyed");                                                           \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_ARRAY_OPT(num, name, var)                                                                             \
  if (info.Length() > num) {                                                                                           \
    if (info[num]->IsArray()) {                                                                                        \
      var = info[num].As<Array>();                                                                                     \
    } else if (!info[num]->IsNull() && !info[num]->IsUndefined()) {                                                    \
      Nan::ThrowTypeError(name " must be an array");                                                                   \
      return;                                                                                                          \
    }                                                                                                                  \
  }

// ----- wrapped methods w/ results-------

#define NODE_WRAPPED_METHOD_WITH_RESULT(klass, method, result_type, wrapped_method)                                    \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    info.GetReturnValue().Set(Nan::New<result_type>(obj->this_->wrapped_method()));                                    \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(                                                               \
  klass, method, result_type, wrapped_method, param_type, param_name)                                                  \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) return Nan::ThrowError(#klass " object has already been destroyed");                          \
    info.GetReturnValue().Set(Nan::New<result_type>(obj->this_->wrapped_method(param->get())));                        \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_ENUM_PARAM(                                                                  \
  klass, method, result_type, wrapped_method, enum_type, param_name)                                                   \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    enum_type param;                                                                                                   \
    NODE_ARG_ENUM(0, #param_name, enum_type, param);                                                                   \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    info.GetReturnValue().Set(Nan::New<result_type>(obj->this_->wrapped_method(param)));                               \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_STRING_PARAM(klass, method, result_type, wrapped_method, param_name)         \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    info.GetReturnValue().Set(Nan::New<result_type>(obj->this_->wrapped_method(param.c_str())));                       \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_INTEGER_PARAM(klass, method, result_type, wrapped_method, param_name)        \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    info.GetReturnValue().Set(Nan::New<result_type>(obj->this_->wrapped_method(param)));                               \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_DOUBLE_PARAM(klass, method, result_type, wrapped_method, param_name)         \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    info.GetReturnValue().Set(Nan::New<result_type>(obj->this_->wrapped_method(param)));                               \
  }

// ----- wrapped asyncable methods-------
#define NODE_WRAPPED_ASYNC_METHOD(klass, method, wrapped_method)                                                       \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    Nan::HandleScope scope;                                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    auto *gdal_obj = obj->this_;                                                                                       \
    GDALAsyncableJob<int> job;                                                                                   \
    job.persist(info.This());                                                                                          \
    job.main = [gdal_obj]() {                                                                                          \
      gdal_obj->wrapped_method();                                                                                      \
      return 0;                                                                                                        \
    };                                                                                                                 \
    job.rval = [](int, GDAL_ASYNCABLE_OBJS) { return Nan::Undefined().As<Value>(); };                                  \
    job.run(info, async, 0);                                                                                                 \
  }

// ----- wrapped asyncable methods w/ results-------

#define NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT(klass, async_type, method, result_type, wrapped_method)                  \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    Nan::HandleScope scope;                                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    auto *gdal_obj = obj->this_;                                                                                       \
    GDALAsyncableJob<async_type> job;                                                                            \
    job.persist(info.This());                                                                                          \
    job.main = [gdal_obj]() { return gdal_obj->wrapped_method(); };                                                    \
    job.rval = [](async_type r, GDAL_ASYNCABLE_OBJS) { return Nan::New<result_type>(r); };                             \
    job.run(info, async, 0);                                                                                                 \
  }

// param_type must be a node-gdal type
#define NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(                                                         \
  klass, async_type, method, result_type, wrapped_method, param_type, param_name)                                      \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    Nan::HandleScope scope;                                                                                            \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) return Nan::ThrowError(#klass " object has already been destroyed");                          \
    auto *gdal_obj = obj->this_;                                                                                       \
    auto *gdal_param = param->get();                                                                                   \
    GDALAsyncableJob<async_type> job;                                                                            \
    job.persist(info.This(), info[0].As<Object>());                                                                    \
    job.main = [gdal_obj, gdal_param]() { return gdal_obj->wrapped_method(gdal_param); };                              \
    job.rval = [](async_type r, GDAL_ASYNCABLE_OBJS) { return Nan::New<result_type>(r); };                             \
    job.run(info, async, 1);                                                                                                 \
  }

#define NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_ENUM_PARAM(                                                            \
  klass, async_type, method, result_type, wrapped_method, enum_type, param_name)                                       \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    Nan::HandleScope scope;                                                                                            \
    enum_type param;                                                                                                   \
    NODE_ARG_ENUM(0, #param_name, enum_type, param);                                                                   \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    auto *gdal_obj = obj->this_;                                                                                       \
    GDALAsyncableJob<async_type> job;                                                                            \
    job.persist(info.This());                                                                                          \
    job.main = [gdal_obj, param]() { return gdal_obj->wrapped_method(param); };                                        \
    job.rval = [](async_type r, GDAL_ASYNCABLE_OBJS) { return Nan::New<result_type>(r); };                             \
    job.run(info, async, 1);                                                                                                 \
  }

#define NODE_WRAPPED_ASYNC_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(                                                  \
  klass, async_type, method, wrapped_method, param_type, param_name)                                                   \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    Nan::HandleScope scope;                                                                                            \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    auto gdal_obj = obj->this_;                                                                                        \
    auto gdal_param = param->get();                                                                                    \
    GDALAsyncableJob<async_type> job;                                                                            \
    job.persist(info.This(), info[0].As<Object>());                                                                    \
    job.main = [gdal_obj, gdal_param]() {                                                                              \
      int err = gdal_obj->wrapped_method(gdal_param);                                                                  \
      if (err) throw getOGRErrMsg(err);                                                                                \
      return err;                                                                                                      \
    };                                                                                                                 \
    job.rval = [](async_type, GDAL_ASYNCABLE_OBJS) { return Nan::Undefined().As<Value>(); };                           \
    job.run(info, async, 1);                                                                                                 \
  }

// ----- wrapped methods w/ CPLErr result (throws) -------

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT(klass, method, wrapped_method)                                          \
  NAN_METHOD(klass::method) {                                                                                          \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method();                                                                            \
    if (err) {                                                                                                         \
      NODE_THROW_CPLERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_WRAPPED_PARAM(klass, method, wrapped_method, param_type, param_name)  \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param->get());                                                                \
    if (err) {                                                                                                         \
      NODE_THROW_CPLERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_STRING_PARAM(klass, method, wrapped_method, param_name)               \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param.c_str());                                                               \
    if (err) {                                                                                                         \
      NODE_THROW_CPLERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_INTEGER_PARAM(klass, method, wrapped_method, param_name)              \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_CPLERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_DOUBLE_PARAM(klass, method, wrapped_method, param_name)               \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_CPLERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

// ----- wrapped methods w/ OGRErr result (throws) -------

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT(klass, method, wrapped_method)                                          \
  NAN_METHOD(klass::method) {                                                                                          \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method();                                                                            \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(klass, method, wrapped_method, param_type, param_name)  \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param->get());                                                                \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_STRING_PARAM(klass, method, wrapped_method, param_name)               \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param.c_str());                                                               \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_INTEGER_PARAM(klass, method, wrapped_method, param_name)              \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_DOUBLE_PARAM(klass, method, wrapped_method, param_name)               \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

// ----- wrapped methods -------

#define NODE_WRAPPED_METHOD(klass, method, wrapped_method)                                                             \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method();                                                                                      \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_WRAPPED_PARAM(klass, method, wrapped_method, param_type, param_name)                \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param->get());                                                                          \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_INTEGER_PARAM(klass, method, wrapped_method, param_name)                            \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_DOUBLE_PARAM(klass, method, wrapped_method, param_name)                             \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_BOOLEAN_PARAM(klass, method, wrapped_method, param_name)                            \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    bool param;                                                                                                        \
    NODE_ARG_BOOL(0, #param_name, param);                                                                              \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_ENUM_PARAM(klass, method, wrapped_method, enum_type, param_name)                    \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    enum_type param;                                                                                                   \
    NODE_ARG_ENUM(0, #param_name, enum_type, param);                                                                   \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_STRING_PARAM(klass, method, wrapped_method, param_name)                             \
  NAN_METHOD(klass::method) {                                                                                          \
    Nan::HandleScope scope;                                                                                            \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = Nan::ObjectWrap::Unwrap<klass>(info.This());                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Nan::ThrowError(#klass " object has already been destroyed");                                                    \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param.c_str());                                                                         \
    return;                                                                                                            \
  }

#endif
