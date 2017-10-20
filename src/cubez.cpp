#include "cubez.h"
#include "defs.h"
#include "private_universe.h"
#include "byte_vector.h"
#include "component.h"

#define AS_PRIVATE(expr) ((PrivateUniverse*)(universe_->self))->expr

static qbUniverse* universe_ = nullptr;

qbResult qb_init(qbUniverse* u) {
  universe_ = u;
  universe_->self = new PrivateUniverse();

  return AS_PRIVATE(init());
}

qbResult qb_start() {
  return AS_PRIVATE(start());
}

qbResult qb_stop() {
  qbResult ret = AS_PRIVATE(stop());
  universe_ = nullptr;
  return ret;
}

qbResult qb_loop() {
  return AS_PRIVATE(loop());
}

qbId qb_create_program(const char* name) {
  return AS_PRIVATE(create_program(name));
}

qbResult qb_run_program(qbId program) {
  return AS_PRIVATE(run_program(program));
}

qbResult qb_detach_program(qbId program) {
  return AS_PRIVATE(detach_program(program));
}

qbResult qb_join_program(qbId program) {
  return AS_PRIVATE(join_program(program));
}

qbResult qb_system_enable(qbSystem system) {
  return AS_PRIVATE(enable_system(system));
}

qbResult qb_system_disable(qbSystem system) {
  return AS_PRIVATE(disable_system(system));
}

qbResult qb_componentattr_create(qbComponentAttr* attr) {
  *attr = (qbComponentAttr)calloc(1, sizeof(qbComponentAttr_));
	return qbResult::QB_OK;
}

