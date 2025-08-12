using cxx
using test

cxx.std = 17

# Build the main executable
exe{kbdlayoutmon}: \
  source/kbdlayoutmon.cpp \
  source/configuration.cpp \
  source/log.cpp \
  resources/res-icon.rc \
  resources/res-versioninfo.rc \
  manifest.xml

# Build the hook DLL
lib{kbdlayoutmonhook}: \
  source/kbdlayoutmonhook.cpp \
  source/configuration.cpp

# Unit tests
exe{run_tests}: \
  tests/test_configuration.cpp \
  tests/test_log.cpp \
  tests/test_utils.cpp \
  source/log.cpp \
  source/configuration.cpp

# Register test target
test{run_tests}

# Include paths
cxx.poptions += -Isource -Iresources -Itests

# Link with Windows libraries
winlibs = lib{shlwapi} lib{user32} lib{gdi32} lib{ole32} lib{advapi32}
exe{kbdlayoutmon}: libs += $winlibs lib{shell32} lib{version}
lib{kbdlayoutmonhook}: libs += $winlibs
