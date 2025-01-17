//  Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//  https://opensource.org/licenses/BSD-3-Clause
//
//  Unless required by applicable law or agreed to in writing, software distributed
//  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
//  language governing permissions and limitations under the License.
//

#include "engine/outlier_detection_executor.h"

#include <iosfwd>
#include <vector>

#include "context_internal.h"
#include "polaris/context.h"
#include "reactor/reactor.h"
#include "reactor/task.h"

namespace polaris {

OutlierDetectionExecutor::OutlierDetectionExecutor(Context* context) : Executor(context) {}

void OutlierDetectionExecutor::SetupWork() {
  reactor_.SubmitTask(new FuncTask<OutlierDetectionExecutor>(TimingDetect, this));
}

void OutlierDetectionExecutor::TimingDetect(OutlierDetectionExecutor* executor) {
  std::vector<ServiceContext*> all_service_contexts;
  executor->context_->GetContextImpl()->GetAllServiceContext(all_service_contexts);
  for (std::size_t i = 0; i < all_service_contexts.size(); ++i) {
    OutlierDetectorChain* outlier_detector_chain =
        all_service_contexts[i]->GetOutlierDetectorChain();
    outlier_detector_chain->DetectInstance();
    all_service_contexts[i]->DecrementRef();
  }
  all_service_contexts.clear();
  // 设置定时任务
  executor->reactor_.AddTimingTask(
      new TimingFuncTask<OutlierDetectionExecutor>(TimingDetect, executor, 1000));
}

}  // namespace polaris
