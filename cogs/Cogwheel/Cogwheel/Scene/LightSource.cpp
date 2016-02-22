// Cogwheel light source.
// ---------------------------------------------------------------------------
// Copyright (C) 2015-2016, Cogwheel. See AUTHORS.txt for authors
//
// This program is open source and distributed under the New BSD License. See
// LICENSE.txt for more detail.
// ---------------------------------------------------------------------------

#include <Cogwheel/Scene/LightSource.h>

using namespace Cogwheel::Math;

namespace Cogwheel {
namespace Scene {

LightSources::UIDGenerator LightSources::m_UID_generator = UIDGenerator(0u);

SceneNodes::UID* LightSources::m_node_IDs = nullptr;
Vector3f* LightSources::m_power = nullptr;

std::vector<LightSources::UID> LightSources::m_lights_created = std::vector<LightSources::UID>(0);
std::vector<LightSources::UID> LightSources::m_lights_destroyed = std::vector<LightSources::UID>(0);

void LightSources::allocate(unsigned int capacity) {
    if (is_allocated())
        return;

    m_UID_generator = UIDGenerator(capacity);
    capacity = m_UID_generator.capacity();

    m_node_IDs = new SceneNodes::UID[capacity];
    m_power = new Vector3f[capacity];
    
    m_lights_created.reserve(capacity / 4);
    m_lights_destroyed.reserve(capacity / 4);
    
    // Allocate dummy element at 0.
    m_node_IDs[0] = SceneNodes::UID::invalid_UID();
    m_power[0] = Vector3f::zero();
}

void LightSources::deallocate() {
    if (!is_allocated())
        return;

    m_UID_generator = UIDGenerator(0u);

    delete[] m_node_IDs; m_node_IDs = nullptr;
    delete[] m_power; m_power = nullptr;
    
    m_lights_created.resize(0); m_lights_created.shrink_to_fit();
    m_lights_destroyed.resize(0); m_lights_destroyed.shrink_to_fit();
}

template <typename T>
static inline T* resize_and_copy_array(T* old_array, unsigned int new_capacity, unsigned int copyable_elements) {
    T* new_array = new T[new_capacity];
    std::copy(old_array, old_array + copyable_elements, new_array);
    delete[] old_array;
    return new_array;
}

void LightSources::reserve_light_data(unsigned int new_capacity, unsigned int old_capacity) {
    assert(m_node_IDs != nullptr);
    assert(m_power != nullptr);

    const unsigned int copyable_elements = new_capacity < old_capacity ? new_capacity : old_capacity;

    m_node_IDs = resize_and_copy_array(m_node_IDs, new_capacity, copyable_elements);
    m_power = resize_and_copy_array(m_power, new_capacity, copyable_elements);
}

void LightSources::reserve(unsigned int new_capacity) {
    unsigned int old_capacity = capacity();
    m_UID_generator.reserve(new_capacity);
    reserve_light_data(m_UID_generator.capacity(), old_capacity);
}

LightSources::UID LightSources::create_point_light(SceneNodes::UID node_ID, Math::Vector3f power) {
    assert(m_node_IDs != nullptr);
    assert(m_power != nullptr);

    unsigned int old_capacity = m_UID_generator.capacity();
    UID id = m_UID_generator.generate();
    if (old_capacity != m_UID_generator.capacity())
        // The capacity has changed and the size of all arrays need to be adjusted.
        reserve_light_data(m_UID_generator.capacity(), old_capacity);

    m_node_IDs[id] = node_ID;
    m_power[id] = power;

    m_lights_created.push_back(id);

    return id;
}

void LightSources::destroy(LightSources::UID light_ID) {
    // We don't actually destroy anything when destroying a light. The properties will get overwritten later when a node is created in same the spot.
    if (m_UID_generator.erase(light_ID))
        m_lights_destroyed.push_back(light_ID);
}

void LightSources::reset_change_notifications() {
    m_lights_created.resize(0);
    m_lights_destroyed.resize(0);
}

} // NS Scene
} // NS Cogwheel