#!/bin/sh

function Usage()
{
  echo "Usage:"
  echo -e "\t$1 [-conf "'"Release"|"Debug"'"] [-action build|clean|rebuild] [-proj project] [-voff|--versioning-off] [-solution sln_file]"
  echo "Default:"
  echo -e '-conf\t\t"Release"'
  echo -e '-action\t\tbuild'
  echo -e '-project\tvsfilter_2010'
  echo -e '-solution\tsrc/filters/transform/vsfilter/VSFilter_vs2010.sln'
}

script_dir=`dirname $0`
cd $script_dir

solution="src/filters/transform/vsfilter/VSFilter_vs2010.sln"
action="build"
configuration="Release"
project="vsfilter_2010"
update_version=1

while [ "$1"x != ""x ]
do
  if [ "$flag"x == ""x ]; then
    if [ "$1"x == "-conf"x ]; then
      flag="configuration"
    elif [ "$1"x == "-action"x ]; then
      flag="action"
    elif [ "$1"x == "-proj"x ]; then
      flag="project"
    elif [ "$1"x == "-solution"x ]; then
      flag="solution"
    elif [ "$1"x == "--versioning-off"x ] || [ "$1"x == "-voff"x ]; then
      update_version=0
      flag=""
    else
      echo "Invalid arguments"
      Usage $0
      exit -1
    fi
  else
    if [ "${1:0:1}"x == "-"x ]; then
      echo "Invalid arguments"
      Usage $0
      exit -1
    fi
    eval $flag='"'$1'"'
    flag=""
  fi
  shift
done

if [ "$flag"x != ""x ]; then
  echo "Invalid arguments"
  Usage $0
  exit -1
fi

if [ "$update_version"x == "1"x ]; then
echo "Updating version info"

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
fi

#build
echo '
CALL "%VS100COMNTOOLS%../../VC/vcvarsall.bat" x86
devenv "'$solution'" /'$action' "'$configuration'" /project "'$project'"
exit
' | cmd
