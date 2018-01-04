#ifndef ENTITY_REGISTRY__H
#define ENTITY_REGISTRY__H

#include "defs.h"
#include "memory_pool.h"
#include "component_registry.h"
#include "sparse_set.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class EntityRegistry {
 public:
  struct CreateEntityEvent {
    EntityRegistry* self;
    qbId entity;
  };

  struct DestroyEntityEvent {
    EntityRegistry* self;
    qbId entity;
  };

  EntityRegistry(ComponentRegistry* component_registry);

  void Init();

  // Creates an entity. Entity will be available for use next frame. Sends a
  // ComponentCreateEvent after all components have been created.
  qbResult CreateEntity(qbEntity* entity, const qbEntityAttr_& attr);

  // Destroys an entity and frees all components. Entity and components will be
  // destroyed next frame. Sends a ComponentDestroyEvent before components are
  // removed. Frees entity memory after all components have been destroyed.
  qbResult DestroyEntity(qbEntity* entity);

  qbResult AddComponent(qbId entity, qbComponent component,
                        void* instance_data);

  qbResult RemoveComponent(qbId entity, qbComponent component);

  qbResult Find(qbEntity* entity, qbId entity_id);

 private:
  struct EntityElement {
    qbEntity_ entity;
    qbInstance_ instances[5];
  };
  qbId AllocEntity();

  qbResult SendDestroyEntityEvent(qbId entity);
  
  qbResult SendRemoveComponentEvent(qbId entity, qbComponent component);

  static void CreateEntityHandler(qbFrame* frame);

  static void DestroyEntityHandler(qbFrame* frame);

  std::atomic_long id_;
  
  ComponentRegistry* component_registry_;

  SparseSet entities_;
  SparseSet destroyed_entities_;
  std::vector<size_t> free_entity_ids_;

  qbEvent add_component_event_;
  qbEvent create_entity_event_;
  qbEvent destroy_entity_event_;

  qbSystem add_component_system_;
  qbSystem create_entity_system_;
  qbSystem destroy_entity_system_;

  std::mutex destroy_entity_mu_;
};

#endif  // ENTITY_REGISTRY__H

