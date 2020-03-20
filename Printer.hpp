#pragma once
#include <string>
#include <map>
#include <iostream>
#include <cstdio>
typedef std::map<std::string, std::string> PrinterArgument;
class Printer {
public:
  void Print(const std::string & strTemplate, const PrinterArgument &oArgument, const char delimiter = '$'){
    int iState = 0;
    size_t pos = 0;
    std::string strResult;
    std::string strArgName;
    while(pos < strTemplate.length()){
      switch(iState){
        case 0:
          if(strTemplate[pos] == delimiter){
            iState = 1;
            strArgName.clear();
          }else{
            strResult.push_back(strTemplate[pos]);
          }
          break;
        case 1:
          if(strTemplate[pos] == delimiter){
            iState = 0;
            if(oArgument.count(strArgName) > 0){
              strResult.append(oArgument.at(strArgName));
            }
          }else{
            strArgName.push_back(strTemplate[pos]);
          }
          break;
          
      }
      pos++;
    }
    this->Print(strResult);
  }
  void Print(const std::string & str){
    this->Write(MakeIndent(m_iDent));
    this->Write(str);
    this->Write("\n");
  }
  virtual void Write(const std::string & str) = 0;
  void Indent(){
    m_iDent+=4;
  }
  void Outdent(){
    m_iDent-=4;
  }
  static std::string MakeIndent(int iDent) {
    std::string str;
    str.reserve(iDent);
    for(int i=0;i<iDent;i++){
      str.push_back(' ');
    }
    return str;
  }
  virtual ~Printer(){
    
  }
protected:
  int m_iDent = 0;
};
class StringPrinter: public Printer{
  public:
  StringPrinter():Printer(){

  }
  virtual void Write(const std::string & str) override{
    m_str.append(str);
  }
  std::string GetResult() const{
    return m_str;
  }
  protected:
    std::string m_str;
};
class FilePrinter: public Printer{
  public:
  FilePrinter(const std::string & str):Printer(){
    m_pFile = fopen(str.c_str(), "w+");
  }
  virtual void Write(const std::string & str) override{
    fputs(str.c_str(), m_pFile);
  }
  std::string GetResult() const{
    return m_str;
  }
  ~FilePrinter(){
    fclose(m_pFile);
  }
  protected:
    std::string m_str;
  private:
    FILE * m_pFile;
};