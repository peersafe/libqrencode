#! /bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Colo

arm=0
rebuild=0
clean=0

function usage() {
    echo "usage:"
    echo " build.sh [option]"
    echo
    echo "options:"
    echo "  clean           clean all buiding files"
    echo "  --arm           creating executables on arm"
    echo "  --rebuild       rebuiding"
    exit 0
}

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    --arm)
    arm=1
    shift # past argument
    ;;
    --rebuild)
    rebuild=1
    shift # past argument
    ;;
    clean)
    clean=1
    shift # past argument
    ;;
    -h|--help)
    usage
    shift # past argument
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

function CLEAN() {
    rm -rf build 2>/dev/null
    rm -rf CMakeFiles 2>/dev/null
    rm CMakeCache.txt 2>/dev/null
    make clean 1>/dev/null 2>&1
}

if [ ${clean} -eq 1 ]; then
    CLEAN
    exit 0
fi

if [ ${rebuild} -eq 1 ]; then
    CLEAN 
fi

if [ ${arm} -eq 1 ]; then
    has_arm_gcc=`which arm-linux-gnueabihf-gcc|wc -l`
    if [ ${has_arm_gcc} -eq 0 ]; then
        sudo apt-get install gcc-arm-linux-gnueabihf
    fi

    has_arm_gcc=`which arm-linux-gnueabihf-gcc|wc -l`
    if [ ${has_arm_gcc} -eq 0 ]; then
        printf "${RED}arm-linux-gnueabihf-gcc dosen't exists.${NC}\n"
        exit 1
    fi

    # statically linked
    cmake -DCROSS_SWITCH=1 -DCMAKE_EXE_LINKER_FLAGS="-static" -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" .
else
    cmake -DCMAKE_EXE_LINKER_FLAGS="-static" -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" .
fi

make -j2