/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#pragma once
#include <cloudwatch_logs_common/dataflow/sink.h>
#include <cloudwatch_logs_common/dataflow/source.h>

namespace Aws {
namespace DataFlow {

enum PriorityLevel : uint {
  LOWEST_PRIORITY = 0,
  LOW_PRIORITY,
  MEDIUM_PRIORITY,
  HIGH_PRIORITY,
  HIGHEST_PRIORITY
};

struct PriorityOptions {
  explicit PriorityOptions(PriorityLevel level = MEDIUM_PRIORITY) {
    priority_level = level;
  }
  PriorityLevel priority_level;

  inline bool operator > (const PriorityOptions &other) const {
    return priority_level > other.priority_level;
  }

  inline bool operator < (const PriorityOptions &other) const {
    return priority_level < other.priority_level;
  }
};

template <typename O>
class OutputStage;
template <typename I>
class InputStage;

template <typename O>
class OutputStage {
public:
  std::shared_ptr<Sink<O>> getSink() {
    return sink_;
  }
  inline void setSink(std::shared_ptr<Sink<O>> sink) {
    sink_ = sink;
  }
 private:
  std::shared_ptr<Sink<O>> sink_;
};

template <typename I>
class InputStage {
 public:
  inline std::shared_ptr<Source<I>> getSource() {
    return source_;
  }
  inline void setSource(std::shared_ptr<Source<I>> source) {
    source_ = source;
  }
 private:
  std::shared_ptr<Source<I>> source_;
};

template <typename I>
class MulitInputStage {
public:
  std::shared_ptr<Source<I>> getSource();
  virtual void addSource(std::shared_ptr<Source<I>> source, PriorityOptions priority_options);
};

}  // namespace DataFlow
}  // namespace Aws
