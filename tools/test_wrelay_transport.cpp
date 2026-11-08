#include <cassert>
#include <cstdio>
#include <string>
struct String:std::string {
    using std::string::string;
    String(std::string value):std::string(value){}
    bool startsWith(const char *prefix)const{return rfind(prefix,0)==0;}
};
static struct {bool debug_mode=false;String vanpi_ip="127.0.0.1";} state;
constexpr int WL_CONNECTED=3;
static struct {int value=WL_CONNECTED;int status()const{return value;}} WiFi;
static struct {template<class...T>void printf(const char*,T...) {}} Serial;
static String get_url(){return "http://127.0.0.1:1880";}
static struct {String url,type,body;int calls=0,timeout=0,connect=0;} sent;
struct HTTPClient {
    void begin(String s){sent.url=s;}
    void setConnectTimeout(int t){sent.connect=t;}
    void setTimeout(int t){sent.timeout=t;}
    void addHeader(const char *name,const char *value){assert(std::string(name)=="Content-Type");sent.type=value;}
    int PUT(const char *body){sent.body=body;++sent.calls;return 200;}
    void end(){}
};
#include "wrelay_transport_under_test.h"
int main(){
    for(int index=1;index<=8;++index)for(const char *value:{"true","false"}){
        String route="/wrelay/"+std::to_string(index)+"/"+value;
        const int before=sent.calls;http_put(route);
        assert(sent.calls==before+1&&sent.type=="text/plain; charset=utf-8"&&sent.body=="{}");
        assert(sent.timeout==1500&&sent.connect==400);
        std::printf("{\"path\":\"%s\",\"contentType\":\"%s\",\"body\":\"%s\"}\n",route.c_str(),sent.type.c_str(),sent.body.c_str());
    }
    for(const char *route:{"/relay/1/false","/dimmer/1/20","/heater/false","/autoterm/temp/21","/maxxfan/false","/wrelayiot/1/false","/wrelay-other/1/false"}){
        http_put(route);assert(sent.type=="application/json"&&sent.body=="{}"&&sent.timeout==1500&&sent.connect==400);
    }
    const int before=sent.calls;
    state.debug_mode=true;http_put("/wrelay/1/false");assert(sent.calls==before);
    state.debug_mode=false;WiFi.value=0;http_put("/wrelay/1/false");assert(sent.calls==before);
    WiFi.value=WL_CONNECTED;state.vanpi_ip="";http_put("/wrelay/1/false");assert(sent.calls==before);
}
