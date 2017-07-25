#pragma once

#include "ao/render/brep/xtree.hpp"
#include "ao/render/axes.hpp"

namespace Kernel {

/*
 *  Class to walk a dual grid for a quad or octree
 *  t needs operator(const std::array<XTree<N>*, N>& trees) defined
 */
template <unsigned N>
class Dual
{
public:
    template<typename V>
    static void walk(const XTree<N>* tree, V& v);
};

////////////////////////////////////////////////////////////////////////////////
// 2D Implementation
template <typename V, Axis::Axis A>
void edge2(const std::array<const XTree<2>*, 2>& ts, V& v)
{
    constexpr uint8_t perp = (Axis::X | Axis::Y) ^ A;

    if (std::any_of(ts.begin(), ts.end(),
        [](const XTree<2>* t){ return t->isBranch(); }))
    {
        edge2<V, A>({{ts[0]->child(perp), ts[1]->child(0)}}, v);
        edge2<V, A>({{ts[0]->child(A|perp), ts[1]->child(A)}}, v);
    }
    else if (std::all_of(ts.begin(), ts.end(),
        [](const XTree<2>* t){ return t->type == Interval::AMBIGUOUS &&
                                      !t->isBranch(); }))
    {
        const auto index = std::min_element(ts.begin(), ts.end(),
                [](const XTree<2>* a, const XTree<2>* b)
                { return a->level < b->level; }) - ts.begin();

        constexpr std::array<uint8_t, 2> corners = {{perp, 0}};

        // If there is a sign change across the relevant edge, then call the
        // watcher with the segment corners (with proper winding order)
        auto a = ts[index]->cornerState(corners[index]);
        auto b = ts[index]->cornerState(corners[index] | A);
        if (a != b)
        {
            auto ts_ = ts;
            if ((a == Interval::FILLED && A == Axis::Y) ||
                (b == Interval::FILLED && A == Axis::X))
            {
                std::reverse(ts_.begin(), ts_.end());
            }
            v(ts_);
        }
    }
}

template <>
template <typename V>
void Dual<2>::walk(const XTree<2>* t, V& v)
{
    if (t->isBranch())
    {
        // Recurse down every subface in the quadtree
        for (unsigned i=0; i < t->children.size(); ++i)
        {
            auto c = t->child(i);
            if (c != t)
            {
                walk(c, v);
            }
        }

        //  Then, call edge on every pair of cells
        edge2<V, Axis::Y>({{t->child(0), t->child(Axis::X)}}, v);
        edge2<V, Axis::Y>({{t->child(Axis::Y), t->child(Axis::Y | Axis::X)}}, v);
        edge2<V, Axis::X>({{t->child(0), t->child(Axis::Y)}}, v);
        edge2<V, Axis::X>({{t->child(Axis::X), t->child(Axis::X | Axis::Y)}}, v);
    }
}
////////////////////////////////////////////////////////////////////////////////

template <typename V, Axis::Axis A>
void edge3(const std::array<const XTree<3>*, 4> ts, V& v)
{
    constexpr auto Q = Axis::Q(A);
    constexpr auto R = Axis::R(A);

    if (std::any_of(ts.begin(), ts.end(),
        [](const XTree<3>* t){ return t->isBranch(); }))
    {
        edge3<V, A>({{ts[0]->child(Q|R), ts[1]->child(R), ts[2]->child(Q), ts[3]->child(0)}}, v);
        edge3<V, A>({{ts[0]->child(Q|R|A), ts[1]->child(R|A), ts[2]->child(Q|A), ts[3]->child(A)}}, v);
    }
    else if (std::all_of(ts.begin(), ts.end(),
        [](const XTree<3>* t){ return t->type == Interval::AMBIGUOUS &&
                                      !t->isBranch(); }))
    {
        /*  We need to check the values on the shared edge to see whether we need
         *  to add a face.  However, this is tricky when the edge spans multiple
         *  octree levels.
         *
         * In the following diagram, the target edge is marked with an x
         * (travelling into the screen):
         *      _________________
         *      | a |           |
         *      ----x   c, d    |
         *      | b |           |
         *      ----------------|
         *
         *  If we were to look at corners of c or d, we wouldn't be looking at the
         *  correct edge.  Instead, we need to look at corners for the smallest cell
         *  among the function arguments.
         */
        const auto index = std::min_element(ts.begin(), ts.end(),
                [](const XTree<3>* a, const XTree<3>* b)
                { return a->level < b->level; }) - ts.begin();

        constexpr std::array<uint8_t, 4> corners = {{Q|R, R, Q, 0}};

        // If there is a sign change across the relevant edge, then call the
        // watcher with the segment corners (with proper winding order)
        auto a = ts[index]->cornerState(corners[index]);
        auto b = ts[index]->cornerState(corners[index] | A);
        if (a != b)
        {
            auto ts_ = ts;
            if ((a == Interval::FILLED && A == Axis::X) ||
                (b == Interval::FILLED && A == Axis::Y)) // ???
            {
                std::reverse(ts_.begin(), ts_.end());
            }
            v(ts_);
        }
    }
}

template <typename V, Axis::Axis A>
void face3(const std::array<const XTree<3>*, 2> ts, V& v)
{
    if (std::any_of(ts.begin(), ts.end(),
        [](const XTree<3>* t){ return t->isBranch(); }))
    {
        constexpr auto Q = Axis::Q(A);
        constexpr auto R = Axis::R(A);

        face3<V, A>({{ts[0]->child(A), ts[1]->child(0)}}, v);
        face3<V, A>({{ts[0]->child(Q|A), ts[1]->child(Q)}}, v);
        face3<V, A>({{ts[0]->child(R|A), ts[1]->child(R)}}, v);
        face3<V, A>({{ts[0]->child(Q|R|A), ts[1]->child(Q|R)}}, v);

        edge3<V, Q>({{ts[0]->child(A), ts[0]->child(R|A), ts[1]->child(0), ts[1]->child(R)}}, v);
        edge3<V, Q>({{ts[0]->child(Q|A), ts[0]->child(Q|R|A), ts[1]->child(Q), ts[1]->child(Q|R)}}, v);

        edge3<V, R>({{ts[0]->child(A), ts[1]->child(0), ts[0]->child(A|Q), ts[1]->child(Q)}}, v);
        edge3<V, R>({{ts[0]->child(R|A), ts[1]->child(R), ts[0]->child(R|A|Q), ts[1]->child(R|Q)}}, v);
    }
}

template <>
template <typename V>
void Dual<3>::walk(const XTree<3>* t, V& v)
{
    if (t->isBranch())
    {
        // Recurse, calling the cell procedure for every child
        for (unsigned i=0; i < t->children.size(); ++i)
        {
            auto c = t->child(i);
            if (c != t)
            {
                walk(c, v);
            }
        }

        // Then call the face procedure on every pair of cells
        face3<V, Axis::X>({{t->child(0), t->child(Axis::X)}}, v);
        face3<V, Axis::X>({{t->child(Axis::Y), t->child(Axis::Y | Axis::X)}}, v);
        face3<V, Axis::X>({{t->child(Axis::Z), t->child(Axis::Z | Axis::X)}}, v);
        face3<V, Axis::X>({{t->child(Axis::Y | Axis::Z), t->child(Axis::Y | Axis::Z | Axis::X)}}, v);

        face3<V, Axis::Y>({{t->child(0), t->child(Axis::Y)}}, v);
        face3<V, Axis::Y>({{t->child(Axis::X), t->child(Axis::X | Axis::Y)}}, v);
        face3<V, Axis::Y>({{t->child(Axis::Z), t->child(Axis::Z | Axis::Y)}}, v);
        face3<V, Axis::Y>({{t->child(Axis::X | Axis::Z), t->child(Axis::X | Axis::Z | Axis::Y)}}, v);

        face3<V, Axis::Z>({{t->child(0), t->child(Axis::Z)}}, v);
        face3<V, Axis::Z>({{t->child(Axis::X), t->child(Axis::X | Axis::Z)}}, v);
        face3<V, Axis::Z>({{t->child(Axis::Y), t->child(Axis::Y | Axis::Z)}}, v);
        face3<V, Axis::Z>({{t->child(Axis::X | Axis::Y), t->child(Axis::X | Axis::Y | Axis::Z)}}, v);

        // Call the edge function 6 times
        edge3<V, Axis::Z>({{t->child(0),
             t->child(Axis::X),
             t->child(Axis::Y),
             t->child(Axis::X | Axis::Y)}}, v);
        edge3<V, Axis::Z>({{t->child(Axis::Z),
             t->child(Axis::X | Axis::Z),
             t->child(Axis::Y | Axis::Z),
             t->child(Axis::X | Axis::Y | Axis::Z)}}, v);

        edge3<V, Axis::X>({{t->child(0),
             t->child(Axis::Y),
             t->child(Axis::Z),
             t->child(Axis::Y | Axis::Z)}}, v);
        edge3<V, Axis::X>({{t->child(Axis::X),
             t->child(Axis::Y | Axis::X),
             t->child(Axis::Z | Axis::X),
             t->child(Axis::Y | Axis::Z | Axis::X)}}, v);

        edge3<V, Axis::Y>({{t->child(0),
             t->child(Axis::Z),
             t->child(Axis::X),
             t->child(Axis::Z | Axis::X)}}, v);
        edge3<V, Axis::Y>({{t->child(Axis::Y),
             t->child(Axis::Z | Axis::Y),
             t->child(Axis::X | Axis::Y),
             t->child(Axis::Z | Axis::X | Axis::Y)}}, v);
    }
}

}   // namespace Kernel
