#ifndef __NODE_OGR_LINEARRING_H__
#define __NODE_OGR_LINEARRING_H__

// node
#include <node.h>
#include <node_object_wrap.h>

// nan
#include "nan-wrapper.h"

// ogr
#include <ogrsf_frmts.h>

#include "gdal_geometry.hpp"

using namespace v8;
using namespace node;

namespace node_gdal {

class LinearRing : public GeometryBase<LinearRing, OGRLinearRing> {

    public:
  static Nan::Persistent<FunctionTemplate> constructor;
  using GeometryBase<LinearRing, OGRLinearRing>::GeometryBase;

  static void Initialize(Local<Object> target);
  static NAN_METHOD(New);
  using GeometryBase<LinearRing, OGRLinearRing>::New;
  static NAN_METHOD(toString);
  static NAN_METHOD(getArea);
};

} // namespace node_gdal
#endif
