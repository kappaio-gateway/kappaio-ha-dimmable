# REST API For ZigBee Dimmable Lights

##Native Compile
*** In progress ***

##Cross Compile (compile Pi's code using PC)
1. Download the cross compile toolchain to your Linux PC
```
git clone https://github.com/kappaIO-Dev/kappaIO-toolchain-crosscompile-armhf.git
```
2. Download the code to `package/` folder:
```
cd kappaIO-toolchain-crosscompile-armhf/package
git clone https://github.com/kappaIO-Dev/kappaio-ha-dimmable.git
```
3. Complie, install
```
cd  kappaIO-toolchain-crosscompile-armhf/package/kappaio-ha-dimmable/build
./build root@192.168.1.15
```
Substitute the IP with your Pi's LAN IP

