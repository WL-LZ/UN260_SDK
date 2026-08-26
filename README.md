# D213-Devkitf-luban

## 1.介绍

​	本仓库主要存放百问网科技有限公司基于匠芯创D213芯片的D213-Devkitf开发板的板级补丁包。

## 2.使用指南

​	1.获取匠芯创官方对外SDK包，在Ubuntu20.04终端下输入以下命令：

```shell
git clone https://gitee.com/artinchip/d211.git
```

​	2.获取D213-Devkitf开发板的板级补丁包，输入以下命令：

```shell
git clone https://gitee.com/weidongshan/d213-devkitf-luban.git
```

​	获取完成后有以下文件夹

```shell
ubuntu@ubuntu2004:$ ls
d211  d213-devkitf-luban
```

​	3.将D213-Devkitf开发板的板级补丁包中的所有文件拷贝到官方SDK包中，输入以下命令：

```shell
cp -rf d213-devkitf-luban/* d211/
```

​	4.拷贝完成后进入官方SDK包目录中：

```shell
cd d211/
```

​	5.激活开发环境：

```
source tools/onestep.sh
```

​	6.选择D213-Devkitf开发板方案：

```shell
lunch d211_d213_devkitf_defconfig
```

​	7.编译

```
make
```

