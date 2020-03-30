#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <coredeps/SatelliteClient.hpp>
#include <coredeps/ContextHelper.hpp>
#include "PaxosKVServiceClient.hpp"
PaxosKVServiceClient::PaxosKVServiceClient(){
  m_pChannel = GetChannel();
}
std::shared_ptr<grpc::Channel> PaxosKVServiceClient::GetChannel()
{
  const std::string strServiceName = "PaxosKVService";
  std::string strServer = SatelliteClient::GetInstance().GetNode(strServiceName);
  return grpc::CreateChannel(strServer, grpc::InsecureChannelCredentials());
}
int PaxosKVServiceClient::Get(const ::paxoskv::GetReq &oReq, ::paxoskv::GetResp &oResp)
{
  paxoskv::PaxosKVService::Stub oStub{m_pChannel};
  grpc::ClientContext oContext;
  auto oStatus = oStub.Get(&oContext, oReq, &oResp);
  if (oStatus.ok() == false)
  {
    return -1;
  }
  return ClientContextHelper(oContext).GetReturnCode();
}