#  Copyright (c) 2020 Steven R. Brandt
#
#  Distributed under the Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# #1242: list(1) does not match itself

from phylanx import Phylanx


@Phylanx
def foo():
    return list(1) == list(1)


result = foo()
assert result, result
