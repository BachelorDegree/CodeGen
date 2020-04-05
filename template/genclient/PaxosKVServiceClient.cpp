#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <coredeps/SatelliteClient.hpp>
#include <coredeps/ContextHelper.hpp>
#include "PaxosKVServiceClient.hpp"
PaxosKVServiceClient::PaxosKVServiceClient():
  m_strServiceName("PaxosKVService")
{
  m_pChannel = this->GetChannel();
}
PaxosKVServiceClient::PaxosKVServiceClient(const std::string &strIpPortOrCanonicalName)
{
  if (strIpPortOrCanonicalName.find(':') == std::string::npos)
  {
    m_strServiceName = strIpPortOrCanonicalName;
    m_pChannel = this->GetChannel();
  }
  else
  {
    m_pChannel = grpc::CreateChannel(strIpPortOrCanonicalName, grpc::InsecureChannelCredentials());
  }
}
std::shared_ptr<grpc::Channel> PaxosKVServiceClient::GetChannel()
{
  std::string strServer = SatelliteClient::GetInstance().GetNode(m_strServiceName);
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