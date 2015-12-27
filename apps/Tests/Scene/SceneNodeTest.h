// Test Cogwheel Scene Nodes.
// ---------------------------------------------------------------------------
// Copyright (C) 2015, Cogwheel. See AUTHORS.txt for authors
//
// This program is open source and distributed under the New BSD License. See
// LICENSE.txt for more detail.
// ---------------------------------------------------------------------------

#ifndef _COGWHEEL_SCENE_SCENE_NODE_TEST_H_
#define _COGWHEEL_SCENE_SCENE_NODE_TEST_H_

#include <Scene/SceneNode.h>

#include <gtest/gtest.h>

namespace Cogwheel {
namespace Scene {

GTEST_TEST(Scene_SceneNode, resizing) {
    SceneNodes::allocate(8u);
    EXPECT_GE(SceneNodes::capacity(), 8u);

    // Test that capacity can be increased.
    unsigned int larger_capacity = SceneNodes::capacity() + 4u;
    SceneNodes::reserve(larger_capacity);
    EXPECT_GE(SceneNodes::capacity(), larger_capacity);

    // Test that capacity won't be decreased.
    SceneNodes::reserve(5u);
    EXPECT_GE(SceneNodes::capacity(), larger_capacity);

    SceneNodes::deallocate();
    EXPECT_LT(SceneNodes::capacity(), larger_capacity);
}

GTEST_TEST(Scene_SceneNode, creating) {
    SceneNodes::allocate(2u);
    SceneNodes::UID id = SceneNodes::create("Foo");
    EXPECT_TRUE(SceneNodes::has(id));
    
    EXPECT_EQ(SceneNodes::get_name(id), "Foo");

    SceneNodes::deallocate();
}

GTEST_TEST(Scene_SceneNode, sentinel_node) {
    SceneNodes::allocate(1u);

    SceneNode sentinel = SceneNodes::UID::invalid_UID();
    SceneNode node = SceneNodes::create("Foo");

    // Test that sentinel node cannot have it's parent set.
    sentinel.set_parent(node);
    SceneNode parent_set = sentinel.get_parent();
    EXPECT_NE(parent_set, node);
    EXPECT_EQ(parent_set, sentinel);

    EXPECT_EQ(sentinel.get_children().size(), 0u);

    SceneNodes::deallocate();
}

GTEST_TEST(Scene_SceneNode, parenting) {
    SceneNodes::allocate(1u);
    SceneNode n0 = SceneNodes::create("n0");
    SceneNode n1 = SceneNodes::create("n1");
    SceneNode n2 = SceneNodes::create("n2");

    EXPECT_FALSE(n1.get_parent().exists());
    
    // Set n0 to the parent and check the parent-child relationship.
    n1.set_parent(n0);
    EXPECT_TRUE(n1.get_parent().exists());
    EXPECT_EQ(n1.get_parent(), n0);
    EXPECT_EQ(n0.get_children().size(), 1u);
    EXPECT_TRUE(n0.has_child(n1));

    // Set n2 to the parent and check the parent-child relationship.
    n1.set_parent(n2);
    EXPECT_TRUE(n1.get_parent().exists());
    EXPECT_EQ(n1.get_parent(), n2);
    EXPECT_EQ(n2.get_children().size(), 1u);
    EXPECT_TRUE(n2.has_child(n1));
    // ... also check that n0 no longer has any children.
    EXPECT_EQ(0u, n0.get_children().size());
    
    SceneNodes::deallocate();
}

GTEST_TEST(Scene_SceneNode, creating_hierarchy) {
    // Tests the following hierachy
    //      id3
    //    /  |  \
    // id0  id4  id6
    //      / \    \
    //    id2 id5  id1
    SceneNodes::allocate(1u);
    SceneNode n0 = SceneNodes::create("n0");
    SceneNode n1 = SceneNodes::create("n1");
    SceneNode n2 = SceneNodes::create("n2");
    SceneNode n3 = SceneNodes::create("n3");
    SceneNode n4 = SceneNodes::create("n4");
    SceneNode n5 = SceneNodes::create("n5");
    SceneNode n6 = SceneNodes::create("n6");
    
    EXPECT_TRUE(n0.exists());
    EXPECT_TRUE(n1.exists());
    EXPECT_TRUE(n2.exists());
    EXPECT_TRUE(n3.exists());
    EXPECT_TRUE(n4.exists());
    EXPECT_TRUE(n5.exists());
    EXPECT_TRUE(n6.exists());

    n0.set_parent(n3);
    n4.set_parent(n3);
    n6.set_parent(n3);
    n2.set_parent(n4);
    n5.set_parent(n4);
    n1.set_parent(n6);

    EXPECT_EQ(n0.get_parent(), n3);
    EXPECT_TRUE(n3.has_child(n0));
    EXPECT_EQ(n4.get_parent(), n3);
    EXPECT_TRUE(n3.has_child(n4));
    EXPECT_EQ(n6.get_parent(), n3);
    EXPECT_TRUE(n3.has_child(n6));
    EXPECT_EQ(n2.get_parent(), n4);
    EXPECT_TRUE(n4.has_child(n2));
    EXPECT_EQ(n5.get_parent(), n4);
    EXPECT_TRUE(n4.has_child(n5));
    EXPECT_EQ(n1.get_parent(), n6);
    EXPECT_TRUE(n6.has_child(n1));

    // Now parent id4 below id0, just for fun and profit.
    //     n3
    //    /  \
    //   n0  n6
    //   |    |
    //   n4  n1
    //  / \
    // n2 n5
    n4.set_parent(n0);

    EXPECT_FALSE(n3.get_parent().exists());
    EXPECT_EQ(n0.get_parent(), n3);
    EXPECT_TRUE(n3.has_child(n0));
    EXPECT_EQ(n6.get_parent(), n3);
    EXPECT_TRUE(n3.has_child(n6));
    EXPECT_EQ(n4.get_parent(), n0);
    EXPECT_TRUE(n0.has_child(n4));
    EXPECT_EQ(n1.get_parent(), n6);
    EXPECT_TRUE(n6.has_child(n1));
    EXPECT_EQ(n2.get_parent(), n4);
    EXPECT_TRUE(n4.has_child(n2));
    EXPECT_EQ(n5.get_parent(), n4);
    EXPECT_TRUE(n4.has_child(n5));

    SceneNodes::deallocate();
}

GTEST_TEST(Scene_SceneNode, grap_traversal) {
    // Tests the following hierachy
    //      id3
    //    /  |  \
    // id0  id4  id6
    //      / \    \
    //    id2 id5  id1
    SceneNodes::allocate(1u);
    SceneNode n0 = SceneNodes::create("n0");
    SceneNode n1 = SceneNodes::create("n1");
    SceneNode n2 = SceneNodes::create("n2");
    SceneNode n3 = SceneNodes::create("n3");
    SceneNode n4 = SceneNodes::create("n4");
    SceneNode n5 = SceneNodes::create("n5");
    SceneNode n6 = SceneNodes::create("n6");

    n0.set_parent(n3);
    n4.set_parent(n3);
    n6.set_parent(n3);
    n2.set_parent(n4);
    n5.set_parent(n4);
    n1.set_parent(n6);

    unsigned int* visits = new unsigned int[SceneNodes::capacity()];
    for (unsigned int i = 0; i < SceneNodes::capacity(); ++i)
        visits[i] = 0u;
    n2.traverse_all_children([&](SceneNodes::UID id) {
        ++visits[id];
    });
    EXPECT_EQ(visits[n0.get_ID()], 0u);
    EXPECT_EQ(visits[n1.get_ID()], 0u);
    EXPECT_EQ(visits[n2.get_ID()], 0u);
    EXPECT_EQ(visits[n3.get_ID()], 0u);
    EXPECT_EQ(visits[n4.get_ID()], 0u);
    EXPECT_EQ(visits[n5.get_ID()], 0u);
    EXPECT_EQ(visits[n6.get_ID()], 0u);

    n4.traverse_graph([&](SceneNodes::UID id) {
        ++visits[id];
    });
    EXPECT_EQ(visits[n0.get_ID()], 0u);
    EXPECT_EQ(visits[n1.get_ID()], 0u);
    EXPECT_EQ(visits[n2.get_ID()], 1u);
    EXPECT_EQ(visits[n3.get_ID()], 0u);
    EXPECT_EQ(visits[n4.get_ID()], 1u);
    EXPECT_EQ(visits[n5.get_ID()], 1u);
    EXPECT_EQ(visits[n6.get_ID()], 0u);

    n6.traverse_all_children([&](SceneNodes::UID id) {
        ++visits[id];
    });
    EXPECT_EQ(visits[n0.get_ID()], 0u);
    EXPECT_EQ(visits[n1.get_ID()], 1u);
    EXPECT_EQ(visits[n2.get_ID()], 1u);
    EXPECT_EQ(visits[n3.get_ID()], 0u);
    EXPECT_EQ(visits[n4.get_ID()], 1u);
    EXPECT_EQ(visits[n5.get_ID()], 1u);
    EXPECT_EQ(visits[n6.get_ID()], 0u);

    n3.traverse_graph([&](SceneNodes::UID id) {
        ++visits[id];
    });
    EXPECT_EQ(visits[n0.get_ID()], 1u);
    EXPECT_EQ(visits[n1.get_ID()], 2u);
    EXPECT_EQ(visits[n2.get_ID()], 2u);
    EXPECT_EQ(visits[n3.get_ID()], 1u);
    EXPECT_EQ(visits[n4.get_ID()], 2u);
    EXPECT_EQ(visits[n5.get_ID()], 2u);
    EXPECT_EQ(visits[n6.get_ID()], 1u);

    SceneNodes::deallocate();
}

} // NS Scene
} // NS Cogwheel

#endif // _COGWHEEL_SCENE_SCENE_NODE_TEST_H_