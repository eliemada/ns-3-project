NS-3 HTTP Cache Simulation Module (ALL FILES)

Contents:
- model/http-header.{h,cc}
- model/http-client-app.{h,cc}
- model/http-cache-app.{h,cc}
- model/http-origin-app.{h,cc}
- examples/http-cache-scenario.cc
- CMakeLists.txt for module and examples

Install fresh:
  rm -rf ~/Documents/ns-3-dev/src/http-cache
  unzip ns3-http-cache-all.zip -d ~/Documents/ns-3-dev/src/
  cd ~/Documents/ns-3-dev
  rm -rf cmake-cache build
  ./ns3 configure --enable-examples -- -G Ninja
  ./ns3 build
Run:
  ./ns3 run http-cache-scenario -- --nReq=100 --interval=0.2 --ttl=3 --cacheCap=5 --numContent=10 --zipf=true --zipfS=1.0 --originDelay=5 --csv=metrics.csv
