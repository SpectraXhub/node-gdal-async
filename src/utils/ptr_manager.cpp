#include "ptr_manager.hpp"
#include "../gdal_dataset.hpp"
#include "../gdal_group.hpp"
#include "../gdal_mdarray.hpp"
#include "../gdal_dimension.hpp"
#include "../gdal_attribute.hpp"
#include "../gdal_layer.hpp"
#include "../gdal_rasterband.hpp"

#include <sstream>
#include <thread>

// Here used to be dragons, but now there is a shopping mall
//
// This is the object store, a singleton
//
// It serves 2 purposes:
//
// First, it keeps track of created objects so that they can be reused
// The point of this mechanism is that it returns a reference to the same object
// for two successive calls of `ds.bands.get(1)` for example
// For this use, the V8 objects are indexed with the pointer to the GDAL
// base object
// uids won't work for this use
//
// Second, it is allocated entirely outside of the V8 memory management and the GC
// Thus, it is accessible from the worker threads
// The async locks live here
// For this use, the V8 objects are indexed with numeric uids
// ptrs won't be safe for this use

namespace node_gdal {

// Because of severe bugs linked to C++14 template variables in MSVC
// these two must be here and must have file scope
// MSVC throws an Internal Compiler Error when specializing templated variables
// and the linker doesn't use the right address when processing exported symbols
template <typename GDALPTR> using UidMap = map<long, shared_ptr<ObjectStoreItem<GDALPTR>>>;
template <typename GDALPTR> using PtrMap = map<GDALPTR, shared_ptr<ObjectStoreItem<GDALPTR>>>;
template <typename GDALPTR> static UidMap<GDALPTR> uidMap;
template <typename GDALPTR> static PtrMap<GDALPTR> ptrMap;

void uv_sem_deleter::operator()(uv_sem_t *p) {
  uv_sem_destroy(p);
  delete p;
}

class uv_scoped_mutex {
    public:
  inline uv_scoped_mutex(uv_mutex_t *lock) : lock(lock) {
    uv_mutex_lock(lock);
  }
  inline ~uv_scoped_mutex() {
    uv_mutex_unlock(lock);
  }

