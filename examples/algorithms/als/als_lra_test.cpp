//   Copyright (c) 2017 Hartmut Kaiser
//
//   Distributed under the Boost Software License, Version 1.0. (See accompanying
//   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <phylanx/phylanx.hpp>
#include <hpx/hpx_init.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <tuple>

#include <blaze/Math.h>
#include <hpx/program_options.hpp>

//////////////////////////////////////////////////////////////////////////////////
// This example uses part of the breast cancer dataset from UCI Machine Learning
// Repository.
//     https://archive.ics.uci.edu/ml/datasets/Breast+Cancer+Wisconsin+(Diagnostic)
//
// A copy of the full dataset in CSV format (breast_cancer.csv), obtained from
// scikit-learn datasets, is provided in the same folder as this example.
//
// The layout of the data in the provided CSV file used by the example
// is as follows:
// 30 features per line followed by the classification
// 569 lines of data
/////////////////////////////////////////////////////////////////////////////////

char const* const read_r_code = R"(block(
    //
    // Read X-data from given CSV file
    //
    define(read_r, filepath, row_start, row_stop, col_start, col_stop,
        annotate_d(
            slice(file_read_csv(filepath),
                list(row_start, row_stop), list(col_start, col_stop)),
            "read_r",
            list("tile",
                list("rows", row_start, row_stop),
                list("columns", col_start, col_stop)
            )
        )
    ),
    read_r
))";


///////////////////////////////////////////////////////////////////////////////
char const* const als_code = R"(
    //
    // Alternating Least squares algorithm
    //
    define(__als, ratings_row, ratings_column, regularization, num_factors,
        iterations, alpha, enable_output,
        block(
            define(num_users, shape(ratings_row, 0)),
            define(total_num_items, shape(ratings_row, 1)),

            define(total_num_users, shape(ratings_column, 0)),
            define(num_items, shape(ratings_column, 1)),

            define(conf_row, alpha * ratings_row),
            define(conf_column, alpha * ratings_column),

            define(conf_u, constant(0.0, make_list(total_num_items))),
            define(conf_i, constant(0.0, make_list(total_num_users))),

            define(c_u, constant(0.0, make_list(total_num_items, total_num_items))),
            define(c_i, constant(0.0, make_list(total_num_users, total_num_users))),

            define(p_u, constant(0.0, make_list(total_num_items))),
            define(p_i, constant(0.0, make_list(total_num_users))),

            set_seed(0),
            define(X_local, random_d(list(total_num_users, num_factors), nil, nil, nil, "row")),
            define(Y_local, random_d(list(total_num_items, num_factors), nil, nil, nil, "row")),

            define(X, all_gather_d(X_local)),
            define(Y, all_gather_d(Y_local)),

            define(I_f, identity(num_factors)),
            define(I_i, identity(total_num_items)),
            define(I_u, identity(total_num_users)),

            define(k, 0),
            define(i, 0),
            define(u, 0),

            define(XtX, dot(transpose(X), X) + regularization * I_f),
            define(YtY, dot(transpose(Y), Y) + regularization * I_f),

            define(A, constant(0.0, make_list(num_factors, num_factors))),
            define(b, constant(0.0, make_list(num_factors))),

            while(k < iterations,
                block(
                    if(enable_output,
                            block(
                                    cout("iteration ", k),
                                    cout("X: ", X),
                                    cout("Y: ", Y)
                            )
                    ),

                    while(u < num_users,
                        block(
                            store(conf_u, slice_row(conf_row, u)),
                            store(c_u, diag(conf_u)),
                            store(p_u, __ne(conf_u, 0.0, true)),
                            store(A, dot(dot(transpose(Y), c_u), Y) + YtY),
                            store(b, dot(dot(transpose(Y), (c_u + I_i)), transpose(p_u))),
                            store(slice(X_local, list(u, u + 1, 1), nil), dot(inverse(A), b)),
                            store(u, u + 1)
                        )
                    ),
                    store(u, 0),

                    store(X, all_gather_d(X_local)),
                    store(XtX, dot(transpose(X), X) + regularization * I_f),

                    while(i < num_items,
                        block(
                            store(conf_i, slice_column(conf_column, i)),
                            store(c_i, diag(conf_i)),
                            store(p_i, __ne(conf_i, 0.0, true)),
                            store(A, dot(dot(transpose(X), c_i), X) + XtX),
                            store(b, dot(dot(transpose(X), (c_i + I_u)), transpose(p_i))),
                            store(slice(Y_local, list(i, i + 1, 1), nil), dot(inverse(A), b)),
                            store(i, i + 1)
                        )
                    ),
                    store(i, 0),

                    store(Y, all_gather_d(Y_local)),
                    store(YtY, dot(transpose(Y), Y) + regularization * I_f),

                    store(k, k + 1)
                )
            ),
            list(X, Y)
        )
    )
    __als
)";

////////////////////////////////////////////////////////////////////////////////

std::tuple<std::int64_t, std::int64_t> calculate_tiling_parameters(std::int64_t start,
    std::int64_t stop)
{
    std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    std::uint32_t this_locality = hpx::get_locality_id();

    std::int64_t dims = stop - start;

    if (dims > num_localities)
    {
        dims = (dims + num_localities) / num_localities;
        start += this_locality * dims;
        stop = (std::min)(stop, start + dims);
    }
    return std::make_tuple(start, stop);
}

