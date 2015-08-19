#!/bin/bash

set -e

make_control()
{
	local package=$1
	local arch=$2
	local version=$3
	local depends=$4

	cat <<EOF
Source: canopen2
Section: pluto
Essential: no
Priority: optional
Package: $package
Architecture: $arch
Version: $version
Maintainer: Pluto Team <PlutoTeamCallisto@marel.com>
Depends: $depends
Description: CANopen master version 2.
EOF
}

make_target_control()
{
	local arch=$1
	local version=$2

	make_control canopen2 $arch $version "libmloop, appbase"
}

make_host_control()
{
	local version=$1
	make_control canopen2-crossdevelopment i386 $version \
		"libmloop-crossdevelopment, appbase-crossdevelopment"
}

build_package()
{
	local arch=$1
	local path=$2
	local name=$3
	local version=$4

	mkdir -p $path/DEBIAN
	if [ "$arch" == "host" ]; then
		arch=i386
		make_host_control $version >$path/DEBIAN/control
	else
		make_target_control $arch $version >$path/DEBIAN/control
	fi

	fakeroot dpkg-deb -b -Zgzip $path $name\_$version\_$arch\.deb

	rm -rf $path/DEBIAN
}

build_target_release()
{
	local arch=$1

	local path=build-$arch-release
	local rootfs=build-host$(marel_getrootprefix $arch)

	USE_GCC5_1=compat marel_make.sh $arch -j8 RELEASE=yes
	make install RELEASE=yes PREFIX=/usr DESTDIR=$path
	make install RELEASE=yes PREFIX=/usr DESTDIR=$rootfs
	make clean
}

build_target_debug()
{
	local arch=$1

	local path=build-$arch-debug
	local rootfs=build-host$(marel_getrootprefix $arch)

	USE_GCC5_1=compat marel_make.sh $arch -j8
	make install PREFIX=/usr DESTDIR=$path
	make install PREFIX=/usr DESTDIR=$rootfs
	make clean
}

build_target()
{
	local arch=$1

	build_target_release $arch && build_target_debug $arch
}

build_host_release()
{
	local path=build-host

	make -j8 RELEASE=yes
	make install RELEASE=yes PREFIX=/usr DESTDIR=$path
	make clean
}

build_host_debug()
{
	local path=build-host

	make -j8
	make install PREFIX=/usr DESTDIR=$path
	make clean
}

build_host()
{
	build_host_release && build_host_debug
}

build_all()
{
	for arch in $(marel_supportedtargets --deb); do
		build_target $arch
	done

	build_host
}

build_all_packages()
{
	local name=$1
	local version=$2

	for arch in $(marel_supportedtargets --deb); do
		build_package $arch build-$arch-release $name $version
		build_package $arch build-$arch-debug $name-dbg $version
	done

	build_package host build-host $name-crossdevelopment $version
}

get_version()
{
	echo $(git describe --tags --dirty)-pluto$(pluto_version)
}

make distclean
build_all
build_all_packages canopen2 $(get_version)

