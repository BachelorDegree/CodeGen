#include <google/protobuf/compiler/importer.h>

#include <iostream>
#include <sys/stat.h>
#include "Printer.hpp"
#include "utils.hpp"
using namespace google::protobuf;
class ErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
  virtual void AddError(const std::string &filename, int line, int column, const std::string &message)
  {
    std::cerr << filename << " " << line << " " << column << " " << message << std::endl;
  }
};
void MakeCmakeList(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->package();
  oArgument["service"] = pDescriptor->service(0)->name();
  FilePrinter oCodePrinter{strBaseDir + "/CMakeLists.txt"};
  oCodePrinter.Print(R"xxx(CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(|package|client)

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_FLAGS "-Wall")
SET(CMAKE_CXX_FLAGS "-fPIC")
IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
    SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-DDEBUG")
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)
INCLUDE_DIRECTORIES(/usr/local/include)
INCLUDE_DIRECTORIES(.)
LINK_LIBRARIES(pthread protobuf gpr grpc++ grpc++_reflection)

SET(APP_SOURCES "./|service|Client.cpp")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./*.cpp")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./*.cc")
ADD_LIBRARY(|package|client STATIC ${APP_SOURCES})
TARGET_LINK_LIBRARIES(|package|client coredeps)

)xxx",
                     oArgument, '|');
}
void MakeClient(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->package();
  oArgument["service"] = pDescriptor->service(0)->name();
  oArgument["service_symbol"] = PBHelper::QualifiedServiceName(pDescriptor->service(0));
  FilePrinter oHeaderPrinter{strBaseDir + "/" + pDescriptor->service(0)->name() + "Client.hpp"};
  oHeaderPrinter.Print(R"xxx(#pragma once
#include <memory>
#include <string>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include "$package$.grpc.pb.h"
#include "$package$.pb.h"
class $service$Client
{
private:
  std::shared_ptr<grpc::Channel> m_pChannel;
  std::string m_strServiceName;
public:
  $service$Client();
  // User specified IpPort or CanonicalName
  $service$Client(const std::string &strIpPortOrCanonicalName);
  std::shared_ptr<grpc::Channel> GetChannel();)xxx",
                       oArgument);
  oHeaderPrinter.Indent();
  for (int i = 0; i < pDescriptor->service_count(); i++)
  {
    auto pService = pDescriptor->service(i);
    for (int j = 0; j < pService->method_count(); j++)
    {
      auto pMethod = pService->method(j);
      oArgument["method"] = pMethod->name();
      oArgument["request"] = pMethod->input_type()->name();
      oArgument["request_symbol"] = PBHelper::QualifiedClassName(pMethod->input_type());
      oArgument["response"] = pMethod->output_type()->name();
      oArgument["response_symbol"] = PBHelper::QualifiedClassName(pMethod->output_type());
      oHeaderPrinter.Print("int $method$(const $request_symbol$ & oReq, $response_symbol$ & oResp);", oArgument);
    }
  }
  oHeaderPrinter.Outdent();
  oHeaderPrinter.Print("};");
  FilePrinter oClientPrinter{strBaseDir + "/" + pDescriptor->service(0)->name() + "Client.cpp"};
  oClientPrinter.Print(R"xxx(#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <coredeps/SatelliteClient.hpp>
#include <coredeps/ContextHelper.hpp>
#include "$service$Client.hpp"
$service$Client::$service$Client():
  m_strServiceName("$service$")
{
  m_pChannel = GetChannel();
}
$service$Client::$service$Client(const std::string &strIpPortOrCanonicalName)
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
std::shared_ptr<grpc::Channel> $service$Client::GetChannel()
{
  std::string strServer = SatelliteClient::GetInstance().GetNode(m_strServiceName);
  return grpc::CreateChannel(strServer, grpc::InsecureChannelCredentials());
})xxx",
                       oArgument);
  for (int i = 0; i < pDescriptor->service_count(); i++)
  {
    auto pService = pDescriptor->service(i);
    for (int j = 0; j < pService->method_count(); j++)
    {
      auto pMethod = pService->method(j);
      oArgument["method"] = pMethod->name();
      oArgument["request"] = pMethod->input_type()->name();
      oArgument["request_symbol"] = PBHelper::QualifiedClassName(pMethod->input_type());
      oArgument["response"] = pMethod->output_type()->name();
      oArgument["response_symbol"] = PBHelper::QualifiedClassName(pMethod->output_type());
      oClientPrinter.Print("int $service$Client::$method$(const $request_symbol$ & oReq, $response_symbol$ & oResp)", oArgument);
      oClientPrinter.Print(R"xxx({
  $service_symbol$::Stub oStub{m_pChannel};
  grpc::ClientContext oContext;
  auto oStatus = oStub.$method$(&oContext, oReq, &oResp);
  if (oStatus.ok() == false)
  {
    return -1;
  }
  return ClientContextHelper(oContext).GetReturnCode();
})xxx",
                           oArgument);
    }
  }
}
void MakeProto(const std::string &strBaseDir, const std::string &strProtoPath)
{

  system(("protoc --cpp_out=" + strBaseDir + " " + strProtoPath + ";").c_str());
  system(("protoc --grpc_out=" + strBaseDir + " --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` " + strProtoPath).c_str());
}
int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " service.proto [includedir1 includedir2 ...]" << std::endl;
    return -1;
  }
  compiler::DiskSourceTree oSourceTree;
  oSourceTree.MapPath("", ".");
  for (int i = 2; i < argc; i++)
  {
    oSourceTree.MapPath("", argv[i]);
  }
  ErrorCollector oErrorCollector;
  compiler::Importer oImporter(&oSourceTree, &oErrorCollector);
  auto pFileDesc = oImporter.Import(argv[1]);
  std::string strDir = pFileDesc->service(0)->name() + "Client";
  mkdir(strDir.c_str(), 0755);
  MakeCmakeList(strDir, pFileDesc);
  MakeProto(strDir, argv[1]);
  MakeClient(strDir, pFileDesc);
}
