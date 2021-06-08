/*
    Copyright (c) 2013, Taiga Nomi and the respective contributors
    All rights reserved.

    Use of this source code is governed by a BSD-style license that can be found
    in the LICENSE file.
*/
#pragma once

#include <memory>
#include "tiny_dnn/activations/sigmoid_layer.h"
#include "tiny_dnn/activations/tanh_layer.h"
#include "tiny_dnn/core/params/params.h"

namespace tiny_dnn {
namespace core {

class lstm_cell_params : public Params {
 public:
  size_t in_size_;
  size_t out_size_;
  std::shared_ptr<tanh_layer> tanh_;
  std::shared_ptr<sigmoid_layer> sigmoid_;
  bool has_bias_;
};

inline lstm_cell_params &Params::lstm_cell() {
  return *(static_cast<lstm_cell_params *>(this));
}

}  // namespace core
}  // namespace tiny_dnn
