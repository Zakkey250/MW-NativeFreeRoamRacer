#pragma once
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
// CPU-only preparation from this installation; no edits or external tools.
namespace skin_resources {
using Bytes=std::vector<unsigned char>;
inline unsigned Word(const Bytes& b,size_t at){if(at>b.size()||b.size()-at<4)throw std::runtime_error("truncated skin resource");unsigned v;memcpy(&v,b.data()+at,4);return v;}
inline void Put(Bytes& b,size_t at,unsigned v){if(at>b.size()||b.size()-at<4)throw std::runtime_error("skin field overflow");memcpy(b.data()+at,&v,4);}
inline unsigned Hash(const char* s){unsigned h=~0u;while(*s)h=h*33+static_cast<unsigned char>(*s++);return h;}
inline std::string Name(const char* family,unsigned slot){return std::string("LB_")+family+std::to_string(slot);}
inline void Append(Bytes& b,unsigned tag,const Bytes& data){if(data.size()>64*1024*1024)throw std::runtime_error("skin chunk limit");auto at=b.size();b.resize(at+8);Put(b,at,tag);Put(b,at+4,unsigned(data.size()));b.insert(b.end(),data.begin(),data.end());}
struct Field {unsigned tag;size_t at,end;};
inline std::vector<Field> Fields(const Bytes& b,size_t at,size_t end){std::vector<Field> out;if(end>b.size())throw std::runtime_error("skin range");while(at<end){auto tag=Word(b,at),n=Word(b,at+4);if(end-at<8||n>end-at-8)throw std::runtime_error("skin chunk extent");out.push_back({tag,at+8,at+8+n});at+=8+n;}return out;}
inline Bytes Slice(const Bytes& b,size_t at,size_t n){if(at>b.size()||n>b.size()-at)throw std::runtime_error("skin slice");return Bytes(b.begin()+at,b.begin()+at+n);}
inline Bytes Decode(const Bytes& b){
 if(b.size()<16)throw std::runtime_error("skin compression header");auto size=Word(b,8);if(size<156||size>20*1024*1024)throw std::runtime_error("skin decode size");
 if(!memcmp(b.data(),"RAWW",4))return Slice(b,16,size);
 if(memcmp(b.data(),"JDLZ",4))throw std::runtime_error("unsupported skin compression");auto limit=Word(b,12);if(limit<18||limit>b.size())throw std::runtime_error("skin decode extent");
 Bytes out;out.reserve(size);size_t at=16;unsigned control=1,mode=1;auto byte=[&](){if(at>=limit)throw std::runtime_error("skin decode input");return unsigned(b[at++]);};
 while(out.size()<size){if(control==1)control=byte()|256;if(mode==1)mode=byte()|256;
  if(control&1){unsigned a=byte(),c=byte(),distance,count;if(mode&1){distance=(a&15)+1;count=((a>>4)<<8|c)+3;}else{distance=((a>>5)<<8|c)+17;count=(a&31)+3;}
   if(distance>out.size()||count>size-out.size())throw std::runtime_error("skin decode backref");while(count--)out.push_back(out[out.size()-distance]);mode>>=1;
  }else out.push_back(static_cast<unsigned char>(byte()));control>>=1;
 }return out;
}
struct Texture {Bytes descriptor,format,pixels;};
inline void Validate(const Texture& t){auto& d=t.descriptor;if(d.size()!=124||t.format.size()!=32)throw std::runtime_error("skin metadata");unsigned short width,height;memcpy(&width,d.data()+68,2);memcpy(&height,d.data()+70,2);
 if(width!=height||width<128||width>2048||(width&(width-1))||d[74]!=32||Word(d,56)!=unsigned(width)*height*4||Word(d,60)!=0||t.pixels.size()!=Word(d,56))throw std::runtime_error("unsupported writable skin template");}
inline Bytes Build(const Bytes& source){
 Bytes info,table,descriptors,formats,pixels;auto top=Fields(source,0,source.size());if(top.size()!=1||top[0].tag!=0xB3300000)throw std::runtime_error("skin pack type");
 for(auto block:Fields(source,top[0].at,top[0].end)){
  if(block.tag==0xB3310000)for(auto f:Fields(source,block.at,block.end)){
   auto data=Slice(source,f.at,f.end-f.at);switch(f.tag){case 0x33310001:info=data;break;case 0x33310003:table=data;break;case 0x33310004:descriptors=data;break;case 0x33310005:formats=data;break;}}
  if(block.tag==0xB3320000)for(auto f:Fields(source,block.at,block.end))if(f.tag==0x33320002){auto at=(f.at+127)&~size_t(127);pixels=Slice(source,at,f.end-at);}
 }
 if(info.size()!=124)throw std::runtime_error("skin pack header");
 auto extract=[&](const char* name){Texture t;auto key=Hash(name);
  if(!table.empty()){if(table.size()%24)throw std::runtime_error("skin index");for(size_t at=0;at<table.size();at+=24)if(Word(table,at)==key){auto raw=Decode(Slice(source,Word(table,at+4),Word(table,at+8)));if(raw.size()!=Word(table,at+12))throw std::runtime_error("skin raw size");t.descriptor=Slice(raw,raw.size()-156,124);t.format=Slice(raw,raw.size()-32,32);t.pixels=Slice(raw,0,raw.size()-156);break;}}
  else{if(descriptors.size()%124||formats.size()!=descriptors.size()/124*32)throw std::runtime_error("skin resident index");for(size_t at=0;at<descriptors.size();at+=124)if(Word(descriptors,at+36)==key){t.descriptor=Slice(descriptors,at,124);t.format=Slice(formats,at/124*32,32);t.pixels=Slice(pixels,Word(t.descriptor,48),Word(t.descriptor,56));break;}}
  Validate(t);return t;
 };
 auto skin=extract("DUMMY_SKIN1"),wheel=extract("DUMMY_WHEEL1");
 struct Entry {unsigned hash;Texture texture;};std::vector<Entry> entries;
 for(unsigned slot=13;slot<=20;++slot)for(auto family:{"SKIN","WHEEL","SPINNER"}){
  auto t=strcmp(family,"SKIN")==0?skin:wheel;auto name=Name(family,slot);auto key=Hash(name.c_str());memset(t.descriptor.data(),0,36);memcpy(t.descriptor.data()+12,name.c_str(),name.size());Put(t.descriptor,36,key);Put(t.descriptor,44,0);Put(t.descriptor,52,~0u);t.descriptor[78]=1;Put(t.format,20,21);entries.push_back({key,std::move(t)});
 }
 std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.hash<b.hash;});Bytes keys,desc,dd,data;
 for(auto& e:entries){auto at=keys.size();keys.resize(at+8);Put(keys,at,e.hash);Put(e.texture.descriptor,48,unsigned(data.size()));desc.insert(desc.end(),e.texture.descriptor.begin(),e.texture.descriptor.end());dd.insert(dd.end(),e.texture.format.begin(),e.texture.format.end());data.insert(data.end(),e.texture.pixels.begin(),e.texture.pixels.end());}
 const char* pack="LB_PRIVATE_SKINS";auto packHash=Hash(pack);memset(info.data()+4,0,120);memcpy(info.data()+4,pack,strlen(pack));memcpy(info.data()+32,pack,strlen(pack));Put(info,96,packHash);
 Bytes head;Append(head,0x33310001,info);Append(head,0x33310002,keys);Append(head,0x33310004,desc);Append(head,0x33310005,dd);
 Bytes body;Append(body,0,Bytes(48));Append(body,0xB3310000,head);auto gap=(128-(8+body.size())%128)%128;if(gap){if(gap<8)gap+=128;Append(body,0,Bytes(gap-8));}
 Bytes vramHeader(24);Put(vramHeader,8,1);Put(vramHeader,12,packHash);Bytes vram;Append(vram,0x33320001,vramHeader);auto base=8+body.size()+8;gap=(128-(base+vram.size())%128)%128;if(gap<8)gap+=128;Append(vram,0,Bytes(gap-8));Bytes aligned((128-(base+vram.size()+8)%128)%128);aligned.insert(aligned.end(),data.begin(),data.end());Append(vram,0x33320002,aligned);Append(body,0xB3320000,vram);Bytes bundle;Append(bundle,0xB3300000,body);return bundle;
}
inline Bytes Read(const wchar_t* path){std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("cannot read current CARS textures");auto n=file.tellg();if(n<=0||n>256*1024*1024)throw std::runtime_error("CARS texture size");Bytes b(static_cast<size_t>(n));file.seekg(0);file.read(reinterpret_cast<char*>(b.data()),n);if(!file)throw std::runtime_error("CARS texture read");return b;}
}