////////////////////////////////////////////////////////////////////////////////
int hpx_main(hpx::program_options::variables_map& vm)
{
    if (vm.count("data_csv") == 0)
    {
        std::cerr << "Please specify '--data_csv=data-file'";
        return hpx::finalize();
    }

    // compile the given code
    using namespace phylanx::execution_tree;

    compiler::function_list snippets;
    auto const& code_read_r = compile("read_r", read_r_code, snippets);
    auto read_r = code_read_r.run();

    // handle command line arguments
    auto filename = vm["data_csv"].as<std::string>();

    auto row_start = vm["row_start"].as<std::int64_t>();
    auto row_stop = vm["row_stop"].as<std::int64_t>();
    auto col_start = vm["col_start"].as<std::int64_t>();
    auto col_stop = vm["col_stop"].as<std::int64_t>();

    auto alpha = vm["alpha"].as<double>();
    auto regularization = vm["regularization"].as<double>();
    auto num_factors = vm["factors"].as<int64_t>();
    auto iterations = vm["num_iterations"].as<std::int64_t>();
    bool enable_output = vm.count("enable_output") != 0;

    // calculate tiling parameters for this locality, read data
    primitive_argument_type ratings_row, ratings_column;
    std::int64_t user_row_start, user_row_stop, item_col_start, item_col_stop;

    std::tie(user_row_start, user_row_stop) = calculate_tiling_parameters(row_start, row_stop);
    ratings_row = read_r(filename, user_row_start, user_row_stop, col_start, col_stop);

    std::tie(item_col_start, item_col_stop) = calculate_tiling_parameters(col_start, col_stop);
    ratings_column = read_r(filename, row_start, row_stop, item_col_start, item_col_stop);


    std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    std::cout << " total num_localities: " << num_localities << std::endl;

    if (hpx::get_locality_id() == 0)
    {
        //auto result_r = extract_list_value(result);
        //auto it_0 = result_r.begin();
        std::cout << " on loc 0: \n"
                  << "there are " << num_localities << " localities \n"
                  << "user row partition: \n"
                  << " user_row_start: " << user_row_start
                  << " user_row_stop: " << user_row_stop
                  << " col_start: " << col_start
                  << " col_stop: " << col_stop 
                  << " item column partition: \n"
                  << " row_start: " << row_start
                  << " row_stop: " << row_stop
                  << " item_col_start: " << item_col_start
                  << " item_col_stop: " << item_col_stop
                  << std::endl;
    }
    if (hpx::get_locality_id() == 1)
    {

        //auto result_r = extract_list_value(result);
        //auto it_1 = result_r.begin();
        std::cout << " on loc 1: \n"
                  << "there are " << num_localities << " localities "
                  << "user row partition: \n"
                  << " user_row_start: " << user_row_start
                  << " user_row_stop: " << user_row_stop
                  << " col_start: " << col_start
                  << " col_stop: " << col_stop 
                  << " item column partition: \n"
                  << " row_start: " << row_start
                  << " row_stop: " << row_stop
                  << " item_col_start: " << item_col_start
                  << " item_col_stop: " << item_col_stop
                  << std::endl;
    }

    // evaluate ALS using the read data
    auto const& code_als = compile("__als", als_code, snippets);
    auto als = code_als.run();

//    // time the execution
//    hpx::util::high_resolution_timer t;
//
//    auto result =
//        als(std::move(ratings_row), std::move(ratings_column), regularization, num_factors,
//        iterations, alpha, enable_output);
//
//    auto elapsed = t.elapsed();
//    auto result_r = extract_list_value(result);
//    auto it = result_r.begin();
//
//    std::cout << "X: \n"
//              << extract_numeric_value(*it++)
//              << "\nY: \n"
//              << extract_numeric_value(*it) << std::endl
//              << "Calculated in: " << elapsed << " seconds" << std::endl;


    return hpx::finalize();
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    using hpx::program_options::options_description;
    using hpx::program_options::value;

    // command line handling
    options_description desc("usage: als [options]");
    desc.add_options()
        ("enable_output,e",
          "enable progress output (default: false)")
        ("num_iterations,n", value<std::int64_t>()->default_value(3),
          "number of iterations (default: 10.0)")
        ("factors,f", value<std::int64_t>()->default_value(10),
         "number of factors (default: 10)")
        ("alpha,a", value<double>()->default_value(40),
          "alpha (default: 40)")
        ("data_csv", value<std::string>(), "file name for reading data")
        ("row_start", value<std::int64_t>()->default_value(0),
          "row_start (default: 0)")
        ("row_stop", value<std::int64_t>()->default_value(30),
          "row_stop (default: 569)")
        ("col_start", value<std::int64_t>()->default_value(0),
          "col_start (default: 0)")
        ("col_stop", value<std::int64_t>()->default_value(100),
          "col_stop (default: 30)")
    ;

    // make sure hpx_main is run on all localities
    std::vector<std::string> cfg = {
        "hpx.run_hpx_main!=1"
    };

    return hpx::init(desc, argc, argv, cfg);
}
