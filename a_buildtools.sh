# E.g.  a_buildtools.sh "Debug" "testtools" ["clean"]
# Setting Z3
BUILD_TYPE=$1
TOOLSDIR=$2
CLEAN=$3


#export Z3_ROOT=~/$TOOLSDIR/river/z3
#sudo source ~/.bashrc
sudo ./a_exportvariables.sh $TOOLSDIR
printf "Variable Z3_ROOT is "
printenv Z3_ROOT
printf "\n"

# Optional cmake flags depending on build
CMAKE_OPTIONAL_FLAGS=""

if [ "$BUILD_TYPE" = "Debug"  ]
then
  printf "### THIS IS A DEBUG BUILD\n"
  #TOOLSDIR="testtools_debug"

  CMAKE_OPTIONAL_FLAGS="${CMAKE_OPTIONAL_FLAGS} -DCMAKE_BUILD_TYPE=Debug"
  printf "Flag to cmake is ${CMAKE_OPTIONAL_FLAGS}\n"
else
  BUILD_TYPE = "Release"
  printf "### THIS IS A RELEASE BUILD"
  #TOOLSDIR="testtools"
fi


BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Do we need to clean everything ?
#----------------------------------------------------
if [ "$CLEAN" = "clean"  ]; then
cd river 
sudo ./clean-all.sh
cd -
fi

printf "${BLUE}Step 3: Setting Z3: ${NC}"
sudo ./a_solveZ3Links.sh $BUILD_TYPE $TOOLSDIR


printf "${BLUE}Step 4: Build and install River\n${NC}"
cd ~/$TOOLSDIR/river/
rm -f river.format # delete the symbolic link if there is already there from github..
ln -s ~/$TOOLSDIR/river.format ./  # Note that river.format is shared between river and simple/distributed tracer. This command will create a symbolic link such that river thinks that river.format is a subfolder

cmake ${CMAKE_OPTIONAL_FLAGS} CMakeLists.txt
make
sudo make install

printf "${BLUE}Step 5: Build and installing simpletracer ${NC}"
cd ~/$TOOLSDIR/simpletracer/
rm -f river.format # remove the symbolic link if it came from github..
ln -s ../river.format/ ./

cmake ${CMAKE_OPTIONAL_FLAGS} CMakeLists.txt
make
sudo make install
