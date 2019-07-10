/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <aws/core/Aws.h>
#include <aws/logs/CloudWatchLogsClient.h>

//--------------------------Metrics Definitions--------------------------------
using LogEvent = Aws::CloudWatchLogs::Model::InputLogEvent;
using LogCollection = std::list<LogEvent>;
using LogTask = Aws::DataFlow::Task<LogCollection>;
using LogTaskSource = std::shared_ptr<Aws::DataFlow::Source<std::shared_ptr<LogTask>>>;
//-----------------------------------------------------------------------------

