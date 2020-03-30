#pragma once
#include <memory>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include "paxoskv.grpc.pb.h"
#include "paxoskv.pb.h"
class PaxosKVServiceClient
{
public:
  PaxosKVServiceClient();
  static std::shared_ptr<grpc::Channel> GetChannel();
  int Get(const ::paxoskv::GetReq &oReq, ::paxoskv::GetResp &oResp);
private:
  std::shared_ptr<grpc::Channel> m_pChannel;
};