    private:
  uv_mutex_t *lock;
};

ObjectStore::ObjectStore() : uid(1) {
#ifdef PTHREAD_MUTEX_DEBUG
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init(&master_lock, &attr);
#else
  uv_mutex_init(&master_lock);
#endif
  uv_cond_init(&master_sleep);
}

ObjectStore::~ObjectStore() {
  uv_mutex_destroy(&master_lock);
  uv_cond_destroy(&master_sleep);
}

bool ObjectStore::isAlive(long uid) {
  if (uid == 0) return true;
  return uidMap<GDALRasterBand *>.count(uid) > 0 || uidMap<OGRLayer *>.count(uid) > 0 ||
    uidMap<GDALDataset *>.count(uid) > 0
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
    || uidMap<shared_ptr<GDALGroup>>.count(uid) > 0 || uidMap<shared_ptr<GDALMDArray>>.count(uid) > 0 ||
    uidMap<shared_ptr<GDALDimension>>.count(uid) > 0 || uidMap<shared_ptr<GDALAttribute>>.count(uid) > 0
#endif
    ;
}

/*
 * Lock a Dataset by uid, throws when the Dataset has been destroyed
 * There is a single global condition which allows to avoid active spinning
 * Every time a Dataset releases a lock it must broadcast the condition
 */
AsyncLock ObjectStore::lockDataset(long uid) {
  uv_scoped_mutex lock(&master_lock);
  while (true) {
    auto parent = uidMap<GDALDataset *>.find(uid);
    if (parent == uidMap<GDALDataset *>.end()) { throw "Parent Dataset object has already been destroyed"; }
    int r = uv_sem_trywait(parent->second->async_lock.get());
    if (r == 0) { return parent->second->async_lock; }
    uv_cond_wait(&master_sleep, &master_lock);
  }
}

/*
 * Lock several Datasets by uid avoiding deadlocks, same semantics as the previous one
 */
vector<AsyncLock> ObjectStore::lockDatasets(vector<long> uids) {
  // There is lots of copying around here but these vectors are never longer than 3 elements
  // Avoid deadlocks
  sort(uids.begin(), uids.end());
  // Eliminate dupes and 0s
  uids.erase(unique(uids.begin(), uids.end()), uids.end());
  if (uids.front() == 0) uids.erase(uids.begin());
  if (uids.size() == 0) return {};
  uv_scoped_mutex lock(&master_lock);
  while (true) {
    try {
      vector<AsyncLock> locks = _tryLockDatasets(uids);
      if (locks.size() > 0) { return locks; }
    } catch (const char *msg) { throw msg; }
    uv_cond_wait(&master_sleep, &master_lock);
  }
}

/*
 * Acquire the lock only if it is free, do not block
 */
AsyncLock ObjectStore::tryLockDataset(long uid) {
  uv_scoped_mutex lock(&master_lock);
  auto parent = uidMap<GDALDataset *>.find(uid);
  if (parent == uidMap<GDALDataset *>.end()) { throw "Parent Dataset object has already been destroyed"; }
  int r = uv_sem_trywait(parent->second->async_lock.get());
  if (r == 0) return parent->second->async_lock;
  return nullptr;
}

vector<AsyncLock> ObjectStore::_tryLockDatasets(vector<long> uids) {
  vector<AsyncLock> locks;
  for (long uid : uids) {
    auto parent = uidMap<GDALDataset *>.find(uid);
    if (parent == uidMap<GDALDataset *>.end()) { throw "Parent Dataset object has already been destroyed"; }
    locks.push_back(parent->second->async_lock);
  }
  vector<AsyncLock> locked;
  int r = 0;
  for (AsyncLock &async_lock : locks) {
    r = uv_sem_trywait(async_lock.get());
    if (r == 0) {
      locked.push_back(async_lock);
    } else {
      // We failed acquiring one of the locks =>
      // free all acquired locks and start a new cycle
      for (AsyncLock &lock : locked) { uv_sem_post(lock.get()); }
      uv_cond_broadcast(&master_sleep);
      break;
    }
  }
  if (r == 0) return locks;
  return {};
}

/*
 * Try to acquire several locks avoiding deadlocks without blocking
 */
vector<AsyncLock> ObjectStore::tryLockDatasets(vector<long> uids) {
  // There is lots of copying around here but these vectors are never longer than 3 elements
  // Avoid deadlocks
  sort(uids.begin(), uids.end());
  // Eliminate dupes and 0s
  uids.erase(unique(uids.begin(), uids.end()), uids.end());
  if (uids.front() == 0) uids.erase(uids.begin());
  if (uids.size() == 0) return {};
  uv_scoped_mutex lock(&master_lock);
  return _tryLockDatasets(uids);
}

// The basic unit of the ObjectStore is the ObjectStoreItem<GDALPTR>
// There is only one such item per GDALPTR
// There are three shared_ptr to it:
// * one in the uidMap
// * one in the ptrMap
// * and one dynamically allocated on the heap passed to the weakCallback

// Do not forget that the destruction path is two-fold
// * through dispose called from the C++ destructor (when the GC calls Nan::ObjectWrap)
// * through the weakCallback called from the GC (when the GC acts on the ObjectStore Persistent)
// Both will happen and there is no order
// Both will disable further use of the object (removing it from the store)
// Only after both have happened, the ObjectStoreItem<GDALPTR> is destroyed
template <typename GDALPTR> long ObjectStore::add(GDALPTR ptr, const Local<Object> &obj, long parent_uid) {
  LOG("ObjectStore: Add %s [<%ld]", typeid(ptr).name(), parent_uid);
  uv_scoped_mutex lock(&master_lock);
  shared_ptr<ObjectStoreItem<GDALPTR>> item(new ObjectStoreItem<GDALPTR>);
  item->uid = uid++;
  if (parent_uid) {
    shared_ptr<ObjectStoreItem<GDALDataset *>> parent = uidMap<GDALDataset *>[parent_uid];
    item->parent = parent;
    parent->children.push_back(item->uid);
  } else {
    item->parent = nullptr;
  }
  item->ptr = ptr;
  item->obj.Reset(obj);
  // The pointer to a shared_ptr is a necessary evil, SetWeak/WeakCallback take only raw pointers
  shared_ptr<ObjectStoreItem<GDALPTR>> *raw = new shared_ptr<ObjectStoreItem<GDALPTR>>(item);
  item->obj.SetWeak(raw, weakCallback<GDALPTR>, Nan::WeakCallbackType::kParameter);

  uidMap<GDALPTR>[item->uid] = item;
  ptrMap<GDALPTR>[ptr] = item;
  LOG("ObjectStore: Added %s [%ld]", typeid(ptr).name(), item->uid);
  return item->uid;
}

// Creating a Layer object is a special case - it can contain SQL results
long ObjectStore::add(OGRLayer *ptr, const Local<Object> &obj, long parent_uid, bool is_result_set) {
  long uid = ObjectStore::add<OGRLayer *>(ptr, obj, parent_uid);
  uidMap<OGRLayer *>[uid] -> is_result_set = is_result_set;
  return uid;
}

// Creating a Dataset object is a special case
// It contains a lock (unless it is a dependant Dataset)
long ObjectStore::add(GDALDataset *ptr, const Local<Object> &obj, long parent_uid) {
  long uid = ObjectStore::add<GDALDataset *>(ptr, obj, parent_uid);
  if (parent_uid == 0) {
    uidMap<GDALDataset *>[uid] -> async_lock = shared_ptr<uv_sem_t>(new uv_sem_t(), uv_sem_deleter());
    uv_sem_init(uidMap<GDALDataset *>[uid] -> async_lock.get(), 1);
  } else {
    uidMap<GDALDataset *>[uid] -> async_lock = uidMap<GDALDataset *>[parent_uid] -> async_lock;
  }
  return uid;
}

template <typename GDALPTR> bool ObjectStore::has(GDALPTR ptr) {
  return ptrMap<GDALPTR>.count(ptr) > 0;
}
template <typename GDALPTR> Local<Object> ObjectStore::get(GDALPTR ptr) {
  Nan::EscapableHandleScope scope;
  return scope.Escape(Nan::New(ptrMap<GDALPTR>[ptr] -> obj));
}

// Explicit instantiation:
// * allows calling object_store.add without <>
// * makes sure that this class template won't be accidentally instantiated with an unsupported type
template long ObjectStore::add(GDALDriver *, const Local<Object> &, long);
template long ObjectStore::add(GDALRasterBand *, const Local<Object> &, long);
template long ObjectStore::add(OGRSpatialReference *, const Local<Object> &, long);
template bool ObjectStore::has(GDALDriver *);
template bool ObjectStore::has(GDALDataset *);
template bool ObjectStore::has(OGRLayer *);
template bool ObjectStore::has(GDALRasterBand *);
template bool ObjectStore::has(OGRSpatialReference *);
template Local<Object> ObjectStore::get(GDALDriver *);
template Local<Object> ObjectStore::get(GDALDataset *);
template Local<Object> ObjectStore::get(OGRLayer *);
template Local<Object> ObjectStore::get(GDALRasterBand *);
template Local<Object> ObjectStore::get(OGRSpatialReference *);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
template long ObjectStore::add(shared_ptr<GDALAttribute>, const Local<Object> &, long);
template long ObjectStore::add(shared_ptr<GDALDimension>, const Local<Object> &, long);
template long ObjectStore::add(shared_ptr<GDALGroup>, const Local<Object> &, long);
template long ObjectStore::add(shared_ptr<GDALMDArray>, const Local<Object> &, long);
template bool ObjectStore::has(shared_ptr<GDALAttribute>);
template bool ObjectStore::has(shared_ptr<GDALDimension>);
template bool ObjectStore::has(shared_ptr<GDALGroup>);
template bool ObjectStore::has(shared_ptr<GDALMDArray>);
template Local<Object> ObjectStore::get(shared_ptr<GDALAttribute>);
template Local<Object> ObjectStore::get(shared_ptr<GDALDimension>);
template Local<Object> ObjectStore::get(shared_ptr<GDALGroup>);
template Local<Object> ObjectStore::get(shared_ptr<GDALMDArray>);
#endif

// Disposing = called by either the destructor or the GC (WeakCallback)
// Removes the object and all its children for the ObjectStore
// Called twice
//
// Is there a simpler solution with a single code path? It remains to be seen

// Disposing a Dataset is a special case - it has children (called with the master lock held)
template <> void ObjectStore::dispose(shared_ptr<ObjectStoreItem<GDALDataset *>> item) {
  uv_sem_wait(item->async_lock.get());
  uidMap<GDALDataset *>.erase(item->uid);
  ptrMap<GDALDataset *>.erase(item->ptr);
  if (item->parent != nullptr) item->parent->children.remove(item->uid);
  uv_sem_post(item->async_lock.get());
  uv_cond_broadcast(&master_sleep);

  while (!item->children.empty()) { do_dispose(item->children.back()); }
}

// Generic disposal (called with the master lock held)
template <typename GDALPTR> void ObjectStore::dispose(shared_ptr<ObjectStoreItem<GDALPTR>> item) {
  AsyncLock async_lock = nullptr;
  if (item->parent != nullptr) uv_sem_wait(item->parent->async_lock.get());
  ptrMap<GDALPTR>.erase(item->ptr);
  uidMap<GDALPTR>.erase(item->uid);
  if (item->parent != nullptr) {
    item->parent->children.remove(item->uid);
    uv_sem_post(item->parent->async_lock.get());
    uv_cond_broadcast(&master_sleep);
  }
}

// Death by GC
template <typename GDALPTR>
void ObjectStore::weakCallback(const Nan::WeakCallbackInfo<shared_ptr<ObjectStoreItem<GDALPTR>>> &data) {
  shared_ptr<ObjectStoreItem<GDALPTR>> *item = (shared_ptr<ObjectStoreItem<GDALPTR>> *)data.GetParameter();
  LOG("ObjectStore: Death by GC %s [%ld]", typeid((*item)->ptr).name(), (*item)->uid);
  uv_scoped_mutex lock(&object_store.master_lock);
  object_store.dispose(*item);
  delete item; // this is the dynamically allocated shared_ptr
}

// Death by calling dispose from C++ code
void ObjectStore::dispose(long uid) {
  LOG("ObjectStore: Death by calling dispose from C++ [%ld]", uid);
  uv_scoped_mutex lock(&master_lock);
  do_dispose(uid);
}

// The locked section of the above function
void ObjectStore::do_dispose(long uid) {
  if (uidMap<GDALDataset *>.count(uid))
    dispose(uidMap<GDALDataset *>[uid]);
  else if (uidMap<OGRLayer *>.count(uid))
    dispose(uidMap<OGRLayer *>[uid]);
  else if (uidMap<GDALRasterBand *>.count(uid))
    dispose(uidMap<GDALRasterBand *>[uid]);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  else if (uidMap<shared_ptr<GDALGroup>>.count(uid))
    dispose(uidMap<shared_ptr<GDALGroup>>[uid]);
  else if (uidMap<shared_ptr<GDALMDArray>>.count(uid))
    dispose(uidMap<shared_ptr<GDALMDArray>>[uid]);
  else if (uidMap<shared_ptr<GDALDimension>>.count(uid))
    dispose(uidMap<shared_ptr<GDALDimension>>[uid]);
  else if (uidMap<shared_ptr<GDALAttribute>>.count(uid))
    dispose(uidMap<shared_ptr<GDALAttribute>>[uid]);
#endif
}

// Destruction = called after both disposals
// Frees the memory and the resources

// This is the final phase of the tear down
// This is triggered when all 3 shared_ptrs have been destroyed
// The last one will call the class destructor
template <typename GDALPTR> ObjectStoreItem<GDALPTR>::~ObjectStoreItem() {
}

// Closing a Dataset is a special case - it requires a GDAL operation
ObjectStoreItem<GDALDataset *>::~ObjectStoreItem() {
  if (ptr) {
    LOG("Closing GDALDataset %ld [%p]", uid, ptr);
    GDALClose(ptr);
    ptr = nullptr;
  }
}

// Closing a Layer is a special case - it can contain SQL results
ObjectStoreItem<OGRLayer *>::~ObjectStoreItem() {
  GDALDataset *parent_ds = parent->ptr;
  if (is_result_set) {
    LOG("Closing OGRLayer with SQL results %ld [%p]", uid, ptr);
    parent_ds->ReleaseResultSet(ptr);
  }
}

} // namespace node_gdal