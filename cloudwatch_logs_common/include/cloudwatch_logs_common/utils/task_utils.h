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

#include <functional>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

/**
 * Function callback when a message has attempted an upload to the cloud.
 */
template <typename Status, typename T>
using UploadStatusFunction = std::function<void (const Status& upload_status, const T &message)>;

}  // namespace Utils
}  // namespace CloudwatchLogs
}  // namespace Aws