qbResult qb_componentattr_destroy(qbComponentAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setprogram(qbComponentAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size) {
  attr->data_size = size;
	return qbResult::QB_OK;
}

qbResult qb_component_create(
    qbComponent* component, qbComponentAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
  return AS_PRIVATE(component_create(component, attr));
}


qbResult qb_component_destroy(qbComponent*) {
	return qbResult::QB_OK;
}


qbResult qb_entityattr_create(qbEntityAttr* attr) {
  *attr = (qbEntityAttr)calloc(1, sizeof(qbEntityAttr_));
	return qbResult::QB_OK;
}

qbResult qb_entityattr_destroy(qbEntityAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_entityattr_addcomponent(qbEntityAttr attr, qbComponent component,
                                    void* instance_data) {
  attr->component_list.push_back({component, instance_data});
	return qbResult::QB_OK;
}

qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr) {
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(!attr->component_list.empty(),
               QB_ERROR_ENTITYATTR_COMPONENTS_ARE_EMPTY);
#endif
  return AS_PRIVATE(entity_create(entity, *attr));
}

qbResult qb_entity_destroy(qbEntity*) {
	return qbResult::QB_OK;
}

qbId qb_entity_getid(qbEntity entity) {
  return entity->id;
}

qbResult qb_systemattr_create(qbSystemAttr* attr) {
  *attr = (qbSystemAttr)calloc(1, sizeof(qbSystemAttr_));
	return qbResult::QB_OK;
}

qbResult qb_systemattr_destroy(qbSystemAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setprogram(qbSystemAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addsource(qbSystemAttr attr, qbComponent component) {
  attr->sources.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addsink(qbSystemAttr attr, qbComponent component) {
  attr->sinks.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setfunction(qbSystemAttr attr, qbTransform transform) {
  attr->transform = transform;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setcallback(qbSystemAttr attr, qbCallback callback) {
  attr->callback = callback;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_settrigger(qbSystemAttr attr, qbTrigger trigger) {
  attr->trigger = trigger;
	return qbResult::QB_OK;
}


qbResult qb_systemattr_setpriority(qbSystemAttr attr, int16_t priority) {
  attr->priority = priority;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setjoin(qbSystemAttr attr, qbComponentJoin join) {
  attr->join = join;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setuserstate(qbSystemAttr attr, void* state) {
  attr->state = state;
	return qbResult::QB_OK;
}


qbResult qb_system_create(qbSystem* system, qbSystemAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->transform || attr->callback,
               qbResult::QB_ERROR_SYSTEMATTR_HAS_FUNCTION_OR_CALLBACK);
#endif
  AS_PRIVATE(system_create(system, *attr));
	return qbResult::QB_OK;
}

qbResult qb_system_destroy(qbSystem*) {
	return qbResult::QB_OK;
}


qbResult qb_collectionattr_create(qbCollectionAttr* attr) {
  (*attr) = (qbCollectionAttr)calloc(1, sizeof(qbCollectionAttr_));
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_destroy(qbCollectionAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setimplementation(qbCollectionAttr attr, void* impl) {
  attr->collection = impl;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setprogram(qbCollectionAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setaccessors(qbCollectionAttr attr, qbValueByOffset by_offset,
                                        qbValueById by_id, qbValueByHandle by_handle) {
  attr->accessor.offset = by_offset;
  attr->accessor.id = by_id;
  attr->accessor.handle = by_handle;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setkeyiterator(qbCollectionAttr attr, qbData data,
                                          size_t stride, uint32_t offset) {
  attr->keys.data = data;
  attr->keys.stride = stride;
  attr->keys.offset = offset;
  attr->keys.size = sizeof(qbId);
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setvalueiterator(qbCollectionAttr attr, qbData data,
                                            size_t size, size_t stride,
                                            uint32_t offset) {
  attr->values.data = data;
  attr->values.stride = stride;
  attr->values.offset = offset;
  attr->values.size = size;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setinsert(qbCollectionAttr attr, qbInsert insert) {
  attr->insert = insert;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setcount(qbCollectionAttr attr, qbCount count) {
  attr->count = count;
	return qbResult::QB_OK;
}


qbResult qb_collection_create(qbCollection* collection, qbCollectionAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->accessor.offset,
               QB_ERROR_COLLECTIONATTR_ACCESSOR_OFFSET_IS_NOT_SET);
  DEBUG_ASSERT(attr->accessor.handle,
               QB_ERROR_COLLECTIONATTR_ACCESSOR_HANDLE_IS_NOT_SET);
  DEBUG_ASSERT(attr->keys.data,
               QB_ERROR_COLLECTIONATTR_KEYITERATOR_DATA_IS_NOT_SET);
  DEBUG_ASSERT(attr->keys.stride > 0,
               QB_ERROR_COLLECTIONATTR_KEYITERATOR_STRIDE_IS_NOT_SET);
  DEBUG_ASSERT(attr->values.data,
               QB_ERROR_COLLECTIONATTR_VALUEITERATOR_DATA_IS_NOT_SET);
  DEBUG_ASSERT(attr->values.stride > 0,
               QB_ERROR_COLLECTIONATTR_VALUEITERATOR_STRIDE_IS_NOT_SET);
  DEBUG_ASSERT(attr->insert, QB_ERROR_COLLECTIONATTR_INSERT_IS_NOT_SET);
  DEBUG_ASSERT(attr->count, QB_ERROR_COLLECTIONATTR_COUNT_IS_NOT_SET);
  DEBUG_ASSERT(attr->collection,
               QB_ERROR_COLLECTIONATTR_IMPLEMENTATION_IS_NOT_SET);
#endif
  return AS_PRIVATE(collection_create(collection, attr));
}

qbResult qb_collection_share(qbCollection collection, qbProgram destination) {
	return AS_PRIVATE(collection_share(collection, destination));
}

qbResult qb_collection_destroy(qbCollection* collection) {
	return AS_PRIVATE(collection_destroy(collection));
}

qbResult qb_eventattr_create(qbEventAttr* attr) {
  *attr = (qbEventAttr)calloc(1, sizeof(qbEventAttr_));
	return qbResult::QB_OK;
}

qbResult qb_eventattr_destroy(qbEventAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setprogram(qbEventAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setmessagesize(qbEventAttr attr, size_t size) {
  attr->message_size = size;
	return qbResult::QB_OK;
}

qbResult qb_event_create(qbEvent* event, qbEventAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->message_size > 0,
               qbResult::QB_ERROR_EVENTATTR_MESSAGE_SIZE_IS_ZERO);
#endif
	return AS_PRIVATE(event_create(event, attr));
}

qbResult qb_event_destroy(qbEvent* event) {
	return AS_PRIVATE(event_destroy(event));
}

qbResult qb_event_flush(qbEvent event) {
	return AS_PRIVATE(event_flush(event));
}

qbResult qb_event_flushall(qbProgram program) {
	return AS_PRIVATE(event_flushall(program));
}

qbResult qb_event_subscribe(qbEvent event, qbSystem system) {
	return AS_PRIVATE(event_subscribe(event, system));
}

qbResult qb_event_unsubscribe(qbEvent event, qbSystem system) {
	return AS_PRIVATE(event_unsubscribe(event, system));
}

qbResult qb_event_send(qbEvent event, void* message) {
  return AS_PRIVATE(event_send(event, message));
}

qbResult qb_event_sendsync(qbEvent event, void* message) {
  return AS_PRIVATE(event_sendsync(event, message));
}

qbId qb_element_getid(qbElement element) {
  return element->id;
}

qbResult qb_element_read(qbElement element, void* buffer) {
  memmove(buffer,
          element->read_buffer,
          element->size);
  element->user_buffer = buffer;
  return QB_OK;
}

qbResult qb_element_write(qbElement element) {
  switch(element->indexed_by) {
    case QB_INDEXEDBY_KEY:
      memmove(element->interface.by_id(&element->interface, element->id),
              element->user_buffer, element->size);
      break;
    case QB_INDEXEDBY_OFFSET:
      memmove(element->interface.by_offset(&element->interface, element->offset),
              element->user_buffer, element->size);
      break;
    case QB_INDEXEDBY_HANDLE:
      memmove(element->interface.by_handle(&element->interface, element->handle),
              element->user_buffer, element->size);
      break;
  }
  return QB_OK;
}
