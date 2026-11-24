# esp-sr movemodel.py prints Unicode box-drawing on Windows; cp1252 stdout crashes the build.
set(_movemodel "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp-sr/model/movemodel.py")
if(NOT EXISTS "${_movemodel}")
    return()
endif()

file(READ "${_movemodel}" _mm)
set(_changed FALSE)

if(_mm MATCHES "print\\(u'─'")
    string(REPLACE "print(u'─' * 40)" "print(u'-' * 40)" _mm "${_mm}")
    set(_changed TRUE)
endif()

set(_win_utf8_hook [[

if sys.platform == 'win32':
    for _stream in (sys.stdout, sys.stderr):
        if hasattr(_stream, 'reconfigure'):
            try:
                _stream.reconfigure(encoding='utf-8', errors='replace')
            except (OSError, ValueError):
                pass
]])
if(NOT _mm MATCHES "sys.platform == 'win32':")
    string(REPLACE "import sys\n\nsys.dont_write_bytecode" "import sys${_win_utf8_hook}\nsys.dont_write_bytecode" _mm "${_mm}")
    set(_changed TRUE)
endif()

if(_changed)
    file(WRITE "${_movemodel}" "${_mm}")
    message(STATUS "Patched esp-sr movemodel.py for Windows console encoding")
endif()
