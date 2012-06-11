#!/bin/sh

#update version info
cur_rev_num=`git rev-list HEAD | wc -l | awk '{print $1}'`
base_rev_num=`git rev-list 3.0.0.4 | wc -l | awk '{print $1}'`
((rev_num=$cur_rev_num-$base_rev_num+4))

rev_sha1=`git rev-parse HEAD`
rev_tag=`git describe --tag --abbrev=0`
ver_major=`echo $rev_tag | awk -F$'.' '{print $1}'`
ver_minor=`echo $rev_tag | awk -F$'.' '{print $2}'`
ver_patch=`echo $rev_tag | awk -F$'.' '{print $3}'`

echo "#define XY_VSFILTER_VERSION_MAJOR $ver_major
#define XY_VSFILTER_VERSION_MINOR  $ver_minor
#define XY_VSFILTER_VERSION_PATCH  $ver_patch
#define XY_VSFILTER_VERSION_COMMIT $rev_num
#define XY_VSFILTER_VERSION_COMMIT_SHA1 \"$rev_sha1\"" > src/filters/transform/vsfilter/version_in.h

#build
echo '
CALL "%VS100COMNTOOLS%../../VC/vcvarsall.bat" x86
devenv src/filters/transform/vsfilter/VSFilter_vs2010.sln /build "Release Unicode"
exit
' | cmd
