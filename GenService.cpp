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
#include "$service$Impl.hpp"
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
    {
        // Firstly, spawn a new handler for next incoming RPC call
        new $method$Handler(service, cq);
        this->BeforeProcess();
        // Implement your logic here
        int iRet = $service$Impl::GetInstance()->$method$(request, response);
        this->SetReturnCode(iRet);
        this->SetStatusFinish();
        responder.Finish(response, grpc::Status::OK, this);
        break;
    }
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
void MakeImpl(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->package();
  oArgument["service"] = pDescriptor->service(0)->name();
  oArgument["service_symbol"] = PBHelper::QualifiedServiceName(pDescriptor->service(0));
  FilePrinter oHeaderPrinter{strBaseDir + "/" + pDescriptor->service(0)->name() + "Impl.hpp"};
  oHeaderPrinter.Print(R"xxx(#pragma once
#include "Proto/$package$.pb.h"
class $service$Impl
{
public:
    static $service$Impl *GetInstance();
    static void SetInstance($service$Impl *);
    static int BeforeServerStart(const char * czConf) {
        return 0;
    }
    int BeforeWorkerStart() {
        return 0;
    })xxx",
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
  FilePrinter oInstancePrinter{strBaseDir + "/" + pDescriptor->service(0)->name() + "ImplInstance.cpp"};
  oInstancePrinter.Print(R"xxx(#include <colib/co_routine_specific.h>
#include "$service$Impl.hpp"
struct __$service$ImplWrapper{
    $service$Impl * pImpl;
};
CO_ROUTINE_SPECIFIC(__$service$ImplWrapper, g_co$service$ImplWrapper)
$service$Impl *$service$Impl::GetInstance()
{
    return g_co$service$ImplWrapper->pImpl;
}
void $service$Impl::SetInstance($service$Impl *pImpl)
{
    g_co$service$ImplWrapper->pImpl = pImpl;
})xxx",
                         oArgument);
  FilePrinter oImplPrinter{strBaseDir + "/" + pDescriptor->service(0)->name() + "Impl.cpp"};
  oImplPrinter.Print("#include \"$service$Impl.hpp\"", oArgument);
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
      oImplPrinter.Print("int $service$Impl::$method$(const $request_symbol$ & oReq, $response_symbol$ & oResp) {", oArgument);
      oImplPrinter.Indent();
      oImplPrinter.Print("return -1;");
      oImplPrinter.Outdent();
      oImplPrinter.Print("}");
    }
  }
}
void MakeAsyncRpcHandler(const std::string &strBaseDir, const FileDescriptor *pDescriptor)
{
  FilePrinter oCodePrinter{strBaseDir + "/AsyncRpcHandler.hpp"};
  oCodePrinter.Print(R"xxx(#pragma once

#include <grpcpp/server_context.h>
#include "coredeps/ContextHelper.hpp"
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
    void BeforeProcess(void) { ServerContextHelper::GetInstance()->BindContext(ctx); }
    void SetReturnCode(int iRet) { ServerContextHelper::GetInstance()->SetReturnCode(iRet); }
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
  PrinterArgument oArgument;
  oArgument["package"] = pDescriptor->package();
  oArgument["service"] = pDescriptor->service(0)->name();
  FilePrinter oCodePrinter{strBaseDir + "/server.conf"};
  oCodePrinter.Print(R"xxx(<server>
    daemon_name = aloha_io
    bind_ip = 0.0.0.0
    bind_port = 8964

    worker_thread_num = 4
    worker_co_num = 20
</server>
<satellite>
    bind_interface = eth0
    <servers>
        server1 = 10.0.0.102:5553
        # server2 = 10.0.0.103:5553
    </servers>
</satellite>
<libs>
    <$service$>
        canonical_service_name = $service$
        dylib_path = /path/to/libsample.so
        config_file = /path/to/business/config.conf
    </$service$>>
</libs>)xxx", oArgument);
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
void            EXPORT_DylibInit(const char *);
grpc::Service * EXPORT_GetGrpcServiceInstance(void);
void            EXPORT_OnWorkerThreadStart(grpc::ServerCompletionQueue*);
void            EXPORT_OnCoroutineWorkerStart(void);
} )xxx");

  FilePrinter oCodePrinter{strBaseDir + "/dylib.cpp"};
  oCodePrinter.Print("#include \"dylib_export.h\"", oArgument);
  oCodePrinter.Print("#include \"Proto/$package$.grpc.pb.h\"", oArgument);
  oCodePrinter.Print("#include \"$service$Impl.hpp\"", oArgument);
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

void EXPORT_DylibInit(const char * conf_file)
{
    $service$Impl::BeforeServerStart(conf_file);
}

grpc::Service * EXPORT_GetGrpcServiceInstance(void)
{
    return &service;
}
void EXPORT_OnCoroutineWorkerStart(void)
{
    $service$Impl::SetInstance(new $service$Impl);
    $service$Impl::GetInstance()->BeforeWorkerStart();
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
INCLUDE_DIRECTORIES(/usr/local/include)
INCLUDE_DIRECTORIES(.)
LINK_LIBRARIES(pthread protobuf gpr grpc++ grpc++_reflection)

SET(APP_SOURCES "./dylib.cpp")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./Proto/*.cc")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./Handler/*.cpp")
FILE(GLOB APP_SOURCES ${APP_SOURCES} "./*.cpp")
INCLUDE_DIRECTORIES(".")
ADD_LIBRARY(|package| SHARED ${APP_SOURCES})
TARGET_LINK_LIBRARIES(|package| coredeps)
)xxx",
                     oArgument, '|');
}
void MakeProto(const std::string &strBaseDir, const std::string &strProtoPath)
{

  system(("protoc --cpp_out=" + strBaseDir + " " + strProtoPath + ";").c_str());
  system(("protoc --grpc_out=" + strBaseDir + " --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` " + strProtoPath).c_str());
}
void MakeGitSubModule(const std::string &strBaseDir)
{
  FilePrinter oCodePrinter{strBaseDir + "/.gitmodules"};
  oCodePrinter.Print(R"xxx([submodule "CoreDeps"]
	path = CoreDeps
	url = https://github.com/BachelorDegree/CoreDeps)xxx");
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
  MakeImpl(pFileDesc->package(), pFileDesc);
  MakeProto(pFileDesc->package() + "/Proto", argv[1]);
  //MakeGitSubModule(pFileDesc->package());
}