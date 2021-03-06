//  Copyright (c) 2015 Anton Bikineev
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <phylanx/config.hpp>
#include <phylanx/util/serialization/variant.hpp>

#include <hpx/hpx_main.hpp>
#include <hpx/include/serialization.hpp>
#include <hpx/modules/testing.hpp>

#include <string>
#include <vector>

template <typename T>
struct A
{
    A() {}

    A(T t) : t_(t) {}
    T t_;

    A & operator=(T t) { t_ = t; return *this; }

    friend bool operator==(A a, A b)
    {
        return a.t_ == b.t_;
    }

    friend std::ostream& operator<<(std::ostream& os, A a)
    {
        os << a.t_;
        return os;
    }

    template <typename Archive>
    void serialize(Archive & ar, unsigned)
    {
        ar & t_;
    }
};

int main()
{
    std::vector<char> buf;
    hpx::serialization::output_archive oar(buf);
    hpx::serialization::input_archive iar(buf);

    phylanx::util::variant<int, std::string, double, A<int> > ovar =
        std::string("dfsdf");
    phylanx::util::variant<int, std::string, double, A<int> > ivar;
    oar << ovar;
    iar >> ivar;

    HPX_TEST_EQ(ivar.index(), ovar.index());
    HPX_TEST(ivar == ovar);

    ovar = 2.5;
    oar << ovar;
    iar >> ivar;

    HPX_TEST_EQ(ivar.index(), ovar.index());
    HPX_TEST(ivar == ovar);

    ovar = 1;
    oar << ovar;
    iar >> ivar;

    HPX_TEST_EQ(ivar.index(), ovar.index());
    HPX_TEST(ivar == ovar);

    ovar = A<int>(2);
    oar << ovar;
    iar >> ivar;

    HPX_TEST_EQ(ivar.index(), ovar.index());
    HPX_TEST(ivar == ovar);

    const phylanx::util::variant<std::string> sovar = std::string("string");
    phylanx::util::variant<std::string> sivar;
    oar << sovar;
    iar >> sivar;

    HPX_TEST_EQ(sivar.index(), sovar.index());
    HPX_TEST(sivar == sovar);

    return hpx::util::report_errors();
}
