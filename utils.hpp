#pragma once
#include <string>
#include <google/protobuf/compiler/importer.h>
class PBHelper
{
  public:
  static std::string ClassName(const google::protobuf::Descriptor *descriptor)
  {
    using namespace google::protobuf;
    const Descriptor *parent = descriptor->containing_type();
    std::string res;
    if (parent)
      res += ClassName(parent) + "_";
    res += descriptor->name();
    return res;
  }
  static std::string QualifiedClassName(const google::protobuf::Descriptor *d)
  {
    auto pFile = d->file();
    std::string name = ClassName(d);
    if (pFile->package().empty())
    {
      return "::" + name;
    }
    return "::" + pFile->package() + "::" + name;
  }
    static std::string QualifiedServiceName(const google::protobuf::ServiceDescriptor *d)
  {
    auto pFile = d->file();
    std::string name = d->name();
    if (pFile->package().empty())
    {
      return "::" + name;
    }
    return "::" + pFile->package() + "::" + name;
  }
};
