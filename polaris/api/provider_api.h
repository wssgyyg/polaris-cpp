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

#ifndef POLARIS_CPP_POLARIS_API_PROVIDER_API_H_
#define POLARIS_CPP_POLARIS_API_PROVIDER_API_H_

namespace polaris {

class Context;

/// @brief POLARIS服务端API的主接口实现
class ProviderApiImpl {
public:
  explicit ProviderApiImpl(Context* context);

  ~ProviderApiImpl();

private:
  friend class ProviderApi;
  Context* context_;
};

}  // namespace polaris

#endif  //  POLARIS_CPP_POLARIS_API_PROVIDER_API_H_
