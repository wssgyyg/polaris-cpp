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

#include "api/provider_api.h"

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "context_internal.h"
#include "logger.h"
#include "monitor/api_stat.h"
#include "polaris/accessors.h"
#include "polaris/config.h"
#include "polaris/context.h"
#include "polaris/defs.h"
#include "polaris/plugin.h"
#include "polaris/provider.h"
#include "utils/time_clock.h"

namespace polaris {

ProviderApi::ProviderApi(ProviderApiImpl* impl) { impl_ = impl; }

ProviderApi::~ProviderApi() {
  if (impl_ != NULL) {
    delete impl_;
    impl_ = NULL;
  }
}

ProviderApi* ProviderApi::Create(Context* context) {
  if (context == NULL) {
    POLARIS_LOG(LOG_ERROR, "create provider api failed because context is null");
    return NULL;
  }
  if (context->GetContextMode() != kPrivateContext && context->GetContextMode() != kShareContext &&
      context->GetContextMode() != kLimitContext) {
    POLARIS_LOG(LOG_ERROR, "create provider api failed because context is init with error mode");
    return NULL;
  }
  ProviderApiImpl* api_impl = new ProviderApiImpl(context);
  return new ProviderApi(api_impl);
}

ProviderApi* ProviderApi::CreateFromConfig(Config* config) {
  if (config == NULL) {
    POLARIS_LOG(LOG_WARN, "create provider api failed because parameter config is null");
    return NULL;
  }
  Context* context = Context::Create(config, kPrivateContext);
  if (context == NULL) {
    return NULL;
  }
  return ProviderApi::Create(context);
}

static ProviderApi* CreateWithConfig(Config* config, const std::string& err_msg) {
  if (config == NULL) {
    POLARIS_LOG(LOG_ERROR, "init config with error: %s", err_msg.c_str());
    return NULL;
  }
  ProviderApi* provider = ProviderApi::CreateFromConfig(config);
  delete config;
  return provider;
}

ProviderApi* ProviderApi::CreateFromFile(const std::string& file) {
  std::string err_msg;
  return CreateWithConfig(Config::CreateFromFile(file, err_msg), err_msg);
}

ProviderApi* ProviderApi::CreateFromString(const std::string& content) {
  std::string err_msg;
  return CreateWithConfig(Config::CreateFromString(content, err_msg), err_msg);
}

ProviderApi* ProviderApi::CreateWithDefaultFile() {
  std::string err_msg;
  return CreateWithConfig(Config::CreateWithDefaultFile(err_msg), err_msg);
}

ProviderApiImpl::ProviderApiImpl(Context* context) { context_ = context; }

ProviderApiImpl::~ProviderApiImpl() {
  if (context_ != NULL && context_->GetContextMode() == kPrivateContext) {
    delete context_;
  }
  context_ = NULL;
}

template <class T>
inline bool CheckInstance(T& request, const char* action) {
  if (request.GetServiceNamespace().empty()) {
    POLARIS_LOG(LOG_ERROR, "%s instance with empty service namespace", action);
    return false;
  }
  if (request.GetServiceName().empty()) {
    POLARIS_LOG(LOG_ERROR, "%s instance with empty service name", action);
    return false;
  }
  if (request.GetServiceToken().empty()) {
    POLARIS_LOG(LOG_ERROR, "%s instance with empty service token", action);
    return false;
  }
  if (request.GetHost().empty()) {
    POLARIS_LOG(LOG_ERROR, "%s instance with empty instance host", action);
    return false;
  }
  if (request.GetPort() <= 0 || request.GetPort() > 65535) {
    POLARIS_LOG(LOG_ERROR, "%s instance with error port, port must in [1, 65535]", action);
    return false;
  }
  return true;
}

ReturnCode ProviderApi::Register(const InstanceRegisterRequest& req, std::string& instance_id) {
  ApiStat api_stat(impl_->context_, kApiStatProvderRegsiter);
  ReturnCode ret_code = kReturnInvalidArgument;
  // 检查请求是否合法
  InstanceRegisterRequestAccessor request(req);
  if (!CheckInstance(request, __func__)) {
    api_stat.Record(ret_code);
    return ret_code;
  }
  ContextImpl* context_impl = impl_->context_->GetContextImpl();

  // 同步发送网络请求
  ServerConnector* server_connector = impl_->context_->GetServerConnector();
  uint64_t timeout_ms =
      request.HasTimeout() ? request.GetTimeout() : context_impl->GetApiDefaultTimeout();
  int retry_times = context_impl->GetApiMaxRetryTimes();
  while (retry_times-- > 0 && timeout_ms > 0) {
    uint64_t begin_time = Time::GetCurrentTimeMs();
    ret_code            = server_connector->RegisterInstance(req, timeout_ms, instance_id);
    uint64_t use_time   = Time::GetCurrentTimeMs() - begin_time;
    if ((ret_code != kReturnNetworkFailed && ret_code != kReturnServerError) ||
        use_time >= timeout_ms) {
      break;
    }
    timeout_ms -= use_time;
    uint64_t backoff = std::min(timeout_ms, context_impl->GetApiRetryInterval());
    usleep(backoff * 1000);
    timeout_ms -= backoff;
  }
  api_stat.Record(ret_code);
  return ret_code;
}

template <class T>
inline bool CheckInstanceOrId(T& request, const char* action) {
  if (request.HasInstanceId()) {
    if (request.GetInstanceId().empty()) {
      POLARIS_LOG(LOG_ERROR, "%s instance with empty instance id", action);
      return false;
    }
    if (request.GetServiceToken().empty()) {
      POLARIS_LOG(LOG_ERROR, "%s instance with empty service token", action);
      return false;
    }
    return true;
  }
  return CheckInstance(request, action);
}

ReturnCode ProviderApi::Deregister(const InstanceDeregisterRequest& req) {
  ApiStat api_stat(impl_->context_, kApiStatProviderDeregisger);
  ReturnCode ret_code = kReturnInvalidArgument;
  InstanceDeregisterRequestAccessor request(req);
  if (!CheckInstanceOrId(request, __func__)) {  // 检查请求是否合法
    api_stat.Record(ret_code);
    return ret_code;
  }

  ContextImpl* context_impl = impl_->context_->GetContextImpl();
  // 同步发送网络请求
  ServerConnector* server_connector = impl_->context_->GetServerConnector();
  uint64_t timeout_ms =
      request.HasTimeout() ? request.GetTimeout() : context_impl->GetApiDefaultTimeout();
  int retry_times = context_impl->GetApiMaxRetryTimes();
  while (retry_times-- > 0 && timeout_ms > 0) {
    uint64_t begin_time = Time::GetCurrentTimeMs();
    ret_code            = server_connector->DeregisterInstance(req, timeout_ms);
    uint64_t use_time   = Time::GetCurrentTimeMs() - begin_time;
    if ((ret_code != kReturnNetworkFailed && ret_code != kReturnServerError) ||
        use_time >= timeout_ms) {
      break;
    }
    timeout_ms -= use_time;
    uint64_t backoff = std::min(timeout_ms, context_impl->GetApiRetryInterval());
    usleep(backoff * 1000);
    timeout_ms -= backoff;
  }
  api_stat.Record(ret_code);
  return ret_code;
}

ReturnCode ProviderApi::Heartbeat(const InstanceHeartbeatRequest& req) {
  ApiStat api_stat(impl_->context_, kApiStatProviderHeartbeat);

  ReturnCode ret_code = kReturnInvalidArgument;
  InstanceHeartbeatRequestAccessor request(req);
  if (!CheckInstanceOrId(request, __func__)) {  // 检查请求是否合法
    api_stat.Record(ret_code);
    return ret_code;
  }

  ContextImpl* context_impl         = impl_->context_->GetContextImpl();
  ServerConnector* server_connector = impl_->context_->GetServerConnector();
  uint64_t timeout_ms =
      request.HasTimeout() ? request.GetTimeout() : context_impl->GetApiDefaultTimeout();
  int retry_times = context_impl->GetApiMaxRetryTimes();
  while (retry_times-- > 0 && timeout_ms > 0) {
    uint64_t begin_time = Time::GetCurrentTimeMs();
    ret_code            = server_connector->InstanceHeartbeat(req, timeout_ms);
    uint64_t use_time   = Time::GetCurrentTimeMs() - begin_time;
    if ((ret_code != kReturnNetworkFailed && ret_code != kReturnServerError) ||
        use_time >= timeout_ms) {
      break;
    }
    timeout_ms -= use_time;
    uint64_t backoff = std::min(timeout_ms, context_impl->GetApiRetryInterval());
    usleep(backoff * 1000);
    timeout_ms -= backoff;
  }
  api_stat.Record(ret_code);
  return ret_code;
}

}  // namespace polaris
