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
void MakeHandler(const std::string &strBaseDir, const MethodDescriptor *pDescriptor)
{
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->file()->package();
  oArgument["method"] = pDescriptor->name();
  oArgument["service"] = pDescriptor->service()->name();
  oArgument["service_symbol"] = PBHelper::QualifiedServiceName(pDescriptor->service());
  oArgument["request"] = pDescriptor->input_type()->name();
  oArgument["request_symbol"] = PBHelper::QualifiedClassName(pDescriptor->input_type());
  oArgument["response"] = pDescriptor->output_type()->name();
  oArgument["response_symbol"] = PBHelper::QualifiedClassName(pDescriptor->output_type());
  FilePrinter oHeaderPrinter{strBaseDir + "/" + pDescriptor->name() + "Handler.hpp"};
  oHeaderPrinter.Print(R"xxx(#pragma once

#include "AsyncRpcHandler.hpp"
#include "Proto/$package$.grpc.pb.h"

class $method$Handler final : public AsyncRpcHandler
{
public:
    $method$Handler($service_symbol$::AsyncService *service, grpc::ServerCompletionQueue *cq):
        AsyncRpcHandler(cq), service(service), responder(&ctx)
    {
        this->Proceed();
    }
    void Proceed(void) override;
    void SetInterfaceName(void) override;

private:
    $service_symbol$::AsyncService*                     service;
    $request_symbol$                                    request;
    $response_symbol$                                   response;
    grpc::ServerAsyncResponseWriter<$response_symbol$>  responder;
};)xxx",
                       oArgument);
  FilePrinter oCodePrinter{strBaseDir + "/" + pDescriptor->name() + "Handler.cpp"};
  oCodePrinter.Print(R"xxx(#include "$method$Handler.hpp"

void $method$Handler::SetInterfaceName(void)
{
    interfaceName = "$service$.$method$";
}

void $method$Handler::Proceed(void)
{
    switch (status)
    {
    case Status::CREATE:
        this->SetStatusProcess();
        service->Request$method$(&ctx, &request, &responder, cq, cq, this);
        break;
    case Status::PROCESS:
        // Firstly, spawn a new handler for next incoming RPC call
        new $method$Handler(service, cq);
        // Implement your logic here
        // response.set_reply(request.greeting());
        this->SetStatusFinish();
        responder.Finish(response, grpc::Status::OK, this);
        break;
    case Status::FINISH:
        delete this;
        break;
    default:
        // throw exception
        ;
    }
}
)xxx",
                     oArgument);
}
void MakeHandlers(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  for (int i = 0; i < pDescriptor->service_count(); i++)
  {
    auto pService = pDescriptor->service(i);
    for (int j = 0; j < pService->method_count(); j++)
    {
      MakeHandler(strBaseDir, pService->method(j));
    }
  }
}
void MakeAsyncRpcHandler(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  FilePrinter oCodePrinter{strBaseDir + "/AsyncRpcHandler.hpp"};
  oCodePrinter.Print(R"xxx(#pragma once

#include <grpcpp/server_context.h>

namespace grpc
{
    class ServerContext;
    class ServerCompletionQueue;
}

class AsyncRpcHandler
{
public:
    enum class Status
    {
        CREATE,
        PROCESS,
        FINISH
    };
    AsyncRpcHandler(void) = delete;
    explicit AsyncRpcHandler(grpc::ServerCompletionQueue *cq):
        status(Status::CREATE), cq(cq) { }

    virtual void Proceed(void) = 0;

    virtual void       SetInterfaceName(void) = 0;
    const std::string& GetInterfaceName(void) const { return interfaceName; }
    
    void SetStatusCreate(void) { status = Status::CREATE; }
    void SetStatusProcess(void) { status = Status::PROCESS; }
    void SetStatusFinish(void) { status = Status::FINISH; }

protected:
    Status                          status;
    grpc::ServerCompletionQueue*    cq;
    grpc::ServerContext             ctx;
    std::string                     interfaceName;
    // You should add (when inherit from this):
    // YourRpcRequestPB                                     request;
    // YourRpcResponsePB                                    response;
    // grpc::ServerAsyncResponseWriter<YourRpcResponsePB>   responder;

    // ...and, the "service instance" itself if needed:
    // YourService::AsyncService*                           service;
};)xxx");
}
void MakeServerConf(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  FilePrinter oCodePrinter{strBaseDir + "/server.conf"};
  oCodePrinter.Print(R"xxx(<server>
    daemon_name = aloha_io
    bind_ip = 0.0.0.0
    bind_port = 8964

    worker_thread_num = 4
</server>
<libs>
    sample = /path/to/libsample.so
</libs>)xxx");
}
void MakeDylibExport(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->package();
  oArgument["service"] = pDescriptor->service(0)->name();
  oArgument["service_symbol"] = PBHelper::QualifiedServiceName(pDescriptor->service(0));
  FilePrinter oHeaderPrinter{strBaseDir + "/dylib_export.h"};
  oHeaderPrinter.Print(R"xxx(#pragma once

namespace grpc
{
    class Service;
    class ServerCompletionQueue;
}

extern "C" 
{

const char *    EXPORT_Description(void);
void            EXPORT_DylibInit(void);
grpc::Service * EXPORT_GetGrpcServiceInstance(void);
void            EXPORT_OnWorkerThreadStart(grpc::ServerCompletionQueue*);

} )xxx");

  FilePrinter oCodePrinter{strBaseDir + "/dylib.cpp"};
  oCodePrinter.Print("#include \"dylib_export.h\"", oArgument);
  oCodePrinter.Print("#include \"Proto/$package$.grpc.pb.h\"", oArgument);
  for (int i = 0; i < pDescriptor->service_count(); i++)
  {
    auto pService = pDescriptor->service(i);
    for (int j = 0; j < pService->method_count(); j++)
    {
      oArgument["method"] = pService->method(j)->name();
      oCodePrinter.Print("#include \"Handler/$method$Handler.hpp\"", oArgument);
    }
  }
  oCodePrinter.Print(R"xxx(
$service_symbol$::AsyncService service;

const char * EXPORT_Description(void)
{
    return "$package$";
}

void EXPORT_DylibInit(void)
{
    // do nothing
}

grpc::Service * EXPORT_GetGrpcServiceInstance(void)
{
    return &service;
}

void EXPORT_OnWorkerThreadStart(grpc::ServerCompletionQueue *cq)
{
    // Bind handlers
)xxx",
                     oArgument);
  for (int i = 0; i < pDescriptor->service_count(); i++)
  {
    auto pService = pDescriptor->service(i);
    oCodePrinter.Indent();
    for (int j = 0; j < pService->method_count(); j++)
    {
      oArgument["method"] = pService->method(j)->name();
      oCodePrinter.Print("new $method$Handler(&service, cq);", oArgument);
    }
    oCodePrinter.Outdent();
  }
  oCodePrinter.Print("}");
}
void MakeCmakeList(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->package();
  oArgument["service"] = pDescriptor->service(0)->name();
  FilePrinter oCodePrinter{strBaseDir + "/CMakeLists.txt"};
  oCodePrinter.Print(R"xxx(CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(|package|)

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_FLAGS "-Wall")
IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
    SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-DDEBUG")
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

LINK_LIBRARIES(pthread protobuf gpr grpc++ grpc++_reflection)

SET(APP_SOURCES "./dylib.cpp")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./Proto/*.cc")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./Handler/*.cpp")
INCLUDE_DIRECTORIES(".")
ADD_LIBRARY(|package| SHARED ${APP_SOURCES}))xxx",
                     oArgument, '|');
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
  mkdir(pFileDesc->package().c_str(), 0755);
  mkdir((pFileDesc->package() + "/Proto").c_str(), 0755);
  mkdir((pFileDesc->package() + "/Handler").c_str(), 0755);
  MakeHandlers(pFileDesc->package() + "/Handler", pFileDesc);
  MakeAsyncRpcHandler(pFileDesc->package(), pFileDesc);
  MakeServerConf(pFileDesc->package(), pFileDesc);
  MakeDylibExport(pFileDesc->package(), pFileDesc);
  MakeCmakeList(pFileDesc->package(), pFileDesc);
  MakeProto(pFileDesc->package() + "/Proto", argv[1]);
}