#pragma once

#include "type_traits.h"
#include <stdexcept>
#include <string>
#include <sstream>
#include <typeinfo>
#include <set>

namespace mms {
namespace impl {

template<class TMM, bool HasFormatVersionEx, bool HasFormatVersion, bool HasTraverseFields>
struct FormatVersionHelper;

template<class TSA>
class MetaFormatVersionHelper {
private:
    typedef typename MmappedType<TSA>::type TMM;
public:
    typedef FormatVersionHelper<
        TMM,
        sizeof(hasFormatVersionEx<TMM>(0)) == sizeof(Yes),
        sizeof(hasFormatVersion<TMM>(0)) == sizeof(Yes),
        HasTraverseFields<TMM>::value
    > type;
};

} // namespace impl

class Versions {
public:
    typedef FormatVersion Ver;

    template<class T>
    Ver get()
    {
        static const Ver RECURSIVE = hash("recursive");
        const std::type_info* ti = &typeid(T);
        std::pair<SeenSet::iterator, bool> pair = seen_.insert(ti);
        if (pair.second) {
            Ver result = impl::MetaFormatVersionHelper<T>::type::version(*this);
            seen_.erase(pair.first);
            return result;
        } else {
            return RECURSIVE;
        }
    }

    Ver combine(Ver lhs, Ver rhs)
    {
        static const size_t FORMAT_POLYNOMIAL_COEF = 478278233;
        return lhs * FORMAT_POLYNOMIAL_COEF + rhs;
    }

    Ver combine(Ver a1, Ver a2, Ver a3)
    {
        return combine(a1, combine(a2, a3));
    }

    static Ver hash(const char* str)
    {
        // Stolen from boost::hash_combine()
        size_t h = 0;
        for (; *str; ++str)
            h ^= static_cast<size_t>(*str) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }

    template<class T>
    Ver dependent(const char* salt) { return combine(get<T>(), hash(salt)); }

    template<class T1, class T2>
    Ver dependent(const char* salt) { return combine(get<T1>(), get<T2>(), hash(salt)); }

private:
    typedef std::set<const std::type_info*> SeenSet;
    SeenSet seen_;
};

namespace impl {

template <class T>
FormatVersion formatVersion()
{
    Versions vs;
    return vs.get<T>();
}

// Case 1. Classes having a 'static FormatVersion formatVersion(Versions&)'.
//         Utilize the method to obtain a version modifier and inject
//         it to the version calculated using cases 3 or 4.
template<class TMM, bool HasFormatVersion, bool HasTraverseFields>
struct FormatVersionHelper<TMM, true, HasFormatVersion, HasTraverseFields> {
    static FormatVersion version(Versions& vs)
    {
        return vs.combine(TMM::formatVersion(vs),
            FormatVersionHelper<TMM, false, false, HasTraverseFields>::version(vs));
    }
};

// Case 2. Classes having a 'static FormatVersion formatVersion()'.
//         Same as above.
template<class TMM, bool HasTraverseFields>
struct FormatVersionHelper<TMM, false, true, HasTraverseFields> {
    static FormatVersion version(Versions& vs)
    {
        return vs.combine(TMM::formatVersion(),
            FormatVersionHelper<TMM, false, false, HasTraverseFields>::version(vs));
    }
};

// Case 3. Classes not having any formatVersion(), but having traverseFields().
//         Find out versions of all fields and combine them.
template<class TMM>
struct FormatVersionHelper<TMM, false, false, true> {
    class FormatVersionCalculator {
    public:
        FormatVersionCalculator(Versions& vs, FormatVersion& version):
            vs_(&vs), version_(version)
        {}

        template<class U>
        void operator()(const U&)
        {
            version_ = vs_->combine(version_, vs_->get<U>());
        }

    private:
        Versions* vs_;
        FormatVersion& version_;
    };

    static FormatVersion version(Versions& vs)
    {
        const TMM* tmm = reinterpret_cast<const TMM*>(0);
        FormatVersion version = 0;
        traverseFields(*tmm, ActionFacade<FormatVersionCalculator>(
            FormatVersionCalculator(vs, version)));
        return vs.combine(version, FormatVersionHelper<TMM, false, false, false>::version(vs));
    }
};

// Case 4. Classes not having anything of the above.
//         Just use the class name and size.
template<class TMM>
struct FormatVersionHelper<TMM, false, false, false> {
    static FormatVersion version(Versions& vs)
    {
        if (mms::type_traits::is_trivial<TMM>::value) {
            return vs.combine(vs.hash(typeid(TMM).name()), sizeof(TMM));
        } else {
            return 0; // FIXME: better fail here, but will break existing data
        }
    }
};

template<class TSA>
void checkFormatVersion(const TSA&, FormatVersion writtenVersion)
{
    FormatVersion actualVersion = Versions().get<TSA>();
    if (actualVersion != writtenVersion) {
        std::ostringstream msg;
        msg << "wrong version for type " << typeid(TSA).name()
            << " (expected " << actualVersion
            << ", encountered " << writtenVersion << ")";
        throw std::runtime_error(msg.str());
    }
}

} // namespace impl

} // namespace mms
