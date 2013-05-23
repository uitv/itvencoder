
编译安装
========

编译需要安装如下工具及开发库
install gnome-common
install autoconf
install automake
install libtool

install gstreamer-devel
install gstreamer-plugins-base-devel

如果需要打包成rpm还需要安装：
install rpm-build

编译按如下步骤完成：

./autogen.sh

./configure (--help)

make

make install

还需安装相关的plugins:

cd plugins
./install.sh

配置运行
========

修改配置文件
配置文件在/etc/itvencoder/目录下。
需要修改的配置文件是channels.d目录下的通道配置文件，如何根据需求修改通道
配置文件，参照docs目录下的iTVEncoderConfig.ppt。

[Management](docs/management.md)
