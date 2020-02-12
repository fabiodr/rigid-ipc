#pragma once

#include "serialize_json.hpp"

namespace ccd {
namespace io {
    template <typename T>
    nlohmann::json to_json(const Eigen::VectorX<T>& vector)
    {
        std::vector<T> vec(vector.data(), vector.data() + vector.size());
        return nlohmann::json(vec);
    }

    template <typename T>
    nlohmann::json to_json(const Eigen::MatrixX<T>& matrix)
    {
        size_t num_rows = size_t(matrix.rows());
        size_t num_cols = size_t(matrix.cols());
        std::vector<std::vector<T>> vec(num_rows);
        for (size_t i = 0; i < num_rows; ++i) {
            std::vector<T> row(num_cols);
            for (size_t j = 0; j < num_cols; ++j) {
                row[j] = matrix(long(i), long(j));
            }
            vec[i] = row;
        }
        return nlohmann::json(vec);
    }

    template <typename T>
    void from_json(const nlohmann::json& json, Eigen::VectorX<T>& vector)
    {
        typedef std::vector<T> L;
        L list = json.get<L>();
        vector = Eigen::Map<Eigen::VectorX<T>>(list.data(), long(list.size()));
    }

    template <typename T>
    void from_json(const nlohmann::json& json, Eigen::VectorX3<T>& vector)
    {
        typedef std::vector<T> L;
        L list = json.get<L>();
        vector = Eigen::Map<Eigen::VectorX3<T>>(list.data(), long(list.size()));
    }

    template <typename T>
    void from_json(const nlohmann::json& json, Eigen::MatrixX<T>& matrix)
    {
        typedef std::vector<std::vector<T>> L;
        L list = json.get<L>();

        size_t num_rows = list.size();
        if (num_rows == 0) {
            return;
        }
        size_t num_cols = list[0].size();
        matrix.resize(long(num_rows), long(num_cols));

        for (size_t i = 0; i < num_rows; ++i) {
            assert(num_cols == list[i].size());
            matrix.row(int(i)) =
                Eigen::Map<Eigen::Matrix<T, 1, Eigen::Dynamic>>(
                    list[i].data(), long(num_cols));
        }
    }
} // namespace io
} // namespace ccd
