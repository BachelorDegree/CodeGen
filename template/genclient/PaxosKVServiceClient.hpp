#pragma once
#include <memory>
#include <string>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include "paxoskv.grpc.pb.h"
#include "paxoskv.pb.h"
class PaxosKVServiceClient
{
public:
  PaxosKVServiceClient();
  // User specified IpPort or CanonicalName
  PaxosKVServiceClient(const std::string &strIpPortOrCanonicalName);
  std::shared_ptr<grpc::Channel> GetChannel();
  int Get(const ::paxoskv::GetReq &oReq, ::paxoskv::GetResp &oResp);

private:
  std::shared_ptr<grpc::Channel> m_pChannel;
  std::string m_strServiceName;
};