#!/bin/sh

function Usage()
{
  echo "Usage:"
  echo -e "\t$1 [-conf "'"Release"|"Debug"'"]  [-plat platform "'"Win32"|"x64"'"] [-action build|clean|rebuild] [-proj projects] [-voff|--versioning-off] [-solution sln_file] [-compiler VS2010|VS2012|VS2013|VS2019]"
  echo -e "\nDefault:"
  echo -e '-conf\t\t"Release"'
  echo -e '-plat\t\t"Win32"'
  echo -e '-action\t\tbuild'
  echo -e '-proj\t\t"vsfilter xy_sub_filter"'
  echo -e '-solution\tVSFilter.sln'
  echo -e '-compiler\tVS2010'
  echo -e "\nVisual Studio 2012:"
  echo -e '-compiler\tVS2012'
  echo -e "\nVisual Studio 2013:"
  echo -e '-compiler\tVS2013'
  echo -e "\nVisual Studio 2019:"
  echo -e '-compiler\tVS2019'
}

script_dir=`dirname $0`
cd $script_dir

solution="VSFilter.sln"
action="build"
configuration="Release"
platform="Win32"
projects="vsfilter xy_sub_filter"
compiler="VS2010"
common_tools="%VS100COMNTOOLS%"
update_version=1

while [ "$1"x != ""x ]
do
  if [ "$flag"x == ""x ]; then
    if [ "$1"x == "-configuration"x ] || [ "$1"x == "-conf"x ]; then
      flag="configuration"
    elif [ "$1"x == "-platform"x ] || [ "$1"x == "-plat"x ]; then
      flag="platform"
    elif [ "$1"x == "-action"x ]; then
      flag="action"
    elif [ "$1"x == "-projects"x ] || [ "$1"x == "-proj"x ]; then
      flag="projects"
    elif [ "$1"x == "-solution"x ]; then
      flag="solution"
    elif [ "$1"x == "-compiler"x ]; then
      flag="compiler"
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

# normalize some arguments
action=`echo $action | awk '{ print tolower($0) }'`
compiler=`echo $compiler | awk '{ print tolower($0) }'`
platform=`echo $platform | awk '{ print tolower($0) }'`

platform_type="x86"
if [ "$platform"x == "x64"x ]; then
  platform_type="x86_amd64"
fi

if [ "$compiler"x == "vs2010"x ]; then
  configuration=$configuration"|"$platform
elif [ "$compiler"x == "vs2012"x ]; then
  common_tools="%VS110COMNTOOLS%"
elif [ "$compiler"x == "vs2013"x ]; then
  common_tools="%VS120COMNTOOLS%"
elif [ "$compiler"x == "vs2019"x ]; then
  common_tools="%VS160COMNTOOLS%"
else
  echo "Invalid compiler argument: $compiler"
  Usage $0
  exit -1
fi

if [ "$compiler"x != "vs2010"x ]; then
  if [ "$action"x == "build"x ]; then
    action=""
  else
    action=":"$action
  fi
fi

#build
for project in $projects
do

if [ "$compiler"x == "vs2010"x ]; then
echo '
CALL "'$common_tools'../../VC/vcvarsall.bat" '$platform_type'
devenv "'$solution'" /'$action' "'$configuration'" /project "'$project'"
' | cmd

elif [ "$compiler"x == "vs2019"x ]; then
echo '
CALL "'$common_tools'../../VC/Auxiliary/Build/vcvarsall.bat" '$platform_type'
msbuild /m /t:'$project''$action' /p:Configuration='$configuration' /p:Platform='$platform' /p:BuildProjectReferences=false "'$solution'"
exit
' | cmd

else

echo '
CALL "'$common_tools'../../VC/vcvarsall.bat" '$platform_type'
msbuild /m /t:'$project''$action' /p:Configuration='$configuration' /p:Platform='$platform' /p:BuildProjectReferences=false "'$solution'"
exit
' | cmd

fi
done
