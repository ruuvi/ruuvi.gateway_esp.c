IDF_VERSION_EXPECTED=`cat CMakeLists.txt | sed -r -e '/^set\(EXPECTED_IDF_VERSION\s+(v.*)\)$/!d' -e 's/^set\(EXPECTED_IDF_VERSION\s+(v.*)\)$/\1/g'`
test ${?} -eq 0 || exit 1
IDF_VERSION_ACTUAL=`(cd $IDF_PATH && git describe --always --tags --dirty --match v4.2.2*)`
test ${?} -eq 0 || exit 1

if [ "$IDF_VERSION_EXPECTED" != "$IDF_VERSION_ACTUAL" ]; then
  echo -e "\033[31mIDF version \"$IDF_VERSION_ACTUAL\" does not match with the expected version \"$IDF_VERSION_EXPECTED\"\033[30m" 1>&2
  exit 1
fi

FW_TAG=`git describe --always --tags --dirty`
test ${?} -eq 0 || exit 1
FW_SUFFIX=`echo $FW_TAG | sed -r 's/v([1-9]+\.[1-9]+\.[1-9]+)(.*)/\2/g'`
test ${?} -eq 0 || exit 1
if [ "$FW_SUFFIX" != "" ]; then
  echo -e "\033[31mCurrent version is not a release version: $FW_TAG\033[30m" 1>&2
  exit 1
fi

idf.py fullclean
test ${?} -eq 0 || exit 1

idf.py build
test ${?} -eq 0 || exit 1

touch CMakeLists.txt
test ${?} -eq 0 || exit 1

idf.py build
test ${?} -eq 0 || exit 1
