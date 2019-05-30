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


#include <cloudwatch_logs_common/dataflow/observed_queue.h>
#include <cloudwatch_logs_common/dataflow/pipeline.h>
#include <cloudwatch_logs_common/dataflow/priority_options.h>
#include <cloudwatch_logs_common/dataflow/queue_monitor.h>
#include <cloudwatch_logs_common/dataflow/sink.h>
#include <cloudwatch_logs_common/dataflow/source.h>
#include <cloudwatch_logs_common/dataflow/status_monitor.h>

namespace Aws {
namespace DataFlow {

template<
    typename T,
    class O>
typename std::enable_if<std::is_base_of<Sink<T>, O>::value, std::shared_ptr<O>>::type
inline operator >> (
  OutputStage<T> &output_stage,
  std::shared_ptr<O> &sink)
{
  output_stage.setSink(sink);
  return sink;
}

template<
    typename T,
    class O>
typename std::enable_if<std::is_base_of<QueueDemux<T>, O>::value, std::shared_ptr<O>>::type
inline operator >> (
  std::tuple<std::shared_ptr<ObservedQueue<T>>,
  PriorityOptions> observed_queue,
  std::shared_ptr<O> sink)
{
  sink->addSource(std::get<0>(observed_queue), std::get<1>(observed_queue));
  return sink;
}

template<typename T>
std::tuple<std::shared_ptr<ObservedQueue<T>>, PriorityOptions>
inline operator >> (
  std::shared_ptr<ObservedQueue<T>> observed_queue,
  PriorityLevel level)
{
  return std::make_tuple(observed_queue, PriorityOptions(level));
}

template<
  typename T,
  class O>
typename std::enable_if<std::is_base_of<Source<T>, O>::value, std::shared_ptr<O>>::type
inline operator >> (
  std::shared_ptr<O> &source,
  InputStage<T> &inputStage)
{
  inputStage.setSource(source);
  return inputStage;
}

}  // namespace DataFlow
}  // namespace Aws
