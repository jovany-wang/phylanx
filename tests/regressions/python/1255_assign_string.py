#  Copyright (c) 2020 Patrick Diehl
#
#  Distributed under the Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# #1255: Passing a dict to Phylanx is not possible

from phylanx import Phylanx


dict_nn = {
    "key": "value"
}


@Phylanx
def change(data):
    data["key_float"] = 42.0
    data["key_int"] = 42
    data["key"] = "new value"
    return data


result = change(dict_nn)
assert result['key_float'] == 42.0, result
assert result['key_int'] == 42, result
assert result['key'] == 'new value', result
