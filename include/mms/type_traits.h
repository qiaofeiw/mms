
/*
 * mms/type_traits.h -- a couple of checks for various member functions
 *                      needed by mms
 *
 * Copyright (c) 2011-2014 Dmitry Prokoptsev <dprokoptsev@yandex-team.ru>
 *
 * This file is part of mms, the memory-mapped storage library.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#pragma once

#include "impl/defs.h"
#include "impl/tags.h"
#include "impl/fwd.h"
#include <functional>
#include <utility>

namespace mms {

class Versions;

namespace impl {

struct Yes {};
struct No { Yes _[2]; };
template<class T, T> struct Check;

namespace {
    struct FakeWriter: public WriterBase {
        void write(const void*, size_t) {}
    };
}

template<class T>
Yes hasWriters(
    Check<size_t (T::*)(FakeWriter&) const,         &T::writeData  >*,
    Check<size_t (T::*)(FakeWriter&, size_t) const, &T::writeField >*
);
template<class T>
No hasWriters(...);

template <class Action>
class ActionFacade {
public:
    explicit ActionFacade(Action action) : action_(action) {}

#if MMS_USE_CXX11
    template <class H, class... T>
    ActionFacade<Action>& operator()(H &h, T&... ts) {
        action_(h);
        return this->operator()(ts...);
    }

    template <class H, class... T>
    ActionFacade<Action>& operator()(
            const FieldDescriptor<H> &desc, T&... ts) {
        action_(*desc.object);
        return this->operator()(ts...);
    }
#endif

    template <class H>
    ActionFacade<Action>& operator()(H &h) {
        action_(h);
        return *this;
    }

    template <class H>
    ActionFacade<Action>& operator()(const FieldDescriptor<H>& desc) {
        action_(*desc.object);
        return *this;
    }

private:
    Action action_;
};

class EmptyCallback {
public:
    template<class U>
    void operator()(const U&) { }
};

template<class T>
Yes _hasTraverseFields(
    Check<void (T::*)(ActionFacade<EmptyCallback>) const,
                &T::traverseFields>*
);
template<class T>
No _hasTraverseFields(...);

template<class T>
struct HasTraverseFields {
    static const bool value =
        sizeof(_hasTraverseFields<T>(0)) == sizeof(Yes);
};

template<class X, class Y>
struct HasTraverseFields< std::pair<X,Y> > {
    static const bool value = true;
};

template<class T>
Yes hasMmappedType(typename T::MmappedType*);
template<class T>
No hasMmappedType(...);

template < class T > struct MmappedType;
template < class T, bool UseDefined > struct MmappedTypeAux;

template < class T >
struct MmappedType {
    typedef typename mms::type_traits::remove_cv<T>::type UnCv;
    typedef typename MmappedTypeAux<
        UnCv,
        sizeof(hasMmappedType<T>(0)) == sizeof(Yes)
    >::type type;
};

template<class T>
Yes hasEnforceVersion(
    Check<FormatVersion (*)(Versions&), &MmappedType<T>::type::enforceVersion>*
);
template<class T>
No hasEnforceVersion(...);

template<class T>
Yes hasFormatVersion(
    Check<FormatVersion (*)(), &MmappedType<T>::type::formatVersion>*
);
template<class T>
No hasFormatVersion(...);

template<class T>
Yes hasFormatVersionEx( // Yeah!
    Check<FormatVersion (*)(Versions&), &MmappedType<T>::type::formatVersion>*
);
template<class T>
No hasFormatVersionEx(...);

template < class T>
struct MmappedTypeAux<T, true>
{
    typedef typename T::MmappedType type;
};

template < class T>
struct MmappedTypeAux<T, false>
{
    /*
     * If you got the 'no type named "type"...' error at this point,
     * you won the prize^W^W^W have the class which is neither
     * a trivial type nor a template having 'mms::Standalone' as its single
     * parameter, and mms has no idea how your class will fit in
     * memory.
     */
    typedef typename mms::type_traits::enable_if<
        mms::type_traits::is_trivial<T>::value, T
    >::type type;
};

template < template<class> class T>
struct MmappedTypeAux< T<Standalone>, false >
{
    typedef T<Mmapped> type;
};

template < template<class> class T>
struct MmappedTypeAux< T<Mmapped>, false >
{
    typedef T<Mmapped> type;
};

template <class X, class Y>
struct MmappedTypeAux<std::pair<X, Y>, false>
{
    typedef std::pair<
        typename MmappedType<X>::type,
        typename MmappedType<Y>::type
    > type;
};

/*
 * If you got the "...has no member named 'traverseFields'"
 * error at this point, you forgot to enumerate the fields
 * in your class.
 *
 * Add the following to your class:
 *
 *   template<class A> 
 *   void traverseFields(A a)
 *   {
 *       a(field1)(field2); // and so on
 *   }
 */
template<class T, class A>
void traverseFields(const T& t, A a) { t.traverseFields(a); }

} //namespace impl

using impl::MmappedType;

} //namespace mms